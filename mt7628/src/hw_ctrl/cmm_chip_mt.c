/*
 ***************************************************************************
 * Ralink Tech Inc.
 * 4F, No. 2 Technology	5th	Rd.
 * Science-based Industrial	Park
 * Hsin-chu, Taiwan, R.O.C.
 *
 * (c) Copyright 2002-2004, Ralink Technology, Inc.
 *
 * All rights reserved.	Ralink's source	code is	an unpublished work	and	the
 * use of a	copyright notice does not imply	otherwise. This	source code
 * contains	confidential trade secret material of Ralink Tech. Any attemp
 * or participation	in deciphering,	decoding, reverse engineering or in	any
 * way altering	the	source code	is stricitly prohibited, unless	the	prior
 * written consent of Ralink Technology, Inc. is obtained.
 ***************************************************************************

	Module Name:
	cmm_asic.c

	Abstract:
	Functions used to communicate with ASIC

	Revision History:
	Who			When			What
	--------	----------		----------------------------------------------
*/


#include "rt_config.h"


#ifdef CONFIG_AP_SUPPORT
static UCHAR check_point_num = 0;
static VOID DumpBcnQMessage(RTMP_ADAPTER *pAd, INT apidx)
{
	int j = 0;
	//BSS_STRUCT *pMbss;
	UINT32 tmp_value = 0, hif_br_start_base = 0x4540;
	CHAR tmp[5];

	if (RTMP_TEST_FLAG(pAd, fRTMP_ADAPTER_HALT_IN_PROGRESS)) {
		return;
	}
	//pMbss = &pAd->ApCfg.MBSSID[apidx];

	DBGPRINT(DBG_LVL_ERROR, ("hif cr dump:\n"));
	for (j = 0; j < 80; j++)
	{
		HW_IO_READ32(pAd, (hif_br_start_base + (j * 4)), &tmp_value);
		MTWF_LOG(DBG_CAT_ALL, DBG_SUBCAT_ALL, DBG_LVL_ERROR,("CR:0x%x=%x\t", (hif_br_start_base + (j * 4)), tmp_value));
		if ((j % 4) == 3)
			MTWF_LOG(DBG_CAT_ALL, DBG_SUBCAT_ALL, DBG_LVL_ERROR, ("\n"));
	}

	MTWF_LOG(DBG_CAT_ALL, DBG_SUBCAT_ALL, DBG_LVL_ERROR, ("\ncheck PSE Q:\n"));
	for (j = 0; j <= 8; j++) {
		sprintf(tmp,"%d",j);
		set_get_fid(pAd, tmp);
	}

	MTWF_LOG(DBG_CAT_ALL, DBG_SUBCAT_ALL, DBG_LVL_ERROR, ("check TX_START and SLOT_IDLE\n"));
	for (j = 0; j < 10; j++) {
		MAC_IO_READ32(pAd, ARB_BCNQCR0, &tmp_value);
		MTWF_LOG(DBG_CAT_ALL, DBG_SUBCAT_ALL, DBG_LVL_ERROR, ("ARB_BCNQCR0: %x\n", tmp_value));
	}
#ifdef DBG
	if (DBG_LVL_WARN <= RTDebugLevel) {
		Show_PSTable_Proc(pAd, NULL);
		ShowPseInfo(pAd, NULL);
	}
	if (DBG_LVL_ERROR <= RTDebugLevel) {
		show_trinfo_proc(pAd, NULL);
	}
#endif /*DBG*/
}


