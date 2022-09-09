// Copyright 2013 The Chromium Authors
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

#include "chrome/browser/password_manager/password_manager_buildflags.h"

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
DECLARE_RESOURCE_ID(IDR_ANDROID_INFOBAR_LITE_MODE, R.drawable.preview_pin_round)
DECLARE_RESOURCE_ID(IDR_ANDROID_INFOBAR_NOTIFICATIONS_OFF,
                    R.drawable.permission_push_notification_off)
DECLARE_RESOURCE_ID(IDR_ANDROID_INFOBAR_PREVIEWS, R.drawable.infobar_chrome)
DECLARE_RESOURCE_ID(IDR_ANDROID_INFOBAR_SAFETYTIP_SHIELD,
                    R.drawable.safetytip_shield)
DECLARE_RESOURCE_ID(IDR_ANDROID_INFOBAR_SAVE_PASSWORD,
                    R.drawable.ic_vpn_key_blue)
DECLARE_RESOURCE_ID(IDR_ANDROID_INFOBAR_TRANSLATE, R.drawable.infobar_translate)
DECLARE_RESOURCE_ID(IDR_ANDROID_INFOBAR_MOBILE_FRIENDLY,
                    R.drawable.infobar_mobile_friendly)
LINK_RESOURCE_ID(IDR_AUTOFILL_GOOGLE_PAY_WITH_DIVIDER,
                 R.drawable.google_pay_with_divider)

// Android only messages (an alternative UI to infobars)
DECLARE_RESOURCE_ID(IDR_ANDORID_MESSAGE_PERMISSION_VIDEOCAM,
                    R.drawable.ic_videocam_24dp)
DECLARE_RESOURCE_ID(IDR_ANDORID_MESSAGE_PERMISSION_STORAGE,
                    R.drawable.ic_storage)
DECLARE_RESOURCE_ID(IDR_ANDORID_MESSAGE_PERMISSION_CAMERA,
                    R.drawable.ic_photo_camera_black)
DECLARE_RESOURCE_ID(IDR_ANDROID_MESSAGE_SETTINGS, R.drawable.settings_cog)
DECLARE_RESOURCE_ID(IDR_ANDROID_MESSAGE_SAFETY_CHECK, R.drawable.safety_check)
DECLARE_RESOURCE_ID(IDR_ANDROID_MESSAGE_SHIELD, R.drawable.shield)
DECLARE_RESOURCE_ID(IDR_ANDORID_MESSAGE_PASSWORD_MANAGER_ERROR,
                    R.drawable.ic_key_error)

// Unified Password Manager resources
// Color logo is used for Google branded builds only.
#if BUILDFLAG(PASSWORD_MANAGER_USE_INTERNAL_ANDROID_RESOURCES)
DECLARE_RESOURCE_ID(IDR_ANDROID_PASSWORD_MANAGER_LOGO_24DP,
                    R.drawable.ic_password_manager_logo_24dp)
#else
DECLARE_RESOURCE_ID(IDR_ANDROID_PASSWORD_MANAGER_LOGO_24DP,
                    R.drawable.ic_vpn_key_blue)
#endif

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
LINK_RESOURCE_ID(IDR_AUTOFILL_GOOGLE_PAY, R.drawable.google_pay)
// Use DECLARE_RESOURCE_ID here as these resources are used for android only.
DECLARE_RESOURCE_ID(IDR_ANDROID_AUTOFILL_CC_SCAN_NEW,
                    R.drawable.ic_photo_camera_black)
DECLARE_RESOURCE_ID(IDR_ANDROID_AUTOFILL_HTTP_WARNING,
                    R.drawable.ic_info_outline_grey_16dp)
DECLARE_RESOURCE_ID(IDR_ANDROID_AUTOFILL_HTTPS_INVALID_WARNING,
                    R.drawable.ic_warning_red_16dp)
DECLARE_RESOURCE_ID(IDR_ANDROID_AUTOFILL_OFFER_TAG_GREEN,
                    R.drawable.ic_offer_tag)

DECLARE_RESOURCE_ID(IDR_SEND_TAB_TO_SELF, R.drawable.send_tab)

// We display settings and edit icon for keyboard accessory using Android's
// |VectorDrawableCompat|. We do not display these icons for autofill popup.
DECLARE_RESOURCE_ID(IDR_ANDROID_AUTOFILL_SETTINGS, R.drawable.ic_settings_black)
DECLARE_RESOURCE_ID(IDR_ANDROID_AUTOFILL_CREATE, R.drawable.ic_edit_24dp)

// Icon displayed in the save address message on Android.
DECLARE_RESOURCE_ID(IDR_ANDROID_AUTOFILL_ADDRESS,
                    R.drawable.gm_filled_location_on_24)

// We use PNG files for the following images.
LINK_RESOURCE_ID(IDR_CREDIT_CARD_CVC_HINT, R.drawable.cvc_icon)
LINK_RESOURCE_ID(IDR_CREDIT_CARD_CVC_HINT_AMEX, R.drawable.cvc_icon_amex)
