/*
 * 86Box	A hypervisor and IBM PC system emulator that specializes in
 *		running old operating systems and software designed for IBM
 *		PC systems and compatibles from 1981 through fairly recent
 *		system designs based on the PCI bus.
 *
 *		This file is part of the 86Box distribution.
 *
 *		Emulation of the Brooktree BT485 and BT485A true colour
 *		RAM DAC's.
 *
 * Version:	@(#)vid_bt485_ramdac.c	1.0.10	2018/10/04
 *
 * Authors:	Miran Grca, <mgrca8@gmail.com>
 *		TheCollector1995,
 *
 *		Copyright 2016-2018 Miran Grca.
 *		Copyright 2018 TheCollector1995.
 */
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <wchar.h>
#include "../86box.h"
#include "../mem.h"
#include "video.h"
#include "vid_svga.h"
#include "vid_bt485_ramdac.h"


static void
bt485_set_bpp(bt485_ramdac_t *ramdac, svga_t *svga)
{
    if (!(ramdac->cr2 & 0x20))
	svga->bpp = 8;
    else switch ((ramdac->cr1 >> 5) & 0x03) {
	case 0:
		svga->bpp = 32;
		break;
	case 1:
		if (ramdac->cr1 & 0x08)
			svga->bpp = 16;
		else
			svga->bpp = 15;
		break;
	case 2:
		svga->bpp = 8;
		break;
	case 3:
		svga->bpp = 4;
		break;
    }
    svga_recalctimings(svga);
}