VOID APCheckBcnQHandler(RTMP_ADAPTER *pAd, INT apidx, BOOLEAN *is_pretbtt_int)
{
	UINT32 val=0, temp = 0, own_mac = 0;
	int j = 0;
	BSS_STRUCT *pMbss;
	//struct wifi_dev *wdev;

	UINT32   Lowpart, Highpart;
	UINT32   int_delta;

    if (RTMP_TEST_FLAG(pAd, fRTMP_ADAPTER_HALT_IN_PROGRESS))
        return;

	pMbss = &pAd->ApCfg.MBSSID[apidx];
	//wdev = &pMbss->wdev;

	if (pMbss->bcn_buf.bcn_state < BCN_TX_DMA_DONE) {
		if (apidx == 0) {
			check_point_num++;
            if (check_point_num > 3) {
                DBGPRINT(RT_DEBUG_ERROR, ("%s()=>BSS%d:BcnPkt not idle(%d) - %d!, \n",
                                __FUNCTION__, apidx, pMbss->bcn_buf.bcn_state, check_point_num));
            }
			if (check_point_num > 10) {
				DumpBcnQMessage(pAd, apidx);
				check_point_num = 0;
			}
            else if (check_point_num % 5 == 4) {
                if (MTK_REV_GTE(pAd, MT7628, MT7628E2)) {
                    dma_sch_reset(pAd, NULL);

                    MAC_IO_READ32(pAd, TMAC_DBGR0, &val);//check Debug CR.
                    if ((val & (TMAC_DBGR0_MAC2PHY_RX | TMAC_DBGR0_MAC2PHY_TX)) == TMAC_DBGR0_MAC2PHY_TX)
                        pAd->TxErrTimes++;
                    else if ((val & (TMAC_DBGR0_MAC2PHY_RX | TMAC_DBGR0_MAC2PHY_TX)) == 0)
                        pAd->TxRxErrTimes++;
                    else
                        pAd->TxRxErrButNotRecTimes++;

                    MAC_IO_READ32(pAd, WF_CFG_OFF_WOCCR, &val);
                    val = val | WF_CFG_OFF_WOCCR_TMAC_LOGIC_GC_DIS;
                    MAC_IO_WRITE32(pAd, WF_CFG_OFF_WOCCR, val);

                    MAC_IO_READ32(pAd, ARB_SCR, &val);//toggle TX_DISABLE
                    val = val | MT_ARB_SCR_TXDIS;
                    MAC_IO_WRITE32(pAd, ARB_SCR, val);
                    val = val & ~MT_ARB_SCR_TXDIS;
                    MAC_IO_WRITE32(pAd, ARB_SCR, val);

                    MAC_IO_READ32(pAd, WF_CFG_OFF_WOCCR, &val);
                    val = val & ~WF_CFG_OFF_WOCCR_TMAC_LOGIC_GC_DIS;
                    MAC_IO_WRITE32(pAd, WF_CFG_OFF_WOCCR, val);
                }
            }
		}
		return;
	} else if (apidx == 0) {
		check_point_num = 0;
	}

    //if (MTK_REV_GTE(pAd, MT7628, MT7628E2))
    //    return;//MT7628 E2, should could skip this operation.

	AsicGetTsfTime(pAd, &Highpart, &Lowpart);
	int_delta = Lowpart - pMbss->WriteBcnDoneTime[pMbss->timer_loop];
	if (int_delta < (pAd->CommonCfg.BeaconPeriod * 1024/* unit is usec */))
	{
		/* update beacon has been called more than once in 1 bcn period,
		it might be called other than HandlePreTBTT interrupt routine.*/
		*is_pretbtt_int = FALSE;
		return;
	}

	if (pMbss->bcn_not_idle_time % 10 == 9) {
		pMbss->bcn_not_idle_time = 0;

        if (MTK_REV_GTE(pAd, MT7628, MT7628E2)) {
            MAC_IO_READ32(pAd, TMAC_DBGR0, &val);//check Debug CR.
            if ((val & (TMAC_DBGR0_MAC2PHY_RX | TMAC_DBGR0_MAC2PHY_TX)) == TMAC_DBGR0_MAC2PHY_TX)
                pAd->TxErrTimes++;
            else if ((val & (TMAC_DBGR0_MAC2PHY_RX | TMAC_DBGR0_MAC2PHY_TX)) == 0)
                pAd->TxRxErrTimes++;
            else
                pAd->TxRxErrButNotRecTimes++;

            MAC_IO_READ32(pAd, WF_CFG_OFF_WOCCR, &val);
            val = val | WF_CFG_OFF_WOCCR_TMAC_LOGIC_GC_DIS;
            MAC_IO_WRITE32(pAd, WF_CFG_OFF_WOCCR, val);

            MAC_IO_READ32(pAd, ARB_SCR, &val);//toggle TX_DISABLE
            val = val | MT_ARB_SCR_TXDIS;
            MAC_IO_WRITE32(pAd, ARB_SCR, val);
            val = val & ~MT_ARB_SCR_TXDIS;
            MAC_IO_WRITE32(pAd, ARB_SCR, val);

            MAC_IO_READ32(pAd, WF_CFG_OFF_WOCCR, &val);
            val = val & ~WF_CFG_OFF_WOCCR_TMAC_LOGIC_GC_DIS;
            MAC_IO_WRITE32(pAd, WF_CFG_OFF_WOCCR, val);
        }

		if (apidx == 0)
			DumpBcnQMessage(pAd, apidx);

		*is_pretbtt_int = FALSE;
		return;
	}
	else if (pMbss->bcn_not_idle_time % 3 == 2) {
		pMbss->bcn_not_idle_time++;
		pMbss->bcn_recovery_num++;
		*is_pretbtt_int = TRUE;
	}
	else {
		pMbss->bcn_not_idle_time++;
		*is_pretbtt_int = FALSE;
        if (pMbss->bcn_not_idle_time > 2) {
            DBGPRINT(RT_DEBUG_ERROR, ("%s()=>BSS%d:BcnPkt not idle(%d) - %d!, \n",
                            __FUNCTION__, apidx, pMbss->bcn_buf.bcn_state, pMbss->bcn_not_idle_time));
        }
		return;
	}

	if (apidx > 0)
        val = val | (1 << (apidx+15));
	else
		val = 1;

	j = 0;
	/* Flush Beacon Queue */
    MAC_IO_WRITE32(pAd, ARB_BCNQCR1, val);
	while (1) {
        MAC_IO_READ32(pAd, ARB_BCNQCR1, &temp);//check bcn_flush cr status
		if (temp & val) {
			j++;
			OS_WAIT(1);
			if (j > 1000) {
				printk("%s, bcn_flush too long!, j = %x\n", __func__, j);
				break;
			}
		}
		else {
			break;
		}
	}

    val = (DMA_FQCR0_FQ_EN |
            DMA_FQCR0_FQ_MODE |
            DMA_FQCR0_FQ_DEST_QID(8) |
            DMA_FQCR0_FQ_DEST_PID(3) |
            DMA_FQCR0_FQ_TARG_QID(7));

	j = 0;
	temp = 0;
	if (apidx > 0)
        own_mac = 0x10 | apidx;
    else
        own_mac = 0;

    val = val | DMA_FQCR0_FQ_TARG_OM(own_mac);
    MAC_IO_WRITE32(pAd, DMA_FQCR0, val);//flush all stuck bcn by own_mac

	while (1) {
        MAC_IO_READ32(pAd, DMA_FQCR0, &temp);//check flush status
		if (temp & DMA_FQCR0_FQ_EN) {
			j++;
			OS_WAIT(1);
			if (j > 1000) {
				printk("%s, flush all stuck bcn too long!! j = %x\n", __func__, j);
				break;
			}
		}
		else {
			break;
		}
	}

	//check filter resilt
	HW_IO_READ32(pAd, DMA_FQCR1, &temp);
	MTWF_LOG(DBG_CAT_ALL, DBG_SUBCAT_ALL, DBG_LVL_OFF, ("flush result = %x\n", temp));
	MTWF_LOG(DBG_CAT_ALL, DBG_SUBCAT_ALL, DBG_LVL_OFF, ("check pse fid Q7:"));
	set_get_fid(pAd, "7");

	val = 0;
	if (apidx > 0)
        val = val | (1 << (apidx+15));
	else
		val = 1;

    MAC_IO_READ32(pAd, ARB_BCNQCR0, &temp);//re-enable bcn_start
	temp = temp | val;
    MAC_IO_WRITE32(pAd, ARB_BCNQCR0, temp);

	pMbss->bcn_buf.bcn_state = BCN_TX_IDLE;
}
#endif /* CONFIG_AP_SUPPORT */


