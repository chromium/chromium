// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_CONSTANTS_CHROME_WEBUI_URL_CONSTANTS_H_
#define ASH_CONSTANTS_CHROME_WEBUI_URL_CONSTANTS_H_

// Defines URL/host constants that Chrome as a web browser provides.
namespace ash::chrome_urls {

// Web UI URLs that Chrome as a web browser provides. Please keep the entries
// in the lexicographical order.
// These need to be consistent with ones that Chrome provides.
// See //chrome/common/webui_url_constants.cc for the check.

// "Chrome Settings" URL for website notifications linked out from OSSettings.
inline constexpr char kChromeUIAppNotificationsBrowserSettingsURL[] =
    "chrome://settings/content/notifications";

// "Chrome Settings" URL for website camera access permissions.
inline constexpr char kChromeUICameraPermissionsSettingsURL[] =
    "chrome://settings/content/camera";

inline constexpr char kChromeUICertificateManagerDialogURL[] =
    "chrome://certificate-manager";
inline constexpr char kChromeUIFlagsURL[] = "chrome://flags/";
inline constexpr char kChromeUIFeedbackURL[] = "chrome://feedback/";
inline constexpr char kChromeUIHistogramsURL[] = "chrome://histograms";

// "Chrome Settings" URL for website location access permissions.
inline constexpr char kChromeUILocationPermissionsSettingsURL[] =
    "chrome://settings/content/location";

inline constexpr char16_t kChromeUIManagementURL16[] = u"chrome://management";

// "Chrome Settings" URL for website microphone access permissions.
inline constexpr char kChromeUIMicrophonePermissionsSettingsURL[] =
    "chrome://settings/content/microphone";

inline constexpr char kChromeUINewTabURL[] = "chrome://newtab/";
inline constexpr char kChromeUISettingsHost[] = "settings";
inline constexpr char kChromeUISettingsURL[] = "chrome://settings/";
inline constexpr char kChromeUITermsHost[] = "terms";
inline constexpr char kChromeUITermsURL[] = "chrome://terms/";

// Settings subpages that Chrome as a web browser provides. Please keep the
// entries in the lexicographical order.
// These need to be consistent with ones that Chrome provides.
// See //chrome/common/webui_url_constants.cc for the check.
inline constexpr char kAppearanceSubPage[] = "appearance";
inline constexpr char kAutofillSubPage[] = "autofill";
inline constexpr char kClearBrowserDataSubPage[] = "clearBrowserData";
inline constexpr char kDownloadsSubPage[] = "downloads";
inline constexpr char kLanguagesSubPage[] = "languages/details";
inline constexpr char kOnStartupSubPage[] = "onStartup";
inline constexpr char kPasswordManagerSubPage[] = "passwords";
inline constexpr char kPrivacySubPage[] = "privacy";
inline constexpr char kResetSubPage[] = "reset";
inline constexpr char kSearchSubPage[] = "search";
inline constexpr char kSyncSetupSubPage[] = "syncSetup";

}  // namespace ash::chrome_urls

#endif  // ASH_CONSTANTS_CHROME_WEBUI_URL_CONSTANTS_H_
