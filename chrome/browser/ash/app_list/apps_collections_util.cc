// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_list/apps_collections_util.h"

#include <map>
#include <string>

#include "ash/constants/web_app_id_constants.h"
#include "ash/public/cpp/app_list/app_list_types.h"
#include "ash/public/cpp/app_list/internal_app_id_constants.h"
#include "ash/webui/mall/app_id.h"
#include "base/no_destructor.h"
#include "chrome/browser/ash/guest_os/guest_os_terminal.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chromeos/ash/components/file_manager/app_id.h"
#include "chromeos/ash/experiences/arc/app/arc_app_constants.h"
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
      {arc::kPlayStoreAppId, ash::AppCollection::kEssentials},
      {extension_misc::kFilesManagerAppId, ash::AppCollection::kEssentials},
      {file_manager::kFileManagerSwaAppId, ash::AppCollection::kEssentials},
      {ash::kCameraAppId, ash::AppCollection::kEssentials},
      {ash::kInternalAppIdSettings, ash::AppCollection::kEssentials},
      {ash::kSettingsAppId, ash::AppCollection::kEssentials},
      {ash::kOsSettingsAppId, ash::AppCollection::kEssentials},

      // Productivity.
      {arc::kGmailAppId, ash::AppCollection::kProductivity},
      {extension_misc::kGmailAppId, ash::AppCollection::kProductivity},
      {ash::kGmailAppId, ash::AppCollection::kProductivity},
      {ash::kGoogleMeetAppId, ash::AppCollection::kProductivity},
      {ash::kGoogleChatAppId, ash::AppCollection::kProductivity},
      {ash::kOldGoogleChatAppId, ash::AppCollection::kProductivity},
      {extension_misc::kGoogleDocsAppId, ash::AppCollection::kProductivity},
      {ash::kGoogleDocsAppId, ash::AppCollection::kProductivity},
      {extension_misc::kGoogleSlidesAppId, ash::AppCollection::kProductivity},
      {ash::kGoogleSlidesAppId, ash::AppCollection::kProductivity},
      {extension_misc::kGoogleSheetsAppId, ash::AppCollection::kProductivity},
      {ash::kGoogleSheetsAppId, ash::AppCollection::kProductivity},
      {extension_misc::kGoogleDriveAppId, ash::AppCollection::kProductivity},
      {ash::kGoogleDriveAppId, ash::AppCollection::kProductivity},
      {extension_misc::kGoogleKeepAppId, ash::AppCollection::kProductivity},
      {ash::kGoogleKeepAppId, ash::AppCollection::kProductivity},
      {arc::kGoogleCalendarAppId, ash::AppCollection::kProductivity},
      {extension_misc::kCalendarAppId, ash::AppCollection::kProductivity},
      {ash::kGoogleCalendarAppId, ash::AppCollection::kProductivity},
      {ash::kMessagesAppId, ash::AppCollection::kProductivity},
      {extension_misc::kGooglePlusAppId, ash::AppCollection::kProductivity},

      // Creativity.
      {arc::kGooglePhotosAppId, ash::AppCollection::kCreativity},
      {extension_misc::kGooglePhotosAppId, ash::AppCollection::kCreativity},
      {ash::kMediaAppId, ash::AppCollection::kCreativity},
      {ash::kCursiveAppId, ash::AppCollection::kCreativity},
      {ash::kCanvasAppId, ash::AppCollection::kCreativity},
      {ash::kChromeUIUntrustedProjectorSwaAppId,
       ash::AppCollection::kCreativity},
      {ash::kAdobeExpressAppId, ash::AppCollection::kCreativity},
      {arc::kLightRoomAppId, ash::AppCollection::kCreativity},
      {arc::kInfinitePainterAppId, ash::AppCollection::kCreativity},
      {ash::kShowtimeAppId, ash::AppCollection::kCreativity},

      // Entertainment.
      {arc::kYoutubeAppId, ash::AppCollection::kEntertainment},
      {extension_misc::kYoutubeAppId, ash::AppCollection::kEntertainment},
      {ash::kYoutubeAppId, ash::AppCollection::kEntertainment},
      {arc::kYoutubeMusicAppId, ash::AppCollection::kEntertainment},
      {ash::kYoutubeMusicAppId, ash::AppCollection::kEntertainment},
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
      {ash::kPlayBooksAppId, ash::AppCollection::kEntertainment},
      {ash::kYoutubeTVAppId, ash::AppCollection::kEntertainment},

      // Utilities.
      {arc::kGoogleMapsAppId, ash::AppCollection::kUtilities},
      {ash::kGoogleMapsAppId, ash::AppCollection::kUtilities},
      {ash::kHelpAppId, ash::AppCollection::kUtilities},
      {ash::kMallSystemAppId, ash::AppCollection::kUtilities},
      {ash::kCalculatorAppId, ash::AppCollection::kUtilities},
      {extension_misc::kCalculatorAppId, ash::AppCollection::kUtilities},
      {extension_misc::kTextEditorAppId, ash::AppCollection::kUtilities},
      {ash::kPrintManagementAppId, ash::AppCollection::kUtilities},
      {ash::kScanningAppId, ash::AppCollection::kUtilities},
      {ash::kShortcutCustomizationAppId, ash::AppCollection::kUtilities},
      {guest_os::kTerminalSystemAppId, ash::AppCollection::kUtilities},
      {ash::kGoogleNewsAppId, ash::AppCollection::kUtilities},
      {extensions::kWebStoreAppId, ash::AppCollection::kUtilities},
  };
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

}  // namespace apps_util