#ifdef RTMP_MAC_PCI
VOID MTPciMlmeRadioOn(PRTMP_ADAPTER pAd)
{
#ifdef CONFIG_AP_SUPPORT
	INT32 IdBss, MaxNumBss = pAd->ApCfg.BssidNum;
#endif

	MCU_CTRL_INIT(pAd);
	RTMP_CLEAR_FLAG(pAd, fRTMP_ADAPTER_RADIO_OFF);
	RTMP_CLEAR_FLAG(pAd, fRTMP_ADAPTER_IDLE_RADIO_OFF);
	RTMP_SET_FLAG(pAd, fRTMP_ADAPTER_MCU_SEND_IN_BAND_CMD);


#ifdef RTMP_MAC_PCI
	/*  Enable Interrupt */
	RTMP_ASIC_INTERRUPT_ENABLE(pAd);
#endif /* RTMP_MAC_PCI */

	/*  Enable PDMA */
	MtAsicSetWPDMA(pAd, PDMA_TX_RX, 1);

	/*  Send radio on command and wait for ack */
	CmdRadioOnOffCtrl(pAd, WIFI_RADIO_ON);

	/* Send Led on command */

	/* Enable RX */
	MtAsicSetMacTxRx(pAd, ASIC_MAC_RX, TRUE);

	/*  Enable normal operation */
	RTMP_OS_NETDEV_START_QUEUE(pAd->net_dev);

#ifdef CONFIG_AP_SUPPORT
	IF_DEV_CONFIG_OPMODE_ON_AP(pAd)
	{
		if (MaxNumBss > MAX_MBSSID_NUM(pAd))
			MaxNumBss = MAX_MBSSID_NUM(pAd);

		/*  first IdBss must not be 0 (BSS0), must be 1 (BSS1) */
		for (IdBss = FIRST_MBSSID; IdBss < MAX_MBSSID_NUM(pAd); IdBss++)
		{
			if (pAd->ApCfg.MBSSID[IdBss].wdev.if_dev)
				RTMP_OS_NETDEV_START_QUEUE(pAd->ApCfg.MBSSID[IdBss].wdev.if_dev);
		}
	}
#endif

	RTMP_CLEAR_FLAG(pAd, fRTMP_ADAPTER_DISABLE_DEQUEUEPACKET);
}


VOID MTPciPollTxRxEmpty(RTMP_ADAPTER *pAd)
{
	UINT32 Loop, Value;
	UINT32 IdleTimes = 0;
	MTWF_LOG(DBG_CAT_ALL, DBG_SUBCAT_ALL, DBG_LVL_TRACE, ("%s\n", __FUNCTION__));

	RtmpOsMsDelay(100);

	/* Fix Rx Ring FULL lead DMA Busy, when DUT is in reset stage */
	RTMP_SET_FLAG(pAd, fRTMP_ADAPTER_POLL_IDLE);

	/* Poll Tx until empty */
	for (Loop = 0; Loop < 20000; Loop++)
	{
		if (RTMP_TEST_FLAG(pAd, fRTMP_ADAPTER_NIC_NOT_EXIST))
			return;

		HIF_IO_READ32(pAd, MT_WPDMA_GLO_CFG, &Value);

		if ((Value & TX_DMA_BUSY) == 0x00)
		{
			IdleTimes++;
			RtmpusecDelay(50);
		}

		if (IdleTimes > 5000)
			break;
	}

	if (Loop >= 20000)
	{
		MTWF_LOG(DBG_CAT_ALL, DBG_SUBCAT_ALL, DBG_LVL_OFF, ("%s: TX DMA Busy!! WPDMA_GLO_CFG_STRUC = %d\n",
										__FUNCTION__, Value));
	}

	IdleTimes = 0;

	/*  Poll Rx to empty */
	for (Loop = 0; Loop < 20000; Loop++)
	{
		if (RTMP_TEST_FLAG(pAd, fRTMP_ADAPTER_NIC_NOT_EXIST))
			return;
		HIF_IO_READ32(pAd, MT_WPDMA_GLO_CFG, &Value);

		if ((Value & RX_DMA_BUSY) == 0x00)
		{
			IdleTimes++;
			RtmpusecDelay(50);
		}

		if (IdleTimes > 5000)
			break;
	}

	if (Loop >= 20000)
	{
		MTWF_LOG(DBG_CAT_ALL, DBG_SUBCAT_ALL, DBG_LVL_OFF, ("%s: RX DMA Busy!! WPDMA_GLO_CFG_STRUC = %d\n",
										__FUNCTION__, Value));
	}

	/* Fix Rx Ring FULL lead DMA Busy, when DUT is in reset stage */
	RTMP_CLEAR_FLAG(pAd, fRTMP_ADAPTER_POLL_IDLE);
}


