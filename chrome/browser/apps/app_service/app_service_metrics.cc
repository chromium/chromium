// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/app_service_metrics.h"

#include "ash/public/cpp/app_list/internal_app_id_constants.h"
#include "base/metrics/histogram_functions.h"
#include "base/time/time.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/chromeos/file_manager/app_id.h"
#include "chrome/browser/web_applications/components/web_app_id_constants.h"
#include "chrome/common/extensions/extension_constants.h"
#include "components/services/app_service/public/cpp/app_update.h"
#include "components/services/app_service/public/mojom/app_service.mojom.h"
#include "extensions/common/constants.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/plugin_vm/plugin_vm_util.h"
#include "chrome/browser/ui/app_list/arc/arc_app_utils.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

namespace {

// The default app's histogram name. This is used for logging so do
// not change the order of this enum.
// https://docs.google.com/document/d/1WJ-BjlVOM87ygIsdDBCyXxdKw3iS5EtNGm1fWiWhfIs
// If you're adding to this enum with the intention that it will be logged,
// update the DefaultAppName enum listing in tools/metrics/histograms/enums.xml.
enum class DefaultAppName {
  kCalculator = 10,
  kText = 11,
  kGetHelp = 12,
  kGallery = 13,
  kVideoPlayer = 14,
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
  kStadia = 40,
  kScanningApp = 41,
  kDiagnosticsApp = 42,
  kPrintManagementApp = 43,

