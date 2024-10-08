// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_list/apps_collections_util.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)

#include <optional>
#include <string>

#include "ash/components/arc/app/arc_app_constants.h"
#include "ash/constants/web_app_id_constants.h"
#include "ash/public/cpp/app_list/app_list_types.h"
#include "ash/public/cpp/app_list/internal_app_id_constants.h"
#include "ash/webui/mall/app_id.h"
#include "ash/webui/projector_app/public/cpp/projector_app_constants.h"
#include "base/no_destructor.h"
#include "build/branding_buildflags.h"
#include "chrome/browser/ash/guest_os/guest_os_terminal.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chromeos/ash/components/file_manager/app_id.h"
#include "chromeos/constants/chromeos_features.h"
#include "components/app_constants/constants.h"
#include "extensions/common/constants.h"

using AppCollectionMap = std::map<std::string, ash::AppCollection>;

namespace {
// Obtain the predefined list of App Collections.
AppCollectionMap GetAppCollectionsMap() {
  return {
      // Test app.
      {apps_util::kTestAppIdWithCollection, ash::AppCollection::kEssentials},

      // Essentials.
      {app_constants::kChromeAppId, ash::AppCollection::kEssentials},
      {app_constants::kLacrosAppId, ash::AppCollection::kEssentials},
      {arc::kPlayStoreAppId, ash::AppCollection::kEssentials},
      {extension_misc::kFilesManagerAppId, ash::AppCollection::kEssentials},
      {file_manager::kFileManagerSwaAppId, ash::AppCollection::kEssentials},
      {web_app::kCameraAppId, ash::AppCollection::kEssentials},
      {ash::kInternalAppIdSettings, ash::AppCollection::kEssentials},
      {web_app::kSettingsAppId, ash::AppCollection::kEssentials},
      {web_app::kOsSettingsAppId, ash::AppCollection::kEssentials},

      // Productivity.
      {arc::kGmailAppId, ash::AppCollection::kProductivity},
      {extension_misc::kGmailAppId, ash::AppCollection::kProductivity},
      {web_app::kGmailAppId, ash::AppCollection::kProductivity},
      {web_app::kGoogleMeetAppId, ash::AppCollection::kProductivity},
      {web_app::kGoogleChatAppId, ash::AppCollection::kProductivity},
      {extension_misc::kGoogleDocsAppId, ash::AppCollection::kProductivity},
      {web_app::kGoogleDocsAppId, ash::AppCollection::kProductivity},
      {extension_misc::kGoogleSlidesAppId, ash::AppCollection::kProductivity},
      {web_app::kGoogleSlidesAppId, ash::AppCollection::kProductivity},
      {extension_misc::kGoogleSheetsAppId, ash::AppCollection::kProductivity},
      {web_app::kGoogleSheetsAppId, ash::AppCollection::kProductivity},
      {extension_misc::kGoogleDriveAppId, ash::AppCollection::kProductivity},
      {web_app::kGoogleDriveAppId, ash::AppCollection::kProductivity},
      {extension_misc::kGoogleKeepAppId, ash::AppCollection::kProductivity},
      {web_app::kGoogleKeepAppId, ash::AppCollection::kProductivity},
      {arc::kGoogleCalendarAppId, ash::AppCollection::kProductivity},
      {extension_misc::kCalendarAppId, ash::AppCollection::kProductivity},
      {web_app::kGoogleCalendarAppId, ash::AppCollection::kProductivity},
      {web_app::kMessagesAppId, ash::AppCollection::kProductivity},
      {extension_misc::kGooglePlusAppId, ash::AppCollection::kProductivity},

      // Creativity.
      {arc::kGooglePhotosAppId, ash::AppCollection::kCreativity},
      {extension_misc::kGooglePhotosAppId, ash::AppCollection::kCreativity},
      {web_app::kMediaAppId, ash::AppCollection::kCreativity},
      {web_app::kCursiveAppId, ash::AppCollection::kCreativity},
      {web_app::kCanvasAppId, ash::AppCollection::kCreativity},
      {ash::kChromeUIUntrustedProjectorSwaAppId,
       ash::AppCollection::kCreativity},
      {web_app::kAdobeExpressAppId, ash::AppCollection::kCreativity},
      {arc::kLightRoomAppId, ash::AppCollection::kCreativity},
      {arc::kInfinitePainterAppId, ash::AppCollection::kCreativity},
      {web_app::kShowtimeAppId, ash::AppCollection::kCreativity},

      // Entertainment.
      {arc::kYoutubeAppId, ash::AppCollection::kEntertainment},
      {extension_misc::kYoutubeAppId, ash::AppCollection::kEntertainment},
      {web_app::kYoutubeAppId, ash::AppCollection::kEntertainment},
      {arc::kYoutubeMusicAppId, ash::AppCollection::kEntertainment},
      {web_app::kYoutubeMusicAppId, ash::AppCollection::kEntertainment},
      {arc::kYoutubeMusicWebApkAppId, ash::AppCollection::kEntertainment},
      {arc::kPlayMoviesAppId, ash::AppCollection::kEntertainment},
      {extension_misc::kGooglePlayMoviesAppId,
       ash::AppCollection::kEntertainment},
      {arc::kGoogleTVAppId, ash::AppCollection::kEntertainment},
      {arc::kPlayMusicAppId, ash::AppCollection::kEntertainment},
      {extension_misc::kGooglePlayMusicAppId,
       ash::AppCollection::kEntertainment},
      {arc::kPlayBooksAppId, ash::AppCollection::kEntertainment},
      {extension_misc::kGooglePlayBooksAppId,
       ash::AppCollection::kEntertainment},
      {web_app::kPlayBooksAppId, ash::AppCollection::kEntertainment},
      {web_app::kYoutubeTVAppId, ash::AppCollection::kEntertainment},

      // Utilities.
      {arc::kGoogleMapsAppId, ash::AppCollection::kUtilities},
      {web_app::kGoogleMapsAppId, ash::AppCollection::kUtilities},
      {web_app::kHelpAppId, ash::AppCollection::kUtilities},
      {web_app::kMallAppId, ash::AppCollection::kUtilities},
      {ash::kMallSystemAppId, ash::AppCollection::kUtilities},
      {web_app::kCalculatorAppId, ash::AppCollection::kUtilities},
      {extension_misc::kCalculatorAppId, ash::AppCollection::kUtilities},
      {extension_misc::kTextEditorAppId, ash::AppCollection::kUtilities},
      {web_app::kPrintManagementAppId, ash::AppCollection::kUtilities},
      {web_app::kScanningAppId, ash::AppCollection::kUtilities},
      {web_app::kShortcutCustomizationAppId, ash::AppCollection::kUtilities},
      {guest_os::kTerminalSystemAppId, ash::AppCollection::kUtilities},
      {web_app::kGoogleNewsAppId, ash::AppCollection::kUtilities},
      {extensions::kWebStoreAppId, ash::AppCollection::kUtilities},
  };
}

// TODO(anasalazar): Remove this when experiment is finished.
// Gets built-in default app order for secondary experimental arm in apps
// collections experiment.
void GetSecondaryDefaultOrder(std::vector<std::string>* app_ids) {
  // clang-format off
  app_ids->insert(app_ids->end(), {
    app_constants::kChromeAppId,
    arc::kPlayStoreAppId,

    extension_misc::kFilesManagerAppId,
    file_manager::kFileManagerSwaAppId
  });

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  if (chromeos::features::IsContainerAppPreinstallEnabled()) {
      app_ids->push_back(web_app::kContainerAppId);
  }
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)

