// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_list/apps_collections_util.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)

#include <optional>
#include <string>

#include "ash/public/cpp/app_list/app_list_types.h"
#include "ash/public/cpp/app_list/internal_app_id_constants.h"
#include "ash/webui/projector_app/public/cpp/projector_app_constants.h"
#include "base/no_destructor.h"
#include "chrome/browser/ash/app_list/arc/arc_app_utils.h"
#include "chrome/browser/ash/file_manager/app_id.h"
#include "chrome/browser/ash/guest_os/guest_os_terminal.h"
#include "chrome/browser/web_applications/web_app_id_constants.h"
#include "chrome/common/extensions/extension_constants.h"
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

      // Creativity.
      {arc::kGooglePhotosAppId, ash::AppCollection::kCreativity},
      {extension_misc::kGooglePhotosAppId, ash::AppCollection::kCreativity},
      {web_app::kCanvasAppId, ash::AppCollection::kCreativity},
      {web_app::kCursiveAppId, ash::AppCollection::kCreativity},
      {ash::kChromeUIUntrustedProjectorSwaAppId,
       ash::AppCollection::kCreativity},

      // Entertainment.
      {arc::kYoutubeAppId, ash::AppCollection::kEntertainment},
      {extension_misc::kYoutubeAppId, ash::AppCollection::kEntertainment},
      {web_app::kYoutubeAppId, ash::AppCollection::kEntertainment},
      {arc::kYoutubeMusicAppId, ash::AppCollection::kEntertainment},
      {web_app::kYoutubeMusicAppId, ash::AppCollection::kEntertainment},
      {arc::kYoutubeMusicWebApkAppId, ash::AppCollection::kEntertainment},
      {arc::kGoogleTVAppId, ash::AppCollection::kEntertainment},
      {arc::kPlayBooksAppId, ash::AppCollection::kEntertainment},
      {extension_misc::kGooglePlayBooksAppId,
       ash::AppCollection::kEntertainment},
      {web_app::kPlayBooksAppId, ash::AppCollection::kEntertainment},

      // Utilities.
      {web_app::kHelpAppId, ash::AppCollection::kUtilities},
      {web_app::kCalculatorAppId, ash::AppCollection::kUtilities},
      {extension_misc::kCalculatorAppId, ash::AppCollection::kUtilities},
      {extension_misc::kTextEditorAppId, ash::AppCollection::kUtilities},
      {web_app::kPrintManagementAppId, ash::AppCollection::kUtilities},
      {web_app::kScanningAppId, ash::AppCollection::kUtilities},
      {guest_os::kTerminalSystemAppId, ash::AppCollection::kUtilities},
      {extensions::kWebStoreAppId, ash::AppCollection::kUtilities},
      {arc::kGoogleMapsAppId, ash::AppCollection::kUtilities},
      {web_app::kGoogleMapsAppId, ash::AppCollection::kUtilities},
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

#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