  // Add any new values above this one, and update kMaxValue to the highest
  // enumerator value.
  kMaxValue = kPrintManagementApp,
};

void RecordDefaultAppLaunch(DefaultAppName default_app_name,
                            apps::mojom::LaunchSource launch_source) {
  switch (launch_source) {
    case apps::mojom::LaunchSource::kUnknown:
    case apps::mojom::LaunchSource::kFromParentalControls:
    case apps::mojom::LaunchSource::kFromTest:
      return;
    case apps::mojom::LaunchSource::kFromAppListGrid:
      base::UmaHistogramEnumeration("Apps.DefaultAppLaunch.FromAppListGrid",
                                    default_app_name);
      break;
    case apps::mojom::LaunchSource::kFromAppListGridContextMenu:
      base::UmaHistogramEnumeration(
          "Apps.DefaultAppLaunch.FromAppListGridContextMenu", default_app_name);
      break;
    case apps::mojom::LaunchSource::kFromAppListQuery:
      base::UmaHistogramEnumeration("Apps.DefaultAppLaunch.FromAppListQuery",
                                    default_app_name);
      break;
    case apps::mojom::LaunchSource::kFromAppListQueryContextMenu:
      base::UmaHistogramEnumeration(
          "Apps.DefaultAppLaunch.FromAppListQueryContextMenu",
          default_app_name);
      break;
    case apps::mojom::LaunchSource::kFromAppListRecommendation:
      base::UmaHistogramEnumeration(
          "Apps.DefaultAppLaunch.FromAppListRecommendation", default_app_name);
      break;
    case apps::mojom::LaunchSource::kFromShelf:
      base::UmaHistogramEnumeration("Apps.DefaultAppLaunch.FromShelf",
                                    default_app_name);
      break;
    case apps::mojom::LaunchSource::kFromFileManager:
      base::UmaHistogramEnumeration("Apps.DefaultAppLaunch.FromFileManager",
                                    default_app_name);
      break;
    case apps::mojom::LaunchSource::kFromLink:
      base::UmaHistogramEnumeration("Apps.DefaultAppLaunch.FromLink",
                                    default_app_name);
      break;
    case apps::mojom::LaunchSource::kFromOmnibox:
      base::UmaHistogramEnumeration("Apps.DefaultAppLaunch.FromOmnibox",
                                    default_app_name);
      break;
    case apps::mojom::LaunchSource::kFromChromeInternal:
      base::UmaHistogramEnumeration("Apps.DefaultAppLaunch.FromChromeInternal",
                                    default_app_name);
      break;
    case apps::mojom::LaunchSource::kFromKeyboard:
      base::UmaHistogramEnumeration("Apps.DefaultAppLaunch.FromKeyboard",
                                    default_app_name);
      break;
    case apps::mojom::LaunchSource::kFromOtherApp:
      base::UmaHistogramEnumeration("Apps.DefaultAppLaunch.FromOtherApp",
                                    default_app_name);
      break;
    case apps::mojom::LaunchSource::kFromMenu:
      base::UmaHistogramEnumeration("Apps.DefaultAppLaunch.FromMenu",
                                    default_app_name);
      break;
    case apps::mojom::LaunchSource::kFromInstalledNotification:
      base::UmaHistogramEnumeration(
          "Apps.DefaultAppLaunch.FromInstalledNotification", default_app_name);
      break;
    case apps::mojom::LaunchSource::kFromArc:
      base::UmaHistogramEnumeration("Apps.DefaultAppLaunch.FromArc",
                                    default_app_name);
      break;
    case apps::mojom::LaunchSource::kFromSharesheet:
      base::UmaHistogramEnumeration("Apps.DefaultAppLaunch.FromSharesheet",
                                    default_app_name);
      break;
    case apps::mojom::LaunchSource::kFromReleaseNotesNotification:
      base::UmaHistogramEnumeration(
          "Apps.DefaultAppLaunch.FromReleaseNotesNotification",
          default_app_name);
      break;
    case apps::mojom::LaunchSource::kFromFullRestore:
      base::UmaHistogramEnumeration("Apps.DefaultAppLaunch.FromFullRestore",
                                    default_app_name);
      break;
    case apps::mojom::LaunchSource::kFromSmartTextContextMenu:
      base::UmaHistogramEnumeration(
          "Apps.DefaultAppLaunch.FromSmartTextContextMenu", default_app_name);
      break;
  }
}

void RecordBuiltInAppLaunch(apps::BuiltInAppName built_in_app_name,
                            apps::mojom::LaunchSource launch_source) {
  switch (launch_source) {
    case apps::mojom::LaunchSource::kUnknown:
    case apps::mojom::LaunchSource::kFromParentalControls:
      break;
    case apps::mojom::LaunchSource::kFromAppListGrid:
    case apps::mojom::LaunchSource::kFromAppListGridContextMenu:
      base::UmaHistogramEnumeration("Apps.AppListInternalApp.Activate",
                                    built_in_app_name);
      break;
    case apps::mojom::LaunchSource::kFromAppListQuery:
    case apps::mojom::LaunchSource::kFromAppListQueryContextMenu:
    case apps::mojom::LaunchSource::kFromAppListRecommendation:
      base::UmaHistogramEnumeration("Apps.AppListSearchResultInternalApp.Open",
                                    built_in_app_name);
      break;
    case apps::mojom::LaunchSource::kFromShelf:
    case apps::mojom::LaunchSource::kFromFileManager:
    case apps::mojom::LaunchSource::kFromLink:
    case apps::mojom::LaunchSource::kFromOmnibox:
    case apps::mojom::LaunchSource::kFromChromeInternal:
    case apps::mojom::LaunchSource::kFromKeyboard:
    case apps::mojom::LaunchSource::kFromOtherApp:
    case apps::mojom::LaunchSource::kFromMenu:
    case apps::mojom::LaunchSource::kFromInstalledNotification:
    case apps::mojom::LaunchSource::kFromTest:
    case apps::mojom::LaunchSource::kFromArc:
    case apps::mojom::LaunchSource::kFromSharesheet:
    case apps::mojom::LaunchSource::kFromReleaseNotesNotification:
    case apps::mojom::LaunchSource::kFromFullRestore:
    case apps::mojom::LaunchSource::kFromSmartTextContextMenu:
      break;
  }
}

}  // namespace