  app_ids->insert(app_ids->end(), {
    web_app::kCameraAppId,

    ash::kInternalAppIdSettings,
    web_app::kSettingsAppId,
    web_app::kOsSettingsAppId,

    arc::kGmailAppId,
    extension_misc::kGmailAppId,
    web_app::kGmailAppId,

    web_app::kGoogleMeetAppId,

    web_app::kGoogleChatAppId,

    extension_misc::kGoogleDocsAppId,
    web_app::kGoogleDocsAppId,

    extension_misc::kGoogleSlidesAppId,
    web_app::kGoogleSlidesAppId,

    extension_misc::kGoogleSheetsAppId,
    web_app::kGoogleSheetsAppId,

    extension_misc::kGoogleDriveAppId,
    web_app::kGoogleDriveAppId,

    extension_misc::kGoogleKeepAppId,
    web_app::kGoogleKeepAppId,

    arc::kGoogleCalendarAppId,
    extension_misc::kCalendarAppId,
    web_app::kGoogleCalendarAppId,

    web_app::kMessagesAppId,

    arc::kGooglePhotosAppId,
    extension_misc::kGooglePhotosAppId,

    web_app::kMediaAppId,
    web_app::kCanvasAppId,

    web_app::kAdobeExpressAppId,

    ash::kChromeUIUntrustedProjectorSwaAppId,
    web_app::kCursiveAppId,

    arc::kYoutubeAppId,
    extension_misc::kYoutubeAppId,
    web_app::kYoutubeAppId,

    arc::kYoutubeMusicAppId,
    web_app::kYoutubeMusicAppId,
    arc::kYoutubeMusicWebApkAppId,

    arc::kPlayMoviesAppId,
    extension_misc::kGooglePlayMoviesAppId,
    arc::kGoogleTVAppId,

    arc::kPlayMusicAppId,
    extension_misc::kGooglePlayMusicAppId,

    arc::kPlayBooksAppId,
    extension_misc::kGooglePlayBooksAppId,
    web_app::kPlayBooksAppId,

    arc::kGoogleMapsAppId,
    web_app::kGoogleMapsAppId,

    web_app::kHelpAppId,

    web_app::kMallAppId,
    ash::kMallSystemAppId,

    web_app::kCalculatorAppId,
    extension_misc::kCalculatorAppId,
    extension_misc::kTextEditorAppId,
    web_app::kPrintManagementAppId,
    web_app::kScanningAppId,
    web_app::kShortcutCustomizationAppId,
    guest_os::kTerminalSystemAppId,

    web_app::kYoutubeTVAppId,
    web_app::kGoogleNewsAppId,
    extensions::kWebStoreAppId,

    arc::kLightRoomAppId,
    arc::kInfinitePainterAppId,
    web_app::kShowtimeAppId,
    extension_misc::kGooglePlusAppId,
  });
  // clang-format on