VOID MTPciMlmeRadioOff(PRTMP_ADAPTER pAd)
{
#ifdef CONFIG_AP_SUPPORT
	INT32 IdBss, MaxNumBss = pAd->ApCfg.BssidNum;
#endif /* CONFIG_AP_SUPPORT */

	/*  Stop send TX packets */
	RTMP_OS_NETDEV_STOP_QUEUE(pAd->net_dev);

#ifdef CONFIG_AP_SUPPORT
	IF_DEV_CONFIG_OPMODE_ON_AP(pAd)
	{
		if (MaxNumBss > MAX_MBSSID_NUM(pAd))
			MaxNumBss = MAX_MBSSID_NUM(pAd);

		/* first IdBss must not be 0 (BSS0), must be 1 (BSS1) */
		for (IdBss = FIRST_MBSSID; IdBss < MAX_MBSSID_NUM(pAd); IdBss++)
		{
			if (pAd->ApCfg.MBSSID[IdBss].wdev.if_dev)
				RTMP_OS_NETDEV_STOP_QUEUE(pAd->ApCfg.MBSSID[IdBss].wdev.if_dev);
		}
	}
#endif /* CONFIG_AP_SUPPORT */

	RTMP_SET_FLAG(pAd, fRTMP_ADAPTER_DISABLE_DEQUEUEPACKET);

	/*  Disable RX */
	MtAsicSetMacTxRx(pAd, ASIC_MAC_RX, FALSE);

	/*  Polling TX/RX path until packets empty */
	MTPciPollTxRxEmpty(pAd);

	/*  Send Led off command */


	/*  Disable PDMA */
	MtAsicSetWPDMA(pAd, PDMA_TX_RX, 0);

	RTMP_SET_FLAG(pAd, fRTMP_ADAPTER_RADIO_OFF);

	/*  Disable Interrupt */
	if (RTMP_TEST_FLAG(pAd, fRTMP_ADAPTER_INTERRUPT_ACTIVE))
	{
		RTMP_ASIC_INTERRUPT_DISABLE(pAd);
	}
}
#endif




INT MtAsicTOPInit(RTMP_ADAPTER *pAd)
{
#ifdef MT7615_FPGA
	mt7615_chk_top_default_cr_setting(pAd);
	mt7615_chk_hif_default_cr_setting(pAd);
#endif /* MT7615_FPGA */

#if defined(MT7603_FPGA) || defined(MT7628_FPGA) || defined(MT7636_FPGA)
	UINT32 mac_val;

	// TODO: shiang-7603
#ifdef MT7628_FPGA
	// enable MAC circuit
	HW_IO_READ32(pAd, 0x2108, &mac_val);
	mac_val &= (~0x7ff0);
	HW_IO_WRITE32(pAd, 0x2108, mac_val);

	mac_val = 0x3e013;
	MAC_IO_WRITE32(pAd, 0x2d004, mac_val);
#endif /* MT7628_FPGA */

	MAC_IO_WRITE32(pAd, 0x24088, 0x900); // Set 40MHz Clock
	MAC_IO_WRITE32(pAd, 0x2d034, 0x64180003);	// Set 32k clock, this clock is used for lower power.
#endif /* defined(MT7603_FPGA) || defined(MT7628_FPGA) || defined(MT7636_FPGA) */

	return TRUE;
}


