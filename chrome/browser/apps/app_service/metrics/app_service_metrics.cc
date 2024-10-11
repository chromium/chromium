// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/metrics/app_service_metrics.h"

#include "ash/constants/web_app_id_constants.h"
#include "ash/webui/mall/app_id.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/time/time.h"
#include "build/branding_buildflags.h"
#include "build/chromeos_buildflags.h"
#include "chrome/common/extensions/extension_constants.h"
#include "components/app_constants/constants.h"
#include "components/services/app_service/public/cpp/app_launch_util.h"
#include "extensions/common/constants.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/components/arc/app/arc_app_constants.h"
#include "ash/constants/ash_features.h"
#include "ash/public/cpp/app_list/internal_app_id_constants.h"
#include "ash/user_education/user_education_util.h"
#include "ash/user_education/welcome_tour/welcome_tour_metrics.h"
#include "ash/webui/projector_app/public/cpp/projector_app_constants.h"
#include "chrome/browser/ash/app_list/arc/arc_app_utils.h"
#include "chrome/browser/ash/plugin_vm/plugin_vm_util.h"
#include "chromeos/ash/components/file_manager/app_id.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

namespace {

void RecordDefaultAppLaunch(apps::DefaultAppName default_app_name,
                            apps::LaunchSource launch_source) {
  switch (launch_source) {
    case apps::LaunchSource::kUnknown:
    case apps::LaunchSource::kFromParentalControls:
    case apps::LaunchSource::kFromTest:
      return;
    case apps::LaunchSource::kFromAppListGrid:
      base::UmaHistogramEnumeration("Apps.DefaultAppLaunch.FromAppListGrid",
                                    default_app_name);
      break;
    case apps::LaunchSource::kFromAppListGridContextMenu:
      base::UmaHistogramEnumeration(
          "Apps.DefaultAppLaunch.FromAppListGridContextMenu", default_app_name);
      break;
    case apps::LaunchSource::kFromAppListQuery:
      base::UmaHistogramEnumeration("Apps.DefaultAppLaunch.FromAppListQuery",
                                    default_app_name);
      break;
    case apps::LaunchSource::kFromAppListQueryContextMenu:
      base::UmaHistogramEnumeration(
          "Apps.DefaultAppLaunch.FromAppListQueryContextMenu",
          default_app_name);
      break;
    case apps::LaunchSource::kFromAppListRecommendation:
      base::UmaHistogramEnumeration(
          "Apps.DefaultAppLaunch.FromAppListRecommendation", default_app_name);
      break;
    case apps::LaunchSource::kFromShelf:
      base::UmaHistogramEnumeration("Apps.DefaultAppLaunch.FromShelf",
                                    default_app_name);
      break;
    case apps::LaunchSource::kFromFileManager:
      base::UmaHistogramEnumeration("Apps.DefaultAppLaunch.FromFileManager",
                                    default_app_name);
      break;
    case apps::LaunchSource::kFromLink:
      base::UmaHistogramEnumeration("Apps.DefaultAppLaunch.FromLink",
                                    default_app_name);
      break;
    case apps::LaunchSource::kFromOmnibox:
      base::UmaHistogramEnumeration("Apps.DefaultAppLaunch.FromOmnibox",
                                    default_app_name);
      break;
    case apps::LaunchSource::kFromChromeInternal:
      base::UmaHistogramEnumeration("Apps.DefaultAppLaunch.FromChromeInternal",
                                    default_app_name);
      break;
    case apps::LaunchSource::kFromKeyboard:
      base::UmaHistogramEnumeration("Apps.DefaultAppLaunch.FromKeyboard",
                                    default_app_name);
      break;
    case apps::LaunchSource::kFromOtherApp:
      base::UmaHistogramEnumeration("Apps.DefaultAppLaunch.FromOtherApp",
                                    default_app_name);
      break;
    case apps::LaunchSource::kFromMenu:
      base::UmaHistogramEnumeration("Apps.DefaultAppLaunch.FromMenu",
                                    default_app_name);
      break;
    case apps::LaunchSource::kFromInstalledNotification:
      base::UmaHistogramEnumeration(
          "Apps.DefaultAppLaunch.FromInstalledNotification", default_app_name);
      break;
    case apps::LaunchSource::kFromArc:
      base::UmaHistogramEnumeration("Apps.DefaultAppLaunch.FromArc",
                                    default_app_name);
      break;
    case apps::LaunchSource::kFromSharesheet:
      base::UmaHistogramEnumeration("Apps.DefaultAppLaunch.FromSharesheet",
                                    default_app_name);
      break;
    case apps::LaunchSource::kFromReleaseNotesNotification:
      base::UmaHistogramEnumeration(
          "Apps.DefaultAppLaunch.FromReleaseNotesNotification",
          default_app_name);
      break;
    case apps::LaunchSource::kFromFullRestore:
      base::UmaHistogramEnumeration("Apps.DefaultAppLaunch.FromFullRestore",
                                    default_app_name);
      break;
    case apps::LaunchSource::kFromSmartTextContextMenu:
      base::UmaHistogramEnumeration(
          "Apps.DefaultAppLaunch.FromSmartTextContextMenu", default_app_name);
      break;
    case apps::LaunchSource::kFromDiscoverTabNotification:
      base::UmaHistogramEnumeration(
          "Apps.DefaultAppLaunch.FromDiscoverTabNotification",
          default_app_name);
      break;
    case apps::LaunchSource::kFromManagementApi:
      base::UmaHistogramEnumeration("Apps.DefaultAppLaunch.FromManagementApi",
                                    default_app_name);
      break;
    case apps::LaunchSource::kFromKiosk:
      base::UmaHistogramEnumeration("Apps.DefaultAppLaunch.FromKiosk",
                                    default_app_name);
      break;
    case apps::LaunchSource::kFromNewTabPage:
      base::UmaHistogramEnumeration("Apps.DefaultAppLaunch.FromNewTabPage",
                                    default_app_name);
      break;
    case apps::LaunchSource::kFromIntentUrl:
      base::UmaHistogramEnumeration("Apps.DefaultAppLaunch.FromIntentUrl",
                                    default_app_name);
      break;
    case apps::LaunchSource::kFromOsLogin:
      base::UmaHistogramEnumeration("Apps.DefaultAppLaunch.FromOsLogin",
                                    default_app_name);
      break;
    case apps::LaunchSource::kFromProtocolHandler:
      base::UmaHistogramEnumeration("Apps.DefaultAppLaunch.FromProtocolHandler",
                                    default_app_name);
      break;
    case apps::LaunchSource::kFromUrlHandler:
      base::UmaHistogramEnumeration("Apps.DefaultAppLaunch.FromUrlHandler",
                                    default_app_name);
      break;
    case apps::LaunchSource::kFromLockScreen:
      base::UmaHistogramEnumeration("Apps.DefaultAppLaunch.FromLockScreen",
                                    default_app_name);
      break;
    case apps::LaunchSource::kFromSysTrayCalendar:
      base::UmaHistogramEnumeration("Apps.DefaultAppLaunch.FromSysTrayCalendar",
                                    default_app_name);
      break;
    case apps::LaunchSource::kFromInstaller:
      base::UmaHistogramEnumeration("Apps.DefaultAppLaunch.FromInstaller",
                                    default_app_name);
      break;
    case apps::LaunchSource::kFromFirstRun:
      base::UmaHistogramEnumeration("Apps.DefaultAppLaunch.FromFirstRun",
                                    default_app_name);
      break;
    case apps::LaunchSource::kFromWelcomeTour:
      base::UmaHistogramEnumeration("Apps.DefaultAppLaunch.FromWelcomeTour",
                                    default_app_name);
      break;
    case apps::LaunchSource::kFromFocusMode:
      base::UmaHistogramEnumeration("Apps.DefaultAppLaunch.FromFocusMode",
                                    default_app_name);
      break;
    case apps::LaunchSource::kFromSparky:
      base::UmaHistogramEnumeration("Apps.DefaultAppLaunch.FromSparky",
                                    default_app_name);
      break;
    case apps::LaunchSource::kFromCommandLine:
    case apps::LaunchSource::kFromBackgroundMode:
    case apps::LaunchSource::kFromAppHomePage:
    case apps::LaunchSource::kFromReparenting:
    case apps::LaunchSource::kFromProfileMenu:
    case apps::LaunchSource::kFromNavigationCapturing:
      NOTREACHED_IN_MIGRATION();
      break;
  }
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
void RecordWelcomeTourInteraction(apps::DefaultAppName default_app_name,
                                  apps::LaunchSource launch_source) {
  // This metric is intended to capture actual user actions after the user
  // completed the Welcome Tour. Do not log automatically launched apps,
  // including apps that were automatically launched by the Welcome Tour.
  if (launch_source == apps::LaunchSource::kFromChromeInternal ||
      launch_source == apps::LaunchSource::kFromWelcomeTour) {
    return;
  }

  PrefService* prefs = ash::user_education_util::GetLastActiveUserPrefService();

  switch (default_app_name) {
    case apps::DefaultAppName::kFiles:
      ash::welcome_tour_metrics::RecordInteraction(
          prefs, ash::welcome_tour_metrics::Interaction::kFilesApp);
      break;
    case apps::DefaultAppName::kHelpApp:
      ash::welcome_tour_metrics::RecordInteraction(
          prefs, ash::welcome_tour_metrics::Interaction::kExploreApp);
      break;
    case apps::DefaultAppName::kSettings:
      ash::welcome_tour_metrics::RecordInteraction(
          prefs, ash::welcome_tour_metrics::Interaction::kSettingsApp);
      break;
    default:
      break;
  }
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

}  // namespace

namespace apps {

std::optional<apps::DefaultAppName> AppIdToName(const std::string& app_id) {
  if (const std::optional<DefaultAppName> app_name =
          PreinstalledWebAppIdToName(app_id)) {
    return app_name;
  }

#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (const std::optional<DefaultAppName> app_name =
          SystemWebAppIdToName(app_id)) {
    return app_name;
  }
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  if (app_id == extension_misc::kCalculatorAppId) {
    // The legacy calculator chrome app.
    return DefaultAppName::kCalculatorChromeApp;
  } else if (app_id == extension_misc::kTextEditorAppId) {
    return DefaultAppName::kText;
  } else if (app_id == app_constants::kChromeAppId) {
    return DefaultAppName::kChrome;
  } else if (app_id == extension_misc::kGoogleDocsAppId) {
    return DefaultAppName::kDocs;
  } else if (app_id == extension_misc::kGoogleDriveAppId) {
    return DefaultAppName::kDrive;
#if BUILDFLAG(IS_CHROMEOS_ASH)
  } else if (app_id == arc::kGoogleDuoAppId) {
    return DefaultAppName::kDuo;
  } else if (app_id == extension_misc::kFilesManagerAppId) {
    return DefaultAppName::kFiles;
  } else if (app_id == extension_misc::kGmailAppId ||
             app_id == arc::kGmailAppId) {
    return DefaultAppName::kGmail;
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
  } else if (app_id == extension_misc::kGoogleKeepAppId) {
    return DefaultAppName::kKeep;
#if BUILDFLAG(IS_CHROMEOS_ASH)
  } else if (app_id == extension_misc::kGooglePhotosAppId ||
             app_id == arc::kGooglePhotosAppId) {
    return DefaultAppName::kPhotos;
  } else if (app_id == arc::kPlayBooksAppId) {
    return DefaultAppName::kPlayBooks;
  } else if (app_id == arc::kPlayGamesAppId) {
    return DefaultAppName::kPlayGames;
  } else if (app_id == arc::kPlayMoviesAppId ||
             app_id == extension_misc::kGooglePlayMoviesAppId) {
    return DefaultAppName::kPlayMovies;
  } else if (app_id == arc::kPlayMusicAppId ||
             app_id == extension_misc::kGooglePlayMusicAppId) {
    return DefaultAppName::kPlayMusic;
  } else if (app_id == arc::kPlayStoreAppId) {
    return DefaultAppName::kPlayStore;
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
  } else if (app_id == extension_misc::kGoogleSheetsAppId) {
    return DefaultAppName::kSheets;
  } else if (app_id == extension_misc::kGoogleSlidesAppId) {
    return DefaultAppName::kSlides;
  } else if (app_id == extensions::kWebStoreAppId) {
    return DefaultAppName::kWebStore;
#if BUILDFLAG(IS_CHROMEOS_ASH)
  } else if (app_id == extension_misc::kYoutubeAppId ||
             app_id == arc::kYoutubeAppId) {
    return DefaultAppName::kYouTube;
  } else if (app_id == arc::kGoogleTVAppId) {
    return DefaultAppName::kGoogleTv;
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
  }

  return std::nullopt;
}

void RecordAppLaunch(const std::string& app_id,
                     apps::LaunchSource launch_source) {
  if (const std::optional<DefaultAppName> app_name = AppIdToName(app_id)) {
    RecordDefaultAppLaunch(app_name.value(), launch_source);
#if BUILDFLAG(IS_CHROMEOS_ASH)
    if (ash::features::IsWelcomeTourEnabled()) {
      RecordWelcomeTourInteraction(app_name.value(), launch_source);
    }
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
  }
}

const std::optional<apps::DefaultAppName> PreinstalledWebAppIdToName(
    const std::string& app_id) {
  if (app_id == web_app::kCalculatorAppId) {
    return apps::DefaultAppName::kCalculator;
  } else if (app_id == web_app::kCanvasAppId) {
    return apps::DefaultAppName::kChromeCanvas;
#if BUILDFLAG(GOOGLE_CHROME_BRANDING) && BUILDFLAG(IS_CHROMEOS)
  } else if (app_id == web_app::kContainerAppId) {
    return apps::DefaultAppName::kContainer;
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING) && BUILDFLAG(IS_CHROMEOS)
  } else if (app_id == web_app::kCursiveAppId) {
    return apps::DefaultAppName::kCursive;
  } else if (app_id == web_app::kGmailAppId) {
    return apps::DefaultAppName::kGmail;
  } else if (app_id == web_app::kGoogleMoviesAppId) {
    return apps::DefaultAppName::kPlayMovies;
  } else if (app_id == web_app::kGoogleCalendarAppId) {
    return apps::DefaultAppName::kGoogleCalendar;
  } else if (app_id == web_app::kGoogleChatAppId) {
    return apps::DefaultAppName::kGoogleChat;
  } else if (app_id == web_app::kGoogleDocsAppId) {
    return apps::DefaultAppName::kDocs;
  } else if (app_id == web_app::kGoogleDriveAppId) {
    return apps::DefaultAppName::kDrive;
  } else if (app_id == web_app::kGoogleMeetAppId) {
    return apps::DefaultAppName::kGoogleMeet;
  } else if (app_id == web_app::kGoogleSheetsAppId) {
    return apps::DefaultAppName::kSheets;
  } else if (app_id == web_app::kGoogleSlidesAppId) {
    return apps::DefaultAppName::kSlides;
  } else if (app_id == web_app::kGoogleKeepAppId) {
    return apps::DefaultAppName::kKeep;
  } else if (app_id == web_app::kGoogleMapsAppId) {
    return apps::DefaultAppName::kGoogleMaps;
  } else if (app_id == web_app::kMallAppId) {
    return DefaultAppName::kMall;
  } else if (app_id == web_app::kMessagesAppId) {
    return apps::DefaultAppName::kGoogleMessages;
  } else if (app_id == web_app::kPlayBooksAppId) {
    return apps::DefaultAppName::kPlayBooks;
  } else if (app_id == web_app::kYoutubeAppId) {
    return apps::DefaultAppName::kYouTube;
  } else if (app_id == web_app::kYoutubeMusicAppId) {
    return apps::DefaultAppName::kYouTubeMusic;
  } else {
    return std::nullopt;
  }
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
const std::optional<apps::DefaultAppName> SystemWebAppIdToName(
    const std::string& app_id) {
  // These apps should all have chrome:// URLs.
  if (app_id == web_app::kCameraAppId) {
    return apps::DefaultAppName::kCamera;
  } else if (app_id == web_app::kDiagnosticsAppId) {
    return apps::DefaultAppName::kDiagnosticsApp;
  } else if (app_id == file_manager::kFileManagerSwaAppId) {
    return apps::DefaultAppName::kFiles;
  } else if (app_id == web_app::kFirmwareUpdateAppId) {
    return apps::DefaultAppName::kFirmwareUpdateApp;
  } else if (app_id == web_app::kHelpAppId) {
    return apps::DefaultAppName::kHelpApp;
  } else if (app_id == ash::kMallSystemAppId) {
    return apps::DefaultAppName::kMall;
  } else if (app_id == web_app::kMediaAppId) {
    return apps::DefaultAppName::kMediaApp;
    // `MockSystemApp` is for tests only.
  } else if (app_id == web_app::kMockSystemAppId) {
    return apps::DefaultAppName::kMockSystemApp;
  } else if (app_id == web_app::kOsFeedbackAppId) {
    return apps::DefaultAppName::kOsFeedbackApp;
  } else if (app_id == web_app::kOsSettingsAppId) {
    return apps::DefaultAppName::kSettings;
  } else if (app_id == web_app::kPrintManagementAppId) {
    return apps::DefaultAppName::kPrintManagementApp;
  } else if (app_id == ash::kChromeUIUntrustedProjectorSwaAppId) {
    return apps::DefaultAppName::kProjector;
  } else if (app_id == web_app::kSanitizeAppId) {
    return apps::DefaultAppName::kSanitizeApp;
  } else if (app_id == web_app::kScanningAppId) {
    return apps::DefaultAppName::kScanningApp;
  } else if (app_id == web_app::kShimlessRMAAppId) {
    return apps::DefaultAppName::kShimlessRMAApp;
  } else if (app_id == web_app::kShortcutCustomizationAppId) {
    return apps::DefaultAppName::kShortcutCustomizationApp;
  } else {
    return std::nullopt;
  }
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

}  // namespace apps