  if (chromeos::features::IsCloudGamingDeviceEnabled()) {
    app_ids->push_back(web_app::kNvidiaGeForceNowAppId);
  }
}

}  // namespace

namespace apps_util {
constexpr char kTestAppIdWithCollection[] = "app_id_from_essentials";

ash::AppCollection GetCollectionIdForAppId(const std::string& app_id) {
  static const auto app_to_collection_map =
      base::NoDestructor<AppCollectionMap>(GetAppCollectionsMap());
  auto app_collection_found = app_to_collection_map->find(app_id);
  return app_collection_found != app_to_collection_map->end()
             ? app_collection_found->second
             : ash::AppCollection::kUnknown;
}

bool GetModifiedOrdinals(const extensions::ExtensionId& extension_id,
                         syncer::StringOrdinal* app_launch_ordinal) {
  // The following defines the default order of apps.
  std::vector<std::string> app_ids;
  GetSecondaryDefaultOrder(&app_ids);

  syncer::StringOrdinal app_launch =
      syncer::StringOrdinal::CreateInitialOrdinal();
  for (auto id : app_ids) {
    if (id == extension_id) {
      *app_launch_ordinal = app_launch;
      return true;
    }
    app_launch = app_launch.CreateAfter();
  }
  return false;
}

}  // namespace apps_util

#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