INT mt_hif_sys_init(RTMP_ADAPTER *pAd)
{

#ifdef RTMP_MAC_PCI
	if (IS_PCI_INF(pAd) || IS_RBUS_INF(pAd))
	{
		UINT32 mac_val;

		HIF_IO_READ32(pAd, MT_WPDMA_GLO_CFG, &mac_val);
		//mac_val |= 0xb0; // bit 7/5~4 => 1
		mac_val = 0x52000850;
		HIF_IO_WRITE32(pAd, MT_WPDMA_GLO_CFG, mac_val);
	}
#endif /* RTMP_MAC_PCI */


#ifdef RTMP_MAC_SDIO
	if (IS_SDIO_INF(pAd))
	{
		UINT32 Value;
		UINT32 counter=0;

		RTMP_SDIO_WRITE32(pAd, WHLPCR, W_INT_EN_CLR);
		RTMP_SDIO_READ32(pAd, WHLPCR, &Value);

		RTMP_SDIO_READ32(pAd, WCIR, &Value);

		if(GET_POR_INDICATOR(Value)) {// POR
			RTMP_SDIO_WRITE32(pAd, WCIR, POR_INDICATOR);
			RTMP_SDIO_READ32(pAd, WCIR, &Value);
			MTWF_LOG(DBG_CAT_ALL, DBG_SUBCAT_ALL, DBG_LVL_OFF, ("%s(): MCR_WCIR: Value:%x\n", __FUNCTION__, Value));
		}
		RtmpOsMsDelay(100);

		//		RTMP_SDIO_WRITE32(pAd, WHIER, 0x0);
		RTMP_SDIO_WRITE32(pAd, WHLPCR, W_INT_EN_CLR);

		//Poll W_FUNC for FW own back
		MTWF_LOG(DBG_CAT_ALL, DBG_SUBCAT_ALL, DBG_LVL_OFF, ("%s(): Request FW-Own back\n",__FUNCTION__));

		RTMP_SDIO_READ32(pAd, WHLPCR, &Value);
		RTMP_SDIO_WRITE32(pAd, WHLPCR, W_FW_OWN_REQ_CLR);
		while(!GET_W_FW_OWN_REQ_SET(Value)) {
			RTMP_SDIO_READ32(pAd, WHLPCR, &Value);
			MTWF_LOG(DBG_CAT_ALL, DBG_SUBCAT_ALL, DBG_LVL_OFF, ("%s(): Request FW-Own processing: %x\n",__FUNCTION__,Value));
			counter++;
			RtmpOsMsDelay(50);
			if(counter >100){
				MTWF_LOG(DBG_CAT_ALL, DBG_SUBCAT_ALL, DBG_LVL_ERROR, ("%s:  FW-Own back Faiure\n",__FUNCTION__));
				break;
			}
		}

		RTMP_SDIO_WRITE32(pAd, WHLPCR, W_INT_EN_CLR);
		RTMP_SDIO_READ32(pAd, WHLPCR, &Value);
		MTWF_LOG(DBG_CAT_ALL, DBG_SUBCAT_ALL, DBG_LVL_OFF, ("%s(): MCR_WHLPCR: Value:%x\n", __FUNCTION__, Value));

		RTMP_SDIO_WRITE32(pAd, WHIER, 0x46);
		RTMP_SDIO_READ32(pAd, WASR, &Value);
		MTWF_LOG(DBG_CAT_ALL, DBG_SUBCAT_ALL, DBG_LVL_OFF, ("%s(): WASR: Value:%x\n", __FUNCTION__, Value));
		RTMP_SDIO_READ32(pAd, WHIER, &Value);
		MTWF_LOG(DBG_CAT_ALL, DBG_SUBCAT_ALL, DBG_LVL_OFF, ("%s(): MCR_WHIER: Value:%x\n", __FUNCTION__, Value));
		RTMP_SDIO_READ32(pAd, WHISR, &Value);
		MTWF_LOG(DBG_CAT_ALL, DBG_SUBCAT_ALL, DBG_LVL_OFF, ("%s(): MCR_WHISR: Value:%x\n", __FUNCTION__, Value));
		RTMP_SDIO_READ32(pAd, WASR, &Value);
		MTWF_LOG(DBG_CAT_ALL, DBG_SUBCAT_ALL, DBG_LVL_OFF, ("%s(): WASR: Value:%x\n", __FUNCTION__, Value));
		RTMP_SDIO_READ32(pAd, WCIR, &Value);
		MTWF_LOG(DBG_CAT_ALL, DBG_SUBCAT_ALL, DBG_LVL_OFF, ("%s(): MCR_WCIR: Value:%x\n", __FUNCTION__, Value));

		RTMP_SDIO_READ32(pAd, WHCR, &Value);

#if CFG_SDIO_RX_ENHANCE
		Value |= RX_ENHANCE_MODE;
#else
		Value &= ~RX_ENHANCE_MODE;
#endif /* CFG_SDIO_RX_AGG */

		RTMP_SDIO_WRITE32(pAd, WHCR, Value);
		RTMP_SDIO_READ32(pAd, WHCR, &Value);
		MTWF_LOG(DBG_CAT_ALL, DBG_SUBCAT_ALL, DBG_LVL_ERROR,("%s(): ==================>WHCR= %x\n", __FUNCTION__,Value));
		RTMP_SDIO_READ32(pAd, WHIER, &Value);
		MTWF_LOG(DBG_CAT_ALL, DBG_SUBCAT_ALL, DBG_LVL_ERROR,("%s(): ==================>WHIER= %x\n", __FUNCTION__,Value));

#if(CFG_SDIO_RX_AGG == 0) && (CFG_SDIO_INTR_ENHANCE == 0)
		SDIO_CFG_MAX_HIF_RX_LEN_NUM(pAd, 1);

#elif(CFG_SDIO_RX_AGG == 0) && (CFG_SDIO_INTR_ENHANCE == 1)
		SDIO_CFG_MAX_HIF_RX_LEN_NUM(pAd, 16);

#elif (CFG_SDIO_RX_AGG == 1) && (CFG_SDIO_INTR_ENHANCE == 1)
		SDIO_CFG_MAX_HIF_RX_LEN_NUM(pAd, 16);
#endif

#if CFG_SDIO_DRIVING_TUNE
                //===== driving
		RTMP_SDIO_READ32(pAd, CMDIOCR, &Value);
		DBGPRINT(RT_DEBUG_OFF, ("%s(): CMDIOCR 1: Value:%x\n", __FUNCTION__, Value));
		RTMP_SDIO_WRITE32(pAd, CMDIOCR, 0x00000022);
		RTMP_SDIO_READ32(pAd, CMDIOCR, &Value);
		DBGPRINT(RT_DEBUG_OFF, ("%s(): CMDIOCR 2: Value:%x\n", __FUNCTION__, Value));

		RTMP_SDIO_READ32(pAd, DAT0IOCR, &Value);
		DBGPRINT(RT_DEBUG_OFF, ("%s(): DAT0IOCR 1: Value:%x\n", __FUNCTION__, Value));
		RTMP_SDIO_WRITE32(pAd, DAT0IOCR, 0x00000022);
		RTMP_SDIO_READ32(pAd, DAT0IOCR, &Value);
		DBGPRINT(RT_DEBUG_OFF, ("%s(): DAT0IOCR 2: Value:%x\n", __FUNCTION__, Value));

		RTMP_SDIO_READ32(pAd, DAT1IOCR, &Value);
		DBGPRINT(RT_DEBUG_OFF, ("%s(): DAT1IOCR 1: Value:%x\n", __FUNCTION__, Value));
		RTMP_SDIO_WRITE32(pAd, DAT1IOCR, 0x00000022);
		RTMP_SDIO_READ32(pAd, DAT1IOCR, &Value);
		DBGPRINT(RT_DEBUG_OFF, ("%s(): DAT1IOCR 2: Value:%x\n", __FUNCTION__, Value));

		RTMP_SDIO_READ32(pAd, DAT2IOCR, &Value);
		DBGPRINT(RT_DEBUG_OFF, ("%s(): DAT2IOCR 1: Value:%x\n", __FUNCTION__, Value));
		RTMP_SDIO_WRITE32(pAd, DAT2IOCR, 0x00000022);
		RTMP_SDIO_READ32(pAd, DAT2IOCR, &Value);
		DBGPRINT(RT_DEBUG_OFF, ("%s(): DAT2IOCR 2: Value:%x\n", __FUNCTION__, Value));

		RTMP_SDIO_READ32(pAd, DAT3IOCR, &Value);
		DBGPRINT(RT_DEBUG_OFF, ("%s(): DAT3IOCR 1: Value:%x\n", __FUNCTION__, Value));
		RTMP_SDIO_WRITE32(pAd, DAT3IOCR, 0x0000002a);
		RTMP_SDIO_READ32(pAd, DAT3IOCR, &Value);
		DBGPRINT(RT_DEBUG_OFF, ("%s(): DAT3IOCR 2: Value:%x\n", __FUNCTION__, Value));

		//===== DAT output delay
		RTMP_SDIO_READ32(pAd, ODATDLYCR, &Value);
		DBGPRINT(RT_DEBUG_OFF, ("%s(): ODATDLYCR 1: Value:%x\n", __FUNCTION__, Value));
		RTMP_SDIO_WRITE32(pAd, ODATDLYCR, 0x00000000);
		RTMP_SDIO_READ32(pAd, ODATDLYCR, &Value);
		DBGPRINT(RT_DEBUG_OFF, ("%s(): ODATDLYCR 2: Value:%x\n", __FUNCTION__, Value));

		//===== cmdR output delay
		RTMP_SDIO_READ32(pAd, CMDDLYCR, &Value);
		DBGPRINT(RT_DEBUG_OFF, ("%s(): CMDDLYCR 1: Value:%x\n", __FUNCTION__, Value));
		RTMP_SDIO_WRITE32(pAd, CMDDLYCR, 0x00ff0000);
		RTMP_SDIO_READ32(pAd, CMDDLYCR, &Value);
		DBGPRINT(RT_DEBUG_OFF, ("%s(): CMDDLYCR 2: Value:%x\n", __FUNCTION__, Value));
#endif

#if CFG_SDIO_BIST
		INT32 bistlen = 1536*20;
		INT32 u4Ret = 0;
		INT32 u4idx = 0;
		INT32 u4bist = 0;
		INT32 bistlimit = 50;
		INT32 prbslimit = 100;
		UCHAR *pBuf;
		os_alloc_mem(NULL, (UCHAR **)&pBuf, 1536*21);

		DBGPRINT(RT_DEBUG_OFF, ("\n\n\n============ BIST Start ============\n"));
		DBGPRINT(RT_DEBUG_OFF, ("%s(): PRBS pattern seed=37\n", __FUNCTION__));
		RTMP_SDIO_WRITE32(pAd, WTMCR, 0x00370002);

		//u4Ret = MTSDIOMultiRead(pAd, WTMDR, pBuf, bistlen);
		//for(u4idx=0;u4idx<200;u4idx++){
		//	DBGPRINT(RT_DEBUG_OFF, (" %x", *(pBuf+u4idx)));
		//}
		for(u4bist=0;u4bist<prbslimit;u4bist++){
			u4Ret = MTSDIOMultiRead(pAd, WTMDR, pBuf, bistlen);
			if (u4Ret != 0){DBGPRINT(RT_DEBUG_OFF, ("%s(): error MTSDIOMultiRead u4Ret=%x\n", __FUNCTION__, u4Ret));}
		}
		for(u4bist=0;u4bist<prbslimit;u4bist++){
			u4Ret = MTSDIOMultiRead(pAd, WTMDR, pBuf, bistlen);
			if (u4Ret != 0){DBGPRINT(RT_DEBUG_OFF, ("%s(): error MTSDIOMultiRead u4Ret=%x\n", __FUNCTION__, u4Ret));}
		}
		for(u4bist=0;u4bist<prbslimit;u4bist++){
			u4Ret = MTSDIOMultiRead(pAd, WTMDR, pBuf, bistlen);
			if (u4Ret != 0){DBGPRINT(RT_DEBUG_OFF, ("%s(): error MTSDIOMultiRead u4Ret=%x\n", __FUNCTION__, u4Ret));}
		}
		//-----------------------------------------------------------------------------
		DBGPRINT(RT_DEBUG_OFF, ("%s(): PRBS pattern seed=ec\n", __FUNCTION__));
		RTMP_SDIO_WRITE32(pAd, WTMCR, 0x00ec0002);

		//u4Ret = MTSDIOMultiRead(pAd, WTMDR, pBuf, bistlen);
		//for(u4idx=0;u4idx<200;u4idx++){
		//	DBGPRINT(RT_DEBUG_OFF, (" %x", *(pBuf+u4idx)));
		//}
		for(u4bist=0;u4bist<prbslimit;u4bist++){
			u4Ret = MTSDIOMultiRead(pAd, WTMDR, pBuf, bistlen);
			if (u4Ret != 0){DBGPRINT(RT_DEBUG_OFF, ("%s(): error MTSDIOMultiRead u4Ret=%x\n", __FUNCTION__, u4Ret));}
		}
		for(u4bist=0;u4bist<prbslimit;u4bist++){
			u4Ret = MTSDIOMultiRead(pAd, WTMDR, pBuf, bistlen);
			if (u4Ret != 0){DBGPRINT(RT_DEBUG_OFF, ("%s(): error MTSDIOMultiRead u4Ret=%x\n", __FUNCTION__, u4Ret));}
		}
		for(u4bist=0;u4bist<prbslimit;u4bist++){
			u4Ret = MTSDIOMultiRead(pAd, WTMDR, pBuf, bistlen);
			if (u4Ret != 0){DBGPRINT(RT_DEBUG_OFF, ("%s(): error MTSDIOMultiRead u4Ret=%x\n", __FUNCTION__, u4Ret));}
	}

//-----------------------------------------------------------------------------
		DBGPRINT(RT_DEBUG_OFF, ("%s(): 0x5a 0x5a 0x5a 0x5a pattern\n", __FUNCTION__));
		RTMP_SDIO_WRITE32(pAd, WTMCR, 0x00000000);
		RTMP_SDIO_WRITE32(pAd, WTMDPCR0, 0x5A5A5A5A);

		for(u4bist=0;u4bist<bistlimit;u4bist++){
		u4Ret = MTSDIOMultiRead(pAd, WTMDR, pBuf, bistlen);
		if (u4Ret != 0){DBGPRINT(RT_DEBUG_OFF, ("%s(): error MTSDIOMultiRead u4Ret=%x\n", __FUNCTION__, u4Ret));}

		for(u4idx=0;u4idx<bistlen;u4idx++){
			if(*(pBuf+u4idx)!=0x5A){
				DBGPRINT(RT_DEBUG_OFF, (" error ====> %x\n", *(pBuf+u4idx)));
				u4idx=bistlen;
				u4bist=bistlimit;
			}
		}
		}
		for(u4bist=0;u4bist<bistlimit;u4bist++){
			u4Ret = MTSDIOMultiRead(pAd, WTMDR, pBuf, bistlen);
			if (u4Ret != 0){DBGPRINT(RT_DEBUG_OFF, ("%s(): error MTSDIOMultiRead u4Ret=%x\n", __FUNCTION__, u4Ret));}
		}
		DBGPRINT(RT_DEBUG_OFF, ("\n"));
//-----------------------------------------------------------------------------
		DBGPRINT(RT_DEBUG_OFF, ("%s(): 0xf0 0xf0 0xf0 0xf0 pattern\n", __FUNCTION__));
		RTMP_SDIO_WRITE32(pAd, WTMDPCR0, 0xF0F0F0F0);
		for(u4bist=0;u4bist<bistlimit;u4bist++){
		u4Ret = MTSDIOMultiRead(pAd, WTMDR, pBuf, bistlen);
		if (u4Ret != 0){DBGPRINT(RT_DEBUG_OFF, ("%s(): error MTSDIOMultiRead u4Ret=%x\n", __FUNCTION__, u4Ret));}

		for(u4idx=0;u4idx<bistlen;u4idx++){
			if(*(pBuf+u4idx)!=0xf0){
				DBGPRINT(RT_DEBUG_OFF, (" error ====> %x\n", *(pBuf+u4idx)));
				u4idx=bistlen;
				u4bist=bistlimit;
			}
		}
		}
		for(u4bist=0;u4bist<bistlimit;u4bist++){
			u4Ret = MTSDIOMultiRead(pAd, WTMDR, pBuf, bistlen);
			if (u4Ret != 0){DBGPRINT(RT_DEBUG_OFF, ("%s(): error MTSDIOMultiRead u4Ret=%x\n", __FUNCTION__, u4Ret));}
		}
		DBGPRINT(RT_DEBUG_OFF, ("\n"));
//-----------------------------------------------------------------------------

		DBGPRINT(RT_DEBUG_OFF, ("%s(): 21 43 65 87 pattern\n", __FUNCTION__));
		RTMP_SDIO_WRITE32(pAd, WTMDPCR0, 0x87654321);

		for(u4bist=0;u4bist<bistlimit;u4bist++){
		u4Ret = MTSDIOMultiRead(pAd, WTMDR, pBuf, bistlen);
		if (u4Ret != 0){DBGPRINT(RT_DEBUG_OFF, ("%s(): error MTSDIOMultiRead u4Ret=%x\n", __FUNCTION__, u4Ret));}

		for(u4idx=0;u4idx<bistlen;u4idx=u4idx+4){
			if(*(pBuf+u4idx+0)!=0x21){
				DBGPRINT(RT_DEBUG_OFF, (" error exp=21 read= %x\n", *(pBuf+u4idx+0)));
				u4idx=bistlen;
				u4bist=bistlimit;
			}
			if(*(pBuf+u4idx+1)!=0x43){
				DBGPRINT(RT_DEBUG_OFF, (" error exp=43 read= %x\n", *(pBuf+u4idx+1)));
				u4idx=bistlen;
				u4bist=bistlimit;
			}
			if(*(pBuf+u4idx+2)!=0x65){
				DBGPRINT(RT_DEBUG_OFF, (" error exp=65 read= %x\n", *(pBuf+u4idx+2)));
				u4idx=bistlen;
				u4bist=bistlimit;
			}
			if(*(pBuf+u4idx+3)!=0x87){
				DBGPRINT(RT_DEBUG_OFF, (" error exp=87 read= %x\n", *(pBuf+u4idx+3)));
				u4idx=bistlen;
				u4bist=bistlimit;
			}
		}
		}
		for(u4bist=0;u4bist<bistlimit;u4bist++){
			u4Ret = MTSDIOMultiRead(pAd, WTMDR, pBuf, bistlen);
			if (u4Ret != 0){DBGPRINT(RT_DEBUG_OFF, ("%s(): error MTSDIOMultiRead u4Ret=%x\n", __FUNCTION__, u4Ret));}
		}
		DBGPRINT(RT_DEBUG_OFF, ("\n"));
//-----------------------------------------------------------------------------
DBGPRINT(RT_DEBUG_OFF, ("%s(): 36 76 10 00 pattern\n", __FUNCTION__));
		RTMP_SDIO_WRITE32(pAd, WTMDPCR0, 0x00107636);

		for(u4bist=0;u4bist<bistlimit;u4bist++){
		u4Ret = MTSDIOMultiRead(pAd, WTMDR, pBuf, bistlen);
		if (u4Ret != 0){DBGPRINT(RT_DEBUG_OFF, ("%s(): error MTSDIOMultiRead u4Ret=%x\n", __FUNCTION__, u4Ret));}

		for(u4idx=0;u4idx<bistlen;u4idx=u4idx+4){
			if(*(pBuf+u4idx+0)!=0x36){
				DBGPRINT(RT_DEBUG_OFF, (" error exp=36 read= %x\n", *(pBuf+u4idx+0)));
				u4idx=bistlen;
				u4bist=bistlimit;
			}
			if(*(pBuf+u4idx+1)!=0x76){
				DBGPRINT(RT_DEBUG_OFF, (" error exp=76 read= %x\n", *(pBuf+u4idx+1)));
				u4idx=bistlen;
				u4bist=bistlimit;
			}
			if(*(pBuf+u4idx+2)!=0x10){
				DBGPRINT(RT_DEBUG_OFF, (" error exp=10 read= %x\n", *(pBuf+u4idx+2)));
				u4idx=bistlen;
				u4bist=bistlimit;
			}
			if(*(pBuf+u4idx+3)!=0x00){
				DBGPRINT(RT_DEBUG_OFF, (" error exp=00 read= %x\n", *(pBuf+u4idx+3)));
				u4idx=bistlen;
				u4bist=bistlimit;
			}
		}
		}
		for(u4bist=0;u4bist<bistlimit;u4bist++){
			u4Ret = MTSDIOMultiRead(pAd, WTMDR, pBuf, bistlen);
			if (u4Ret != 0){DBGPRINT(RT_DEBUG_OFF, ("%s(): error MTSDIOMultiRead u4Ret=%x\n", __FUNCTION__, u4Ret));}
		}
		DBGPRINT(RT_DEBUG_OFF, ("\n"));
//-----------------------------------------------------------------------------

		DBGPRINT(RT_DEBUG_OFF, ("%s(): 0x01 0x01 0x01 0x01 pattern\n", __FUNCTION__));
		RTMP_SDIO_WRITE32(pAd, WTMDPCR0, 0x01010101);

		for(u4bist=0;u4bist<bistlimit;u4bist++){
		u4Ret = MTSDIOMultiRead(pAd, WTMDR, pBuf, bistlen);
		if (u4Ret != 0){DBGPRINT(RT_DEBUG_OFF, ("%s(): error MTSDIOMultiRead u4Ret=%x\n", __FUNCTION__, u4Ret));}

		for(u4idx=0;u4idx<bistlen;u4idx++){
			if(*(pBuf+u4idx)!=0x1){
				DBGPRINT(RT_DEBUG_OFF, (" error ====> %x\n", *(pBuf+u4idx)));
				u4idx=bistlen;
				u4bist=bistlimit;
			}
		}
		}
		for(u4bist=0;u4bist<bistlimit;u4bist++){
			u4Ret = MTSDIOMultiRead(pAd, WTMDR, pBuf, bistlen);
			if (u4Ret != 0){DBGPRINT(RT_DEBUG_OFF, ("%s(): error MTSDIOMultiRead u4Ret=%x\n", __FUNCTION__, u4Ret));}
		}
		DBGPRINT(RT_DEBUG_OFF, ("\n"));
		DBGPRINT(RT_DEBUG_OFF, ("============ BIST End ============\n\n\n"));
		os_free_mem(NULL, pBuf);

#endif
}

#endif

	return TRUE;
}


