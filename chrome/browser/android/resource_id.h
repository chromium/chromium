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

#include "chrome/browser/page_info/page_info_buildflags.h"
#include "components/password_manager/core/browser/password_manager_buildflags.h"

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
DECLARE_RESOURCE_ID(IDR_ANDROID_INFOBAR_TRANSLATE, R.drawable.infobar_translate)
LINK_RESOURCE_ID(IDR_AUTOFILL_GOOGLE_PAY_WITH_DIVIDER,
                 R.drawable.google_pay_with_divider)

// Android only messages (an alternative UI to infobars)
DECLARE_RESOURCE_ID(IDR_ANDORID_MESSAGE_PERMISSION_VIDEOCAM,
                    R.drawable.ic_videocam_24dp)
DECLARE_RESOURCE_ID(IDR_ANDORID_MESSAGE_PERMISSION_STORAGE,
                    R.drawable.ic_storage)
DECLARE_RESOURCE_ID(IDR_ANDORID_MESSAGE_PERMISSION_CAMERA,
                    R.drawable.ic_photo_camera_black)
DECLARE_RESOURCE_ID(IDR_ANDROID_MESSAGE_PERMISSION_XR,
                    R.drawable.gm_filled_cardboard_24)
DECLARE_RESOURCE_ID(IDR_ANDROID_MESSAGE_PERMISSION_HAND_TRACKING,
                    R.drawable.gm_filled_hand_gesture_24)
DECLARE_RESOURCE_ID(IDR_ANDROID_MESSAGE_SETTINGS, R.drawable.settings_cog)
DECLARE_RESOURCE_ID(IDR_ANDROID_MESSAGE_SHIELD_BLUE,
                    R.drawable.blue_google_shield)
DECLARE_RESOURCE_ID(IDR_ANDROID_MESSAGE_SHIELD_GRAY, R.drawable.gray_shield)
DECLARE_RESOURCE_ID(IDR_ANDORID_MESSAGE_PASSWORD_MANAGER_ERROR,
                    R.drawable.ic_key_error)
DECLARE_RESOURCE_ID(IDR_ANDROID_IC_MOBILE_FRIENDLY,
                    R.drawable.ic_mobile_friendly)
DECLARE_RESOURCE_ID(IDR_ANDROID_MESSAGE_LOCATION_OFF,
                    R.drawable.permission_location_off)

// Unified Password Manager resources
// Color logo is used for Google branded builds only.
#if BUILDFLAG(PASSWORD_MANAGER_USE_INTERNAL_ANDROID_RESOURCES)
DECLARE_RESOURCE_ID(IDR_ANDROID_PASSWORD_MANAGER_LOGO_24DP,
                    R.drawable.ic_password_manager_logo_24dp)
#else
DECLARE_RESOURCE_ID(IDR_ANDROID_PASSWORD_MANAGER_LOGO_24DP,
                    R.drawable.ic_vpn_key_blue)
#endif

// The icon shown when an update to GMSCore is needed.
DECLARE_RESOURCE_ID(IDR_ANDROID_IC_ERROR, R.drawable.ic_error)

DECLARE_RESOURCE_ID(IDR_SEND_TAB_TO_SELF, R.drawable.send_tab)

// Icon displayed in the save address message on Android.
DECLARE_RESOURCE_ID(IDR_ANDROID_AUTOFILL_ADDRESS,
                    R.drawable.gm_filled_location_on_24)
DECLARE_RESOURCE_ID(IDR_ANDROID_AUTOFILL_UPLOAD_ADDRESS,
                    R.drawable.ic_cloud_upload_24dp)

// We use PNG files for the following images.
LINK_RESOURCE_ID(IDR_CREDIT_CARD_CVC_HINT_BACK, R.drawable.cvc_icon)
LINK_RESOURCE_ID(IDR_CREDIT_CARD_CVC_HINT_FRONT_AMEX, R.drawable.cvc_icon_amex)

// About this site resources
// Page insights logo is used for Google branded builds only.
#if BUILDFLAG(PAGE_INFO_USE_INTERNAL_ANDROID_RESOURCES)
DECLARE_RESOURCE_ID(IDR_ANDROID_ABOUT_THIS_SITE_LOGO_24DP,
                    R.drawable.ic_page_insights_logo_24dp)
#else
DECLARE_RESOURCE_ID(IDR_ANDROID_ABOUT_THIS_SITE_LOGO_24DP,
                    R.drawable.ic_info_outline_grey_24dp)
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING) && BUILDFLAG(IS_ANDROID)