void
bt485_ramdac_out(uint16_t addr, int rs2, int rs3, uint8_t val, bt485_ramdac_t *ramdac, svga_t *svga)
{
    uint32_t o32;
    uint8_t *cd;
    uint8_t index;
    uint8_t rs = (addr & 0x03);
    rs |= (!!rs2 << 2);
    rs |= (!!rs3 << 3);

    switch (rs) {
	case 0x00:	/* Palette Write Index Register (RS value = 0000) */
		svga_out(addr, val, svga);
		if (ramdac->type >= BT485)
			svga->dac_addr |= ((int) (ramdac->cr3 & 0x03) << 8);
		break;
	case 0x03:
		svga->dac_pos = 0;
		svga->dac_status = addr & 0x03;
		svga->dac_addr = val;
		if (ramdac->type >= BT485)
			svga->dac_addr |= ((int) (ramdac->cr3 & 0x03) << 8);
		svga->dac_addr++;
		if (ramdac->type >= BT485)
			svga->dac_addr &= 0x3ff;
		else
			svga->dac_addr &= 0x0ff;
		break;
	case 0x01:	/* Palette Data Register (RS value = 0001) */
	case 0x02:	/* Pixel Read Mask Register (RS value = 0010) */
		svga_out(addr, val, svga);
		break;
	case 0x04:	/* Ext Palette Write Index Register (RS value = 0100) */
	case 0x07:	/* Ext Palette Read Index Register (RS value = 0111) */
		svga->dac_pos = 0;
		svga->dac_status = rs & 0x03;
		ramdac->ext_addr = (val + (rs & 0x01)) & 255;
		break;
	case 0x05:	/* Ext Palette Data Register (RS value = 0101) */
		svga->dac_status = 0;
		svga->fullchange = changeframecount;
		switch (svga->dac_pos) {
			case 0:
				svga->dac_r = val;
				svga->dac_pos++;
				break;
			case 1:
				svga->dac_g = val;
				svga->dac_pos++;
				break;
			case 2:
				index = ramdac->ext_addr & 3;
				ramdac->extpal[index].r = svga->dac_r;
				ramdac->extpal[index].g = svga->dac_g;
				ramdac->extpal[index].b = val;
				if (svga->ramdac_type == RAMDAC_8BIT)
					ramdac->extpallook[index] = makecol32(ramdac->extpal[index].r, ramdac->extpal[index].g, ramdac->extpal[index].b);
				else
					ramdac->extpallook[index] = makecol32(video_6to8[ramdac->extpal[index].r & 0x3f], video_6to8[ramdac->extpal[index].g & 0x3f], video_6to8[ramdac->extpal[index].b & 0x3f]);

				if ((svga->crtc[0x33] & 0x40) && !index) {
					o32 = svga->overscan_color;
					svga->overscan_color = ramdac->extpallook[0];
					if (o32 != svga->overscan_color)
						svga_recalctimings(svga);
				}
				ramdac->ext_addr = (ramdac->ext_addr + 1) & 0xff;
				svga->dac_pos = 0;
				break;
		}
		break;
	case 0x06:	/* Command Register 0 (RS value = 0110) */
		ramdac->cr0 = val;
		svga->ramdac_type = (val & 0x02) ? RAMDAC_8BIT : RAMDAC_6BIT;
		break;
	case 0x08:	/* Command Register 1 (RS value = 1000) */
		ramdac->cr1 = val;
		bt485_set_bpp(ramdac, svga);
		break;
	case 0x09:	/* Command Register 2 (RS value = 1001) */
		ramdac->cr2 = val;
		svga->hwcursor.ena = !!(val & 0x03);
		bt485_set_bpp(ramdac, svga);
		break;
	case 0x0a:
		if ((ramdac->type >= BT485) && (ramdac->cr0 & 0x80)) {
			switch ((svga->dac_addr & 0xff)) {
				case 0x01:
					/* Command Register 3 (RS value = 1010) */
					ramdac->cr3 = val;
					svga->hwcursor.xsize = svga->hwcursor.ysize = (val & 4) ? 64 : 32;
					svga->hwcursor.yoff = (svga->hwcursor.ysize == 32) ? 32 : 0;
					svga->hwcursor.x = ramdac->hwc_x - svga->hwcursor.xsize;
					svga->hwcursor.y = ramdac->hwc_y - svga->hwcursor.ysize;
					svga->dac_addr = (svga->dac_addr & 0x00ff) | ((val & 0x03) << 8);
					svga_recalctimings(svga);
					break;
				case 0x02:
				case 0x20:
				case 0x21:
				case 0x22:
					if (ramdac->type != BT485A)
						break;
					else if (svga->dac_addr == 2) {
						ramdac->cr4 = val;
						break;
					}
					break;
			}
		}
		break;
	case 0x0b:	/* Cursor RAM Data Register (RS value = 1011) */
		index = svga->dac_addr & 0x03ff;
		if ((ramdac->type >= BT485) && (svga->hwcursor.xsize == 64))
			cd = (uint8_t *) ramdac->cursor64_data;
		else {
			if (ramdac->type < BT485)
				index &= 0x00ff;
			cd = (uint8_t *) ramdac->cursor32_data;
		}

		cd[index] = val;

		svga->dac_addr++;
		svga->dac_addr &= (ramdac->type >= BT485) ? 0x03ff : 0x00ff;
		break;
	case 0x0c:	/* Cursor X Low Register (RS value = 1100) */
		ramdac->hwc_x = (ramdac->hwc_x & 0x0f00) | val;
		svga->hwcursor.x = ramdac->hwc_x - svga->hwcursor.xsize;
		break;
	case 0x0d:	/* Cursor X High Register (RS value = 1101) */
		ramdac->hwc_x = (ramdac->hwc_x & 0x00ff) | ((val & 0x0f) << 8);
		svga->hwcursor.x = ramdac->hwc_x - svga->hwcursor.xsize;
		break;
	case 0x0e:	/* Cursor Y Low Register (RS value = 1110) */
		ramdac->hwc_y = (ramdac->hwc_y & 0x0f00) | val;
		svga->hwcursor.y = ramdac->hwc_y - svga->hwcursor.ysize;
		break;
	case 0x0f:	/* Cursor Y High Register (RS value = 1111) */
		ramdac->hwc_y = (ramdac->hwc_y & 0x00ff) | ((val & 0x0f) << 8);
		svga->hwcursor.y = ramdac->hwc_y - svga->hwcursor.ysize;
		break;
    }

    return;
}


