// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_APP_SERVICE_METRICS_APP_SERVICE_METRICS_H_
#define CHROME_BROWSER_APPS_APP_SERVICE_METRICS_APP_SERVICE_METRICS_H_

#include <map>
#include <string>

#include "build/chromeos_buildflags.h"
#include "components/services/app_service/public/cpp/app_launch_util.h"

namespace apps {

// The default app's histogram name. This is used for logging so do
// not change the order of this enum.
// https://docs.google.com/document/d/1WJ-BjlVOM87ygIsdDBCyXxdKw3iS5EtNGm1fWiWhfIs
// If you're adding to this enum with the intention that it will be logged,
// update the DefaultAppName enum listing in tools/metrics/histograms/enums.xml.
enum class DefaultAppName {
  // Legacy calculator chrome app was replaced by a PWA in m96.
  kCalculatorChromeApp = 10,
  kText = 11,
  kGetHelp = 12,
  // Gallery was replaced by MediaApp in M86 and deleted in M91.
  kDeletedGalleryChromeApp = 13,
  // VideoPlayer was replaced by MediaApp in M93 and deleted in M96.
  kDeletedVideoPlayerChromeApp = 14,
  kAudioPlayer = 15,
  kChromeCanvas = 16,
  kCamera = 17,
  kHelpApp = 18,
  kMediaApp = 19,
  kChrome = 20,
  kDocs = 21,
  kDrive = 22,
  kDuo = 23,
  kFiles = 24,
  kGmail = 25,
  kKeep = 26,
  kPhotos = 27,
  kPlayBooks = 28,
  kPlayGames = 29,
  kPlayMovies = 30,
  kPlayMusic = 31,
  kPlayStore = 32,
  kSettings = 33,
  kSheets = 34,
  kSlides = 35,
  kWebStore = 36,
  kYouTube = 37,
  kYouTubeMusic = 38,
  // This is our test SWA. It's only installed in tests.
  kMockSystemApp = 39,
  // Stadia was removed from the web app definitions in M112.
  kDeletedStadia = 40,
  kScanningApp = 41,
  kDiagnosticsApp = 42,
  kPrintManagementApp = 43,
  kShortcutCustomizationApp = 44,
  kShimlessRMAApp = 45,
  kOsFeedbackApp = 46,
  kCursive = 47,
  // MediaAppAudio is scheduled to be absorbed into MediaApp in M97.
  kDeletedMediaAppAudio = 48,
  kProjector = 49,
  kCalculator = 50,
  kFirmwareUpdateApp = 51,
  kGoogleTv = 52,
  kGoogleCalendar = 53,
  kGoogleChat = 54,
  kGoogleMeet = 55,
  kGoogleMaps = 56,
  kGoogleMessages = 57,
  kContainer = 58,
  kMall = 59,
  kSanitizeApp = 60,
  // Add any new values above this one, and update kMaxValue to the highest
  // enumerator value.
  kMaxValue = kSanitizeApp,
};

// The built-in app's histogram name. This is used for logging so do not change
// the order of this enum.
enum class BuiltInAppName {
  kKeyboardShortcutViewer = 0,
  kSettings = 1,
  // kContinueReading = 2, obsolete
  kCameraDeprecated = 3,
  // kDiscover = 4, obsolete
  kPluginVm = 5,
  kReleaseNotes = 6,
  kMaxValue = kReleaseNotes,
};

// Converts an app ID to the corresponding `DefaultAppName`, or nullopt if
// it doesn't match a known ID.
std::optional<apps::DefaultAppName> AppIdToName(const std::string& app_id);

void RecordAppLaunch(const std::string& app_id,
                     apps::LaunchSource launch_source);

// Converts a preinstalled web app ID to the corresponding `DefaultAppName`, or
// nullopt if it doesn't match a known ID.
const std::optional<apps::DefaultAppName> PreinstalledWebAppIdToName(
    const std::string& app_id);

#if BUILDFLAG(IS_CHROMEOS_ASH)
// Converts a system web app ID to the corresponding `DefaultAppName`, or
// nullopt if it doesn't match a known ID.
const std::optional<apps::DefaultAppName> SystemWebAppIdToName(
    const std::string& app_id);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

}  // namespace apps

#endif  // CHROME_BROWSER_APPS_APP_SERVICE_METRICS_APP_SERVICE_METRICS_H_
