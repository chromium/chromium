// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_CONSTANTS_URL_CONSTANTS_H_
#define ASH_CONSTANTS_URL_CONSTANTS_H_

// Contains constants for known URLs for OS system parts of chromeos-chrome.
//
// - Keep the constants sorted by name within its section.

namespace ash::external_urls {

// URL for the Google Privacy Policy.
inline constexpr char kGooglePrivacyPolicyUrl[] =
    "https://policies.google.com/privacy";

// Accessibility help link.
inline constexpr char kAccessibilityHelpURL[] =
    "https://support.google.com/chromebook/topic/6323347";

// The URL for the "Learn more" link for Android Messages.
inline constexpr char kAndroidMessagesLearnMoreURL[] =
    "https://support.google.com/chromebook/?p=multi_device_messages";

// "Learn more" URL for APN settings.
inline constexpr char16_t kApnSettingsLearnMoreUrl[] =
    u"https://support.google.com/chromebook?p=apn";

// The URL path to online ARC++ terms of service.
inline constexpr char kArcTosOnlineURLPath[] =
    "https://play.google/play-terms/embedded/";

// The URL for Auto Update Policy.
inline constexpr char16_t kAutoUpdatePolicyURL[] =
    u"https://support.google.com/chrome/a?p=auto-update-policy";

// The URL path to Online Chrome and Chrome OS terms of service.
inline constexpr char kCrosEulaOnlineURLPath[] =
    "https://www.google.com/intl/%s/chrome/terms/";

// The URL for the learn more link about extended automatic updates for
// ChromeOS devices.
inline constexpr char16_t kDeviceExtendedUpdatesLearnMoreURL[] =
    u"https://www.google.com/chromebook/autoupdates-opt-in/";

// The URL for EOL notification
inline constexpr char16_t kEolNotificationURL[] =
    u"https://www.google.com/chromebook/older/";

// The URL path to Online Google EULA.
inline constexpr char kGoogleEulaOnlineURLPath[] =
    "https://policies.google.com/terms/embedded?hl=%s";

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

// The URL for the "Learn more" page for the Location Accuracy setting under the
// privacy hub location subpage.
// TODO(crbug.com/485470549): migrate with
// ash/webui/personalization_app/personalization_app_url_constants.cc
inline constexpr char16_t kPrivacyHubGeolocationAccuracyLearnMoreURL[] =
    u"https://support.google.com/android/?p=location_accuracy";

inline constexpr char kSafetyPixelbookURL[] = "https://g.co/Pixelbook/legal";
inline constexpr char kSafetyPixelSlateURL[] = "https://g.co/PixelSlate/legal";

}  // namespace ash::external_urls

#endif  // ASH_CONSTANTS_URL_CONSTANTS_H_