uint8_t
bt485_ramdac_in(uint16_t addr, int rs2, int rs3, bt485_ramdac_t *ramdac, svga_t *svga)
{
    uint8_t temp = 0xff;
    uint8_t *cd;
    uint8_t index;
    uint8_t rs = (addr & 0x03);
    rs |= (!!rs2 << 2);
    rs |= (!!rs3 << 3);

    switch (rs) {
	case 0x00:	/* Palette Write Index Register (RS value = 0000) */
	case 0x01:	/* Palette Data Register (RS value = 0001) */
	case 0x02:	/* Pixel Read Mask Register (RS value = 0010) */
		temp = svga_in(addr, svga);
		break;
	case 0x03:	/* Palette Read Index Register (RS value = 0011) */
		temp = svga->dac_addr & 0xff;
		break;
	case 0x04:	/* Ext Palette Write Index Register (RS value = 0100) */
		temp = ramdac->ext_addr;
		break;
	case 0x05:	/* Ext Palette Data Register (RS value = 0101) */
		index = (ramdac->ext_addr - 1) & 3;
		svga->dac_status = 3;
		switch (svga->dac_pos) {
			case 0:
				svga->dac_pos++;
				if (svga->ramdac_type == RAMDAC_8BIT)
					temp = ramdac->extpal[index].r;
				else
					temp = ramdac->extpal[index].r & 0x3f;
			case 1:
				svga->dac_pos++;
				if (svga->ramdac_type == RAMDAC_8BIT)
					temp = ramdac->extpal[index].g;
				else
					temp = ramdac->extpal[index].g & 0x3f;
			case 2:
				svga->dac_pos=0;
				ramdac->ext_addr = ramdac->ext_addr + 1;
				if (svga->ramdac_type == RAMDAC_8BIT)
					temp = ramdac->extpal[index].b;
				else
					temp = ramdac->extpal[index].b & 0x3f;
		}
		break;
	case 0x06:	/* Command Register 0 (RS value = 0110) */
		temp = ramdac->cr0;
		break;
	case 0x07:	/* Ext Palette Read Index Register (RS value = 0111) */
		temp = ramdac->ext_addr;
		break;
	case 0x08:	/* Command Register 1 (RS value = 1000) */
		temp = ramdac->cr1;
		break;
	case 0x09:	/* Command Register 2 (RS value = 1001) */
		temp = ramdac->cr2;
		break;
	case 0x0a:
		if ((ramdac->type >= BT485) && (ramdac->cr0 & 0x80)) {
			switch ((svga->dac_addr & 0xff)) {
				case 0x00:
					temp = ramdac->status | (svga->dac_status ? 0x04 : 0x00);
					break;
				case 0x01:
					temp = ramdac->cr3 & 0xfc;
					temp |= (svga->dac_addr & 0x300) >> 8;
					break;
				case 0x02:
				case 0x20:
				case 0x21:
				case 0x22:
					if (ramdac->type != BT485A)
						break;
					else if (svga->dac_addr == 2) {
						temp = ramdac->cr4;
						break;
					} else {
						/* TODO: Red, Green, and Blue Signature Analysis Registers */
						temp = 0xff;
						break;
					}
					break;
			}
		} else
			temp = ramdac->status | (svga->dac_status ? 0x04 : 0x00);
		break;
	case 0x0b:	/* Cursor RAM Data Register (RS value = 1011) */
		index = (svga->dac_addr - 1) & 0x03ff;
		if ((ramdac->type >= BT485) && (svga->hwcursor.xsize == 64))
			cd = (uint8_t *) ramdac->cursor64_data;
		else {
			if (ramdac->type < BT485)
				index &= 0x00ff;
			cd = (uint8_t *) ramdac->cursor32_data;
		}

		temp = cd[index];

		svga->dac_addr++;
		svga->dac_addr &= (ramdac->type >= BT485) ? 0x03ff : 0x00ff;
		break;
	case 0x0c:	/* Cursor X Low Register (RS value = 1100) */
		temp = ramdac->hwc_x & 0xff;
		break;
	case 0x0d:	/* Cursor X High Register (RS value = 1101) */
		temp = (ramdac->hwc_x >> 8) & 0xff;
		break;
	case 0x0e:	/* Cursor Y Low Register (RS value = 1110) */
		temp = ramdac->hwc_y & 0xff;
		break;
	case 0x0f:	/* Cursor Y High Register (RS value = 1111) */
		temp = (ramdac->hwc_y >> 8) & 0xff;
		break;
    }

    return temp;
}

void bt485_init(bt485_ramdac_t *ramdac, svga_t *svga, uint8_t type)
{
    memset(ramdac, 0, sizeof(bt485_ramdac_t));
    ramdac->type = type;

    if (ramdac->type < BT485) {
	/* The BT484 and AT&T 20C504 only have a 32x32 cursor. */
	svga->hwcursor.xsize = svga->hwcursor.ysize = 32;
	svga->hwcursor.yoff = 32;
    }

    /* Set the RAM DAC status byte to the correct ID bits.

	Both the BT484 and BT485 datasheets say this:
	SR7-SR6: These bits are identification values. SR7=0 and SR6=1.
	But all other sources seem to assume SR7=1 and SR6=0. */
    switch (ramdac->type) {
	case BT484:
		ramdac->status = 0x40;
		break;
	case ATT20C504:
		ramdac->status = 0x40;
		break;
	case BT485:
		ramdac->status = 0x60;
		break;
	case ATT20C505:
		ramdac->status = 0xd0;
		break;
	case BT485A:
		ramdac->status = 0x20;
		break;
    }
}