BOOLEAN MonitorRxPse(RTMP_ADAPTER *pAd)
{
	UINT32 RemapBase, RemapOffset;
	UINT32 Value;
	UINT32 RestoreValue;

	if (pAd->RxPseCheckTimes < 10)
	{
		/* Check RX FIFO if not ready */
		MAC_IO_WRITE32(pAd, 0x4244, 0x28000000);
		MAC_IO_READ32(pAd, 0x4244, &Value);
		if ((Value & (1 << 8)) != 0)
		{
			pAd->RxPseCheckTimes = 0;
			return FALSE;
		}
		else
		{
			MAC_IO_READ32(pAd, MCU_PCIE_REMAP_2, &RestoreValue);
			RemapBase = GET_REMAP_2_BASE(0x800c006c) << 19;
			RemapOffset = GET_REMAP_2_OFFSET(0x800c006c);
			MAC_IO_WRITE32(pAd, MCU_PCIE_REMAP_2, RemapBase);

			MAC_IO_WRITE32(pAd, 0x80000 + RemapOffset, 3);

			MAC_IO_READ32(pAd, 0x80000 + RemapOffset, &Value);

			if(((Value & (0x8001 << 16)) == (0x8001 << 16)) ||
					((Value & (0xe001 << 16)) == (0xe001 << 16)))
			{
				pAd->RxPseCheckTimes++;
				MAC_IO_WRITE32(pAd, MCU_PCIE_REMAP_2, RestoreValue);
				return FALSE;
			}
			else
			{
				pAd->RxPseCheckTimes = 0;
				MAC_IO_WRITE32(pAd, MCU_PCIE_REMAP_2, RestoreValue);
				return FALSE;
			}
		}
	}
	else
	{
		pAd->RxPseCheckTimes = 0;
		return TRUE;
	}
}


VOID PSEWatchDog(RTMP_ADAPTER *pAd)
{
	BOOLEAN NoDataIn = FALSE;

	NoDataIn = MonitorRxPse(pAd);

	if (NoDataIn)
	{
		DBGPRINT(RT_DEBUG_OFF, ("PSE Reset\n"));
		pAd->PSEResetCount++;
		goto reset;
	}

	return;

reset:
	;
#ifdef RTMP_PCI_SUPPORT
	PSEResetAndRecovery(pAd);
#endif
}

