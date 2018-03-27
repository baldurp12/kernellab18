/*
 * Kernellab
 */
#include <linux/module.h>

#include <linux/kernel.h>	/* printk() */
#include <linux/slab.h>		/* kmalloc() */
#include <linux/fs.h>		/* file related operations */
#include <linux/types.h>        /* size_t */
#include <linux/errno.h>        /* error codes */
#include <linux/sched.h>        /* task_struct */
#include <linux/uaccess.h>      /* copy_to_user copy_from_user */
#include <linux/semaphore.h>    /* semaphore support */

#include <linux/kobject.h>      /* kobject support */
#include <linux/string.h>       /* similar to string.h */
#include <linux/sysfs.h>        /* sysfs support */
#include <linux/cdev.h>         /* character device support */
#include <linux/device.h>       /* device and class information */

#include "pidinfo.h"



struct kernellab_dev {
	int                     open_count;     /* number of times opened */
	struct semaphore        sem;            /* mutual exclusion semaphore */
	struct cdev             cdev;           /* Char device structure */

	/* Add a minor number to differantiate between devices */
	int						minor;
};

/* New global semaphore */
struct semaphore glob_sem;


struct kernellab_dev *kernellab_device;
static struct class *kl_class; /* Global variable for the device class */
static dev_t kl_dev; /* Global variable for the first device number */
static int nr_devs = 2; /* Number of devices */

static int current_count;
static int pid_count;
static int all_count;

/**
 * Sysfs operations
 */
static ssize_t kernellab_current_count(struct kobject *kobj,
				    struct kobj_attribute *attr, char *buf)
{

	/* Your code here */
	return sprintf(buf, "%d\n", current_count);

}

static struct kobj_attribute kernellab_current_count_attribute =
	__ATTR(current_count, 0440, kernellab_current_count, NULL);

static ssize_t kernellab_pid_count(struct kobject *kobj,
				    struct kobj_attribute *attr, char *buf)
{


	/* Your code here */


  	return 0;
}

static struct kobj_attribute kernellab_pid_count_attribute =
	__ATTR(pid_count, 0440, kernellab_pid_count, NULL);

static ssize_t kernellab_all_count(struct kobject *kobj,
				    struct kobj_attribute *attr, char *buf)
{


	/* Your code here */


  	return 0;
}

static struct kobj_attribute kernellab_all_count_attribute =
	__ATTR(all_count, 0440, kernellab_all_count, NULL);



/* Setup list of sysfs entries */
static struct attribute *attrs[] = {
	&kernellab_current_count_attribute.attr,
	&kernellab_pid_count_attribute.attr,
	&kernellab_all_count_attribute.attr,
	NULL,
};

static struct attribute_group attr_group = {
	.attrs = attrs,
};

static struct kobject *kernellab_kobj;


/**
 * Device file operations
 */
static int kernellab_open(struct inode *inode, struct file *filp)
{
	struct kernellab_dev *dev; /* device information */
	dev = container_of(inode->i_cdev, struct kernellab_dev, cdev);
	filp->private_data = dev; /* for other methods */

	
	/* Your code here */


	/* How to use the device semaphore */
	if (down_interruptible(&dev->sem))
		return -ERESTARTSYS;
	/* Critical section */
	up(&dev->sem);

	
	return 0;
}

static int kernellab_release(struct inode *inode, struct file *filp)
{
	struct kernellab_dev *dev = filp->private_data;

	
	/* Your code here */

	
	return 0;
}

static long kernellab_ioctl(struct file *filp, unsigned int cmd,
			    unsigned long arg)
{
	struct kernellab_dev *dev = filp->private_data;


	/* Your code here */


	return -ENOIOCTLCMD;
}

/* Write the pid of the __currently__ running process to a memory location specified by a user program (buf)*/

static ssize_t kernellab_read(struct file *filp, char __user *buf, size_t count,
			   loff_t *f_pos)
{
	struct kernellab_dev *dev = filp->private_data;
	printk("READ: open(%d)\n",dev->minor);

	/* Only want to write the pid of kernellab1 */
	if(dev->minor == 1)
	{
		/* Semaphore locked */
		if (down_interruptible(&glob_sem))
			return -ERESTARTSYS;
		
		/* If copy_to_user fails  */

		if(copy_to_user(buf, &current->pid, sizeof(pid_t)))
		{
			printk("Copy to user failed\n");
			return -EFAULT;
		}
		
		/* Semaphore open */
		up(&glob_sem);	
	}
	printk("Copied %d to buf\n", current->pid);
	/* If we are in device 2 we return EFAULT by default */
	return -EFAULT;
}

