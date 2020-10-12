// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file maps Chromium resource IDs to Android resource IDs.

// Presence of regular include guards is checked by:
// 1. cpplint
// 2. a custom presubmit in src/PRESUBMIT.py
// 3. clang (but it only checks the guard is correct if present)
// Disable the first two with these magic comments:
// NOLINT(build/header_guard)
// no-include-guard-because-multiply-included

// LINK_RESOURCE_ID is used for IDs that come from a .grd file.
#ifndef LINK_RESOURCE_ID
#error "LINK_RESOURCE_ID should be defined before including this file"
#endif
// DECLARE_RESOURCE_ID is used for IDs that don't have .grd entries, and
// are only declared in this file.
#ifndef DECLARE_RESOURCE_ID
#error "DECLARE_RESOURCE_ID should be defined before including this file"
#endif

// Create a mapping that identifies when a resource isn't being passed in.
LINK_RESOURCE_ID(0, 0)

// InfoBar resources.
LINK_RESOURCE_ID(IDR_INFOBAR_AUTOFILL_CC, R.drawable.infobar_autofill_cc)

// Android only infobars.
DECLARE_RESOURCE_ID(IDR_ANDROID_INFOBAR_FROZEN_TAB, R.drawable.infobar_restore)
DECLARE_RESOURCE_ID(IDR_ANDROID_INFOBAR_LITE_MODE, R.drawable.preview_pin_round)
DECLARE_RESOURCE_ID(IDR_ANDROID_INFOBAR_MEDIA_STREAM_SCREEN,
                    R.drawable.infobar_screen_share)
DECLARE_RESOURCE_ID(IDR_ANDROID_INFOBAR_MULTIPLE_DOWNLOADS,
                    R.drawable.infobar_downloading)
DECLARE_RESOURCE_ID(IDR_ANDROID_INFOBAR_NOTIFICATIONS_OFF,
                    R.drawable.permission_push_notification_off)
DECLARE_RESOURCE_ID(IDR_ANDROID_INFOBAR_PREVIEWS, R.drawable.infobar_chrome)
DECLARE_RESOURCE_ID(IDR_ANDROID_INFOBAR_SAFETYTIP_SHIELD,
                    R.drawable.safetytip_shield)
DECLARE_RESOURCE_ID(IDR_ANDROID_INFOBAR_SAVE_PASSWORD,
                    R.drawable.ic_vpn_key_blue)
DECLARE_RESOURCE_ID(IDR_ANDROID_INFOBAR_TRANSLATE, R.drawable.infobar_translate)
LINK_RESOURCE_ID(IDR_AUTOFILL_GOOGLE_PAY_WITH_DIVIDER,
                 R.drawable.google_pay_with_divider)

// Autofill popup and keyboard accessory images.
// We use Android's |VectorDrawableCompat| for the following images that are
// displayed using |DropdownAdapter|.
LINK_RESOURCE_ID(IDR_AUTOFILL_CC_AMEX, R.drawable.amex_card)
LINK_RESOURCE_ID(IDR_AUTOFILL_CC_DINERS, R.drawable.diners_card)
LINK_RESOURCE_ID(IDR_AUTOFILL_CC_DISCOVER, R.drawable.discover_card)
LINK_RESOURCE_ID(IDR_AUTOFILL_CC_ELO, R.drawable.elo_card)
LINK_RESOURCE_ID(IDR_AUTOFILL_CC_GENERIC, R.drawable.ic_credit_card_black)
LINK_RESOURCE_ID(IDR_AUTOFILL_CC_JCB, R.drawable.jcb_card)
LINK_RESOURCE_ID(IDR_AUTOFILL_CC_MASTERCARD, R.drawable.mc_card)
LINK_RESOURCE_ID(IDR_AUTOFILL_CC_MIR, R.drawable.mir_card)
LINK_RESOURCE_ID(IDR_AUTOFILL_CC_TROY, R.drawable.troy_card)
LINK_RESOURCE_ID(IDR_AUTOFILL_CC_UNIONPAY, R.drawable.unionpay_card)
LINK_RESOURCE_ID(IDR_AUTOFILL_CC_VISA, R.drawable.visa_card)
LINK_RESOURCE_ID(IDR_AUTOFILL_GOOGLE_ISSUED_CARD,
                 R.drawable.ic_credit_card_black)
LINK_RESOURCE_ID(IDR_AUTOFILL_GOOGLE_PAY, R.drawable.google_pay)
// Use DECLARE_RESOURCE_ID here as these resources are used for android only.
DECLARE_RESOURCE_ID(IDR_ANDROID_AUTOFILL_CC_SCAN_NEW,
                    R.drawable.ic_photo_camera_black)
DECLARE_RESOURCE_ID(IDR_ANDROID_AUTOFILL_HTTP_WARNING,
                    R.drawable.ic_info_outline_grey_16dp)
DECLARE_RESOURCE_ID(IDR_ANDROID_AUTOFILL_HTTPS_INVALID_WARNING,
                    R.drawable.ic_warning_red_16dp)

// We display settings and edit icon for keyboard accessory using Android's
// |VectorDrawableCompat|. We do not display these icons for autofill popup.
DECLARE_RESOURCE_ID(IDR_ANDROID_AUTOFILL_SETTINGS, R.drawable.ic_settings_black)
DECLARE_RESOURCE_ID(IDR_ANDROID_AUTOFILL_CREATE, R.drawable.ic_edit_24dp)

// We use PNG files for the following images.
LINK_RESOURCE_ID(IDR_CREDIT_CARD_CVC_HINT, R.drawable.cvc_icon)
LINK_RESOURCE_ID(IDR_CREDIT_CARD_CVC_HINT_AMEX, R.drawable.cvc_icon_amex)
