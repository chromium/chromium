// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_CONSTANTS_URL_CONSTANTS_H_
#define ASH_CONSTANTS_URL_CONSTANTS_H_

// Contains constants for known URLs for ash components. URL constants for
// ash-chrome should be in chrome/common/url_contants.h.
//
// - Keep the constants sorted by name within its section.
// - Use the same order in this header and ash_url_constants.cc.

namespace chrome {

// URL for the Google Privacy Policy.
inline constexpr char kGooglePrivacyPolicyUrl[] =
    "https://policies.google.com/privacy";

// The URL for the "Learn more" link for Android Messages.
inline constexpr char kAndroidMessagesLearnMoreURL[] =
    "https://support.google.com/chromebook/?p=multi_device_messages";

// "Learn more" URL for APN settings.
inline constexpr char16_t kApnSettingsLearnMoreUrl[] =
    u"https://support.google.com/chromebook?p=apn";

// "Learn more" URL for Help Me Read and Help Me Write feature on ChromeOS.
inline constexpr char kHelpMeReadWriteLearnMoreURL[] =
    "https://support.google.com/chromebook/?p=settings_help_me_read_write";

// "Learn more" URL for Lobster feature on ChromeOS.
inline constexpr char kLobsterLearnMoreURL[] =
    "https://support.google.com/chromebook?p=dugong2";

// "Learn more" URL for Scanner feature on ChromeOS.
inline constexpr char kScannerLearnMoreUrl[] =
    "https://support.google.com/chromebook?p=dugong3";

// The URL for additional help that is given when Linux export/import fails.
inline constexpr char kLinuxExportImportHelpURL[] =
    "https://support.google.com/chromebook?p=linux_backup_restore";

// The URL for the "Learn more" link in the connected devices.
inline constexpr char kMultiDeviceLearnMoreURL[] =
    "https://support.google.com/chromebook/?p=multi_device";

inline constexpr char kSafetyPixelbookURL[] = "https://g.co/Pixelbook/legal";
inline constexpr char kSafetyPixelSlateURL[] = "https://g.co/PixelSlate/legal";

}  // namespace chrome

#endif  // ASH_CONSTANTS_URL_CONSTANTS_H_