static ssize_t kernellab_write(struct file *filp, const char __user *buf,
			       size_t count, loff_t *f_pos)
{
	struct kernellab_dev *dev = filp->private_data;


	/* Your code here */

	
	return -EFAULT;
}

static struct file_operations kernellab_fops = {
	.read           = kernellab_read,
	.write          = kernellab_write,
	.open           = kernellab_open,
	.release        = kernellab_release,
	.unlocked_ioctl = kernellab_ioctl,
	.owner          = THIS_MODULE
};

static int __init setup_devices(void)
{
	int err;
	struct device *dev_ret;
	dev_t dev;
	sema_init(&glob_sem,1);

	for (int i = 0; i < nr_devs; i++) {
		dev = MKDEV(MAJOR(kl_dev), MINOR(kl_dev) + i);
		dev_ret = device_create(kl_class, NULL, dev, NULL,
					"kernellab%d", i + 1);
		if (IS_ERR(dev_ret)) {
			err = PTR_ERR(dev_ret);
			while (i--) {
				dev = MKDEV(MAJOR(kl_dev), MINOR(kl_dev) + i);
				device_destroy(kl_class, dev);
			}
			return err;
		}
	}
	for (int i = 0; i < nr_devs; i++) {
		dev = MKDEV(MAJOR(kl_dev), MINOR(kl_dev) + i);
		sema_init(&kernellab_device[i].sem, 1);
		cdev_init(&kernellab_device[i].cdev, &kernellab_fops);
		if ((err = cdev_add(&kernellab_device[i].cdev, dev, 1)) < 0)
			return err;
	}
	return 0;
}

static int __init kernellab_init(void)
{
	int err;

	if ((err = alloc_chrdev_region(&kl_dev, 0, nr_devs, "kernellab")) < 0)
		goto out1;
	if (IS_ERR(kl_class = class_create(THIS_MODULE, "sty16"))) {
		err = PTR_ERR(kl_class);
		goto out2;
	}
	if (!(kernellab_device = kmalloc(nr_devs * sizeof(struct kernellab_dev),
					 GFP_KERNEL))) {
		err = -ENOMEM;
		goto out2;
	}
	memset(kernellab_device, 0, nr_devs * sizeof(struct kernellab_dev));
	if ((err = setup_devices()) < 0)
		goto out3;
	
	/* Log to dmesg buffer that the module was injected */
	printk("kernellab: module INJECTED\n");

	/* Set the minor number for the device*/
	for(int i = 0; i < nr_devs; i++){
		kernellab_device[i].minor = i + 1;
	}

	/* Initialize the counters */
	/* 
	* Dont really need the semaphore here but since
	* They are global variables shared with others, better safe than sorry
	*
	*/
	

	/* Semaphore locked */
	if (down_interruptible(&glob_sem))
		return -ERESTARTSYS;
	/* Critical section */
	
	current_count = 0;
	pid_count = 0;
	all_count = 0;
	
	/* Semaphore open */
	up(&glob_sem);
	
	
	kernellab_kobj = kobject_create_and_add("kernellab", kernel_kobj);
	if (!kernellab_kobj) {
		err = -ENOMEM;
		goto out4;
	}
	if ((err = sysfs_create_group(kernellab_kobj, &attr_group)))
		goto out4;
	return 0;
out4:
	for (int i = 0; i < nr_devs; i++) {
		dev_t dev = MKDEV(MAJOR(kl_dev), MINOR(kl_dev) + i);
		device_destroy(kl_class, dev);
	}
out3:
	class_destroy(kl_class);
out2:
	unregister_chrdev_region(kl_dev, nr_devs);
out1:
	return err;
}

static void __exit kernellab_exit(void)

{
	for (int i = 0; i < nr_devs; i++) {
		dev_t dev = MKDEV(MAJOR(kl_dev), MINOR(kl_dev) + i);
		cdev_del(&kernellab_device[i].cdev);
		device_destroy(kl_class, dev);
	}
	class_destroy(kl_class);
	unregister_chrdev_region(kl_dev, nr_devs);
	kobject_put(kernellab_kobj);

	// Log to dmesg buffer that the module was unloaded
	printk("kernellab: module UNLOADED\n");

	
	kfree(kernellab_device);
}

module_init(kernellab_init);
module_exit(kernellab_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Baldur Mar Petursson <baldurp12@ru.is>");
