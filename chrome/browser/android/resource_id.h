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
LINK_RESOURCE_ID(IDR_INFOBAR_3D_BLOCKED, R.drawable.infobar_3d_blocked)
LINK_RESOURCE_ID(IDR_INFOBAR_AUTOFILL_CC, R.drawable.infobar_autofill_cc)

// Android only infobars.
DECLARE_RESOURCE_ID(IDR_ANDROID_INFOBAR_ACCESSIBILITY_EVENTS,
                    R.drawable.infobar_accessibility_events)
DECLARE_RESOURCE_ID(IDR_ANDROID_INFOBAR_BLOCKED_POPUPS,
                    R.drawable.infobar_blocked_popups)
DECLARE_RESOURCE_ID(IDR_ANDROID_INFOBAR_CLIPBOARD,
                    R.drawable.infobar_clipboard)
DECLARE_RESOURCE_ID(IDR_ANDROID_INFOBAR_FOLDER, R.drawable.ic_folder_blue_24dp)
DECLARE_RESOURCE_ID(IDR_ANDROID_INFOBAR_FROZEN_TAB, R.drawable.infobar_restore)
DECLARE_RESOURCE_ID(IDR_ANDROID_INFOBAR_GEOLOCATION,
                    R.drawable.infobar_geolocation)
DECLARE_RESOURCE_ID(IDR_ANDROID_INFOBAR_MEDIA_STREAM_CAMERA,
                    R.drawable.infobar_camera)
DECLARE_RESOURCE_ID(IDR_ANDROID_INFOBAR_MEDIA_STREAM_MIC,
                    R.drawable.infobar_microphone)
DECLARE_RESOURCE_ID(IDR_ANDROID_INFOBAR_MEDIA_STREAM_SCREEN,
                    R.drawable.infobar_screen_share)
DECLARE_RESOURCE_ID(IDR_ANDROID_INFOBAR_MIDI, R.drawable.infobar_midi)
DECLARE_RESOURCE_ID(IDR_ANDROID_INFOBAR_MULTIPLE_DOWNLOADS,
                    R.drawable.infobar_multiple_downloads)
DECLARE_RESOURCE_ID(IDR_ANDROID_INFOBAR_NOTIFICATIONS,
                    R.drawable.infobar_desktop_notifications)
DECLARE_RESOURCE_ID(IDR_ANDROID_INFOBAR_PREVIEWS,
                    R.drawable.infobar_chrome)
DECLARE_RESOURCE_ID(IDR_ANDROID_INFOBAR_PROTECTED_MEDIA_IDENTIFIER,
                    R.drawable.infobar_protected_media_identifier)
DECLARE_RESOURCE_ID(IDR_ANDROID_INFOBAR_SAVE_PASSWORD,
                    R.drawable.ic_vpn_key_blue)
DECLARE_RESOURCE_ID(IDR_ANDROID_INFOBAR_TRANSLATE, R.drawable.infobar_translate)
DECLARE_RESOURCE_ID(IDR_ANDROID_INFOBAR_WARNING, R.drawable.infobar_warning)
LINK_RESOURCE_ID(IDR_AUTOFILL_GOOGLE_PAY_WITH_DIVIDER,
                 R.drawable.google_pay_with_divider)

// PageInfoUI images, used in ConnectionInfoPopup
// Good:
DECLARE_RESOURCE_ID(IDR_PAGEINFO_GOOD, R.drawable.pageinfo_good)
// Warnings:
DECLARE_RESOURCE_ID(IDR_PAGEINFO_WARNING_MINOR, R.drawable.pageinfo_warning)
// Bad:
DECLARE_RESOURCE_ID(IDR_PAGEINFO_BAD, R.drawable.pageinfo_bad)
// Should never occur, use warning just in case:
// Enterprise managed: ChromeOS only.
DECLARE_RESOURCE_ID(IDR_PAGEINFO_ENTERPRISE_MANAGED,
                    R.drawable.pageinfo_warning)
// Info: Only shown on chrome:// urls, which don't show the connection info
// popup.
DECLARE_RESOURCE_ID(IDR_PAGEINFO_INFO, R.drawable.pageinfo_warning)
// Major warning: Used on insecure pages, which don't show the connection info
// popup.
DECLARE_RESOURCE_ID(IDR_PAGEINFO_WARNING_MAJOR, R.drawable.pageinfo_warning)

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
LINK_RESOURCE_ID(IDR_AUTOFILL_CC_UNIONPAY, R.drawable.unionpay_card)
LINK_RESOURCE_ID(IDR_AUTOFILL_CC_VISA, R.drawable.visa_card)
LINK_RESOURCE_ID(IDR_AUTOFILL_CC_SCAN_NEW, R.drawable.ic_photo_camera_black)
LINK_RESOURCE_ID(IDR_AUTOFILL_GOOGLE_PAY, R.drawable.google_pay)
LINK_RESOURCE_ID(IDR_AUTOFILL_HTTP_WARNING, R.drawable.ic_info_outline_grey)
LINK_RESOURCE_ID(IDR_AUTOFILL_HTTPS_INVALID_WARNING, R.drawable.ic_warning_red)

// We display settings and edit icon for keyboard accessory using Android's
// |VectorDrawableCompat|. We do not display these icons for autofill popup.
LINK_RESOURCE_ID(IDR_AUTOFILL_SETTINGS, R.drawable.ic_settings_black)
LINK_RESOURCE_ID(IDR_AUTOFILL_CREATE, R.drawable.ic_edit_24dp)

// We use PNG files for the following images.
LINK_RESOURCE_ID(IDR_CREDIT_CARD_CVC_HINT, R.drawable.cvc_icon)
LINK_RESOURCE_ID(IDR_CREDIT_CARD_CVC_HINT_AMEX, R.drawable.cvc_icon_amex)