namespace apps {

void RecordAppLaunch(const std::string& app_id,
                     apps::mojom::LaunchSource launch_source) {
  if (app_id == extension_misc::kCalculatorAppId)
    RecordDefaultAppLaunch(DefaultAppName::kCalculator, launch_source);
  else if (app_id == extension_misc::kTextEditorAppId)
    RecordDefaultAppLaunch(DefaultAppName::kText, launch_source);
  else if (app_id == file_manager::kGalleryAppId)
    RecordDefaultAppLaunch(DefaultAppName::kGallery, launch_source);
  else if (app_id == file_manager::kVideoPlayerAppId)
    RecordDefaultAppLaunch(DefaultAppName::kVideoPlayer, launch_source);
  else if (app_id == file_manager::kAudioPlayerAppId)
    RecordDefaultAppLaunch(DefaultAppName::kAudioPlayer, launch_source);
  else if (app_id == web_app::kCanvasAppId)
    RecordDefaultAppLaunch(DefaultAppName::kChromeCanvas, launch_source);
  else if (app_id == extension_misc::kCameraAppId)
    RecordDefaultAppLaunch(DefaultAppName::kCamera, launch_source);
  else if (app_id == web_app::kCameraAppId)
    RecordDefaultAppLaunch(DefaultAppName::kCamera, launch_source);
  else if (app_id == web_app::kHelpAppId)
    RecordDefaultAppLaunch(DefaultAppName::kHelpApp, launch_source);
  else if (app_id == web_app::kMediaAppId)
    RecordDefaultAppLaunch(DefaultAppName::kMediaApp, launch_source);
  else if (app_id == extension_misc::kChromeAppId)
    RecordDefaultAppLaunch(DefaultAppName::kChrome, launch_source);
  else if (app_id == extension_misc::kGoogleDocAppId)
    RecordDefaultAppLaunch(DefaultAppName::kDocs, launch_source);
  else if (app_id == extension_misc::kDriveHostedAppId)
    RecordDefaultAppLaunch(DefaultAppName::kDrive, launch_source);
#if BUILDFLAG(IS_CHROMEOS_ASH)
  else if (app_id == arc::kGoogleDuoAppId)
    RecordDefaultAppLaunch(DefaultAppName::kDuo, launch_source);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
  else if (app_id == extension_misc::kFilesManagerAppId)
    RecordDefaultAppLaunch(DefaultAppName::kFiles, launch_source);
#if BUILDFLAG(IS_CHROMEOS_ASH)
  else if (app_id == extension_misc::kGmailAppId || app_id == arc::kGmailAppId)
    RecordDefaultAppLaunch(DefaultAppName::kGmail, launch_source);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
  else if (app_id == extension_misc::kGoogleKeepAppId)
    RecordDefaultAppLaunch(DefaultAppName::kKeep, launch_source);
#if BUILDFLAG(IS_CHROMEOS_ASH)
  else if (app_id == extension_misc::kGooglePhotosAppId)
    RecordDefaultAppLaunch(DefaultAppName::kPhotos, launch_source);
  else if (app_id == arc::kPlayBooksAppId)
    RecordDefaultAppLaunch(DefaultAppName::kPlayBooks, launch_source);
  else if (app_id == arc::kPlayGamesAppId)
    RecordDefaultAppLaunch(DefaultAppName::kPlayGames, launch_source);
  else if (app_id == arc::kPlayMoviesAppId ||
           app_id == extension_misc::kGooglePlayMoviesAppId)
    RecordDefaultAppLaunch(DefaultAppName::kPlayMovies, launch_source);
  else if (app_id == arc::kPlayMusicAppId ||
           app_id == extension_misc::kGooglePlayMusicAppId)
    RecordDefaultAppLaunch(DefaultAppName::kPlayMusic, launch_source);
  else if (app_id == arc::kPlayStoreAppId)
    RecordDefaultAppLaunch(DefaultAppName::kPlayStore, launch_source);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
  else if (app_id == web_app::kOsSettingsAppId)
    RecordDefaultAppLaunch(DefaultAppName::kSettings, launch_source);
  else if (app_id == extension_misc::kGoogleSheetsAppId)
    RecordDefaultAppLaunch(DefaultAppName::kSheets, launch_source);
  else if (app_id == extension_misc::kGoogleSlidesAppId)
    RecordDefaultAppLaunch(DefaultAppName::kSlides, launch_source);
  else if (app_id == extensions::kWebStoreAppId)
    RecordDefaultAppLaunch(DefaultAppName::kWebStore, launch_source);
#if BUILDFLAG(IS_CHROMEOS_ASH)
  else if (app_id == extension_misc::kYoutubeAppId ||
           app_id == arc::kYoutubeAppId)
    RecordDefaultAppLaunch(DefaultAppName::kYouTube, launch_source);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
  else if (app_id == web_app::kYoutubeMusicAppId)
    RecordDefaultAppLaunch(DefaultAppName::kYouTubeMusic, launch_source);
  else if (app_id == web_app::kStadiaAppId)
    RecordDefaultAppLaunch(DefaultAppName::kStadia, launch_source);
#if BUILDFLAG(IS_CHROMEOS_ASH)
  else if (app_id == web_app::kScanningAppId)
    RecordDefaultAppLaunch(DefaultAppName::kScanningApp, launch_source);
  else if (app_id == web_app::kDiagnosticsAppId)
    RecordDefaultAppLaunch(DefaultAppName::kDiagnosticsApp, launch_source);
  else if (app_id == web_app::kPrintManagementAppId)
    RecordDefaultAppLaunch(DefaultAppName::kPrintManagementApp, launch_source);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  // Above are default apps; below are built-in apps.

  if (app_id == ash::kInternalAppIdKeyboardShortcutViewer) {
    RecordBuiltInAppLaunch(BuiltInAppName::kKeyboardShortcutViewer,
                           launch_source);
  } else if (app_id == ash::kInternalAppIdSettings) {
    RecordBuiltInAppLaunch(BuiltInAppName::kSettings, launch_source);
  } else if (app_id == ash::kInternalAppIdContinueReading) {
    RecordBuiltInAppLaunch(BuiltInAppName::kContinueReading, launch_source);
#if BUILDFLAG(IS_CHROMEOS_ASH)
  } else if (app_id == plugin_vm::kPluginVmShelfAppId) {
    RecordBuiltInAppLaunch(BuiltInAppName::kPluginVm, launch_source);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
  } else if (app_id == web_app::kMockSystemAppId) {
    RecordDefaultAppLaunch(DefaultAppName::kMockSystemApp, launch_source);
  }
}

void RecordBuiltInAppSearchResult(const std::string& app_id) {
  if (app_id == ash::kInternalAppIdKeyboardShortcutViewer) {
    base::UmaHistogramEnumeration("Apps.AppListSearchResultInternalApp.Show",
                                  BuiltInAppName::kKeyboardShortcutViewer);
  } else if (app_id == ash::kInternalAppIdSettings) {
    base::UmaHistogramEnumeration("Apps.AppListSearchResultInternalApp.Show",
                                  BuiltInAppName::kSettings);
  } else if (app_id == ash::kInternalAppIdContinueReading) {
    base::UmaHistogramEnumeration("Apps.AppListSearchResultInternalApp.Show",
                                  BuiltInAppName::kContinueReading);
#if BUILDFLAG(IS_CHROMEOS_ASH)
  } else if (app_id == plugin_vm::kPluginVmShelfAppId) {
    base::UmaHistogramEnumeration("Apps.AppListSearchResultInternalApp.Show",
                                  BuiltInAppName::kPluginVm);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
  }
}

void RecordAppBounce(const apps::AppUpdate& app) {
  base::Time install_time = app.InstallTime();
  base::Time uninstall_time = base::Time::Now();

  DCHECK(uninstall_time >= install_time);

  base::TimeDelta amount_time_installed = uninstall_time - install_time;

  const base::TimeDelta seven_days = base::TimeDelta::FromDays(7);

  if (amount_time_installed < seven_days) {
    base::UmaHistogramBoolean("Apps.Bounced", true);
  } else {
    base::UmaHistogramBoolean("Apps.Bounced", false);
  }
}

void RecordAppsPerNotification(int count) {
  if (count <= 0) {
    return;
  }
  base::UmaHistogramBoolean("ChromeOS.Apps.NumberOfAppsForNotification",
                            (count > 1));
}

}  // namespace apps
