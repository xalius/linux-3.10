
/*
 ******************************************************************************
 *
 * sunxi_cci.c
 *
 * Hawkview ISP - sunxi_cci.c module
 *
 * Copyright (c) 2014 by Allwinnertech Co., Ltd.  http://www.allwinnertech.com
 *
 * Version		  Author         Date		    Description
 *
 *   2.0		  Yang Feng   	2014/06/23	      Second Version
 *
 ******************************************************************************
 */

#include <linux/platform_device.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>

#include "bsp_cci.h"
#include "sunxi_cci.h"
#include "../platform/platform_cfg.h"

#define CCI_MODULE_NAME "vin_cci"

#ifdef CCI_IRQ
static irqreturn_t cci_irq_handler(int this_irq, void *dev)
{
	unsigned long flags = 0;
	struct cci_dev *cci = (struct cci_dev *)dev;
	spin_lock_irqsave(&cci->slock, flags);
	bsp_cci_irq_process(cci->cci_sel);
	spin_unlock_irqrestore(&cci->slock, flags);
	return IRQ_HANDLED;
}
#endif

static int cci_probe(struct platform_device *pdev)
{
	struct device_node *np = pdev->dev.of_node;
	struct cci_dev *cci = NULL;
	struct cci_platform_data *pdata = NULL;
	int ret, irq = 0;

	if (np == NULL) {
		vin_err("CCI failed to get of node\n");
		return -ENODEV;
	}
	cci = kzalloc(sizeof(struct cci_dev), GFP_KERNEL);
	if (!cci) {
		ret = -ENOMEM;
		goto ekzalloc;
	}
	pdata = kzalloc(sizeof(struct cci_platform_data), GFP_KERNEL);
	if (pdata == NULL) {
		ret = -ENOMEM;
		goto freedev;
	}
	pdev->dev.platform_data = pdata;

	of_property_read_u32(np, "device_id", &pdev->id);
	if (pdev->id < 0) {
		vin_err("CCI failed to get device id\n");
		ret = -EINVAL;
		goto freepdata;
	}
	pdata->cci_sel = pdev->id;

	irq = irq_of_parse_and_map(np, 0);
	if (irq <= 0) {
		vin_err("[CCI%d] failed to get irq\n", pdev->id);
		ret = -EINVAL;
		goto freepdata;
	}
	cci->base = of_iomap(np, 0);
	if (!cci->base) {
		ret = -EIO;
		goto freepdata;
	}
	cci->irq = irq;
	cci->cci_sel = pdata->cci_sel;
	spin_lock_init(&cci->slock);
	init_waitqueue_head(&cci->wait);

#ifdef CCI_IRQ
	ret = request_irq(irq, cci_irq_handler,
				IRQF_DISABLED, CCI_MODULE_NAME, cci);
	if (ret) {
		vin_err("[CCI%d] requeset irq failed!\n", cci->cci_sel);
		goto unmap;
	}
#endif
	ret = bsp_csi_cci_set_base_addr(0, (unsigned long)cci->base);
	if (ret < 0)
		goto freeirq;
	ret = bsp_csi_cci_set_base_addr(1, (unsigned long)cci->base);
	if (ret < 0)
		goto freeirq;
	platform_set_drvdata(pdev, cci);
	vin_print("cci probe end cci_sel = %d!\n", pdata->cci_sel);

	return 0;

freeirq:
#ifdef CCI_IRQ
	free_irq(irq, cci);
unmap:
#endif
	iounmap(cci->base);
freepdata:
	kfree(pdata);
freedev:
	kfree(cci);
ekzalloc:
	vin_print("cci probe err!\n");
	return ret;
}

static int cci_remove(struct platform_device *pdev)
{
	struct cci_dev *cci = platform_get_drvdata(pdev);
	platform_set_drvdata(pdev, NULL);
#ifdef CCI_IRQ
	free_irq(cci->irq, cci);
#endif
	if (cci->base)
		iounmap(cci->base);
	kfree(cci);
	kfree(pdev->dev.platform_data);
	return 0;
}

static const struct of_device_id sunxi_cci_match[] = {
	{.compatible = "allwinner,sunxi-csi_cci",},
	{},
};

MODULE_DEVICE_TABLE(of, sunxi_cci_match);

static struct platform_driver cci_platform_driver = {
	.probe = cci_probe,
	.remove = cci_remove,
	.driver = {
		   .name = CCI_MODULE_NAME,
		   .owner = THIS_MODULE,
		   .of_match_table = sunxi_cci_match,
		   }
};

static int __init cci_init(void)
{
	int ret;
	ret = platform_driver_register(&cci_platform_driver);
	if (ret) {
		vin_err("platform driver register failed\n");
		return ret;
	}
	vin_print("cci_init end\n");
	return 0;
}

static void __exit cci_exit(void)
{
	platform_driver_unregister(&cci_platform_driver);
	vin_print("cci_exit end\n");
}

module_init(cci_init);
module_exit(cci_exit);

MODULE_AUTHOR("yangfeng");
MODULE_LICENSE("Dual BSD/GPL");
MODULE_DESCRIPTION("Camera CCI DRIVER for sunxi");
