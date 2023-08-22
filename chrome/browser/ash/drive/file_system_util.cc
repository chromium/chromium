// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/drive/file_system_util.h"

#include <stddef.h>

#include <memory>
#include <string>
#include <vector>

#include "ash/constants/ash_constants.h"
#include "ash/constants/ash_features.h"
#include "ash/constants/ash_switches.h"
#include "base/check_op.h"
#include "base/command_line.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/callback_helpers.h"
#include "base/strings/escape.h"
#include "chrome/browser/ash/drive/drive_integration_service.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_paths_internal.h"
#include "chromeos/ash/components/login/login_state/login_state.h"
#include "components/drive/drive_pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "components/version_info/version_info.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/network_service_instance.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "services/network/public/cpp/network_connection_tracker.h"

using content::BrowserThread;

namespace drive::util {

using user_manager::User;
using user_manager::UserManager;

DriveIntegrationService* GetIntegrationServiceByProfile(Profile* profile) {
  DriveIntegrationService* service =
      DriveIntegrationServiceFactory::FindForProfile(profile);
  if (!service || !service->IsMounted()) {
    return nullptr;
  }
  return service;
}

bool IsUnderDriveMountPoint(const base::FilePath& path) {
  std::vector<base::FilePath::StringType> components = path.GetComponents();
  if (components.size() < 4) {
    return false;
  }
  if (components[0] != FILE_PATH_LITERAL("/")) {
    return false;
  }
  if (components[1] != FILE_PATH_LITERAL("media")) {
    return false;
  }
  if (components[2] != FILE_PATH_LITERAL("fuse")) {
    return false;
  }
  static const base::FilePath::CharType kPrefix[] =
      FILE_PATH_LITERAL("drivefs");
  if (components[3].compare(0, std::size(kPrefix) - 1, kPrefix) != 0) {
    return false;
  }

  return true;
}

base::FilePath GetCacheRootPath(const Profile* const profile) {
  base::FilePath cache_base_path;
  chrome::GetUserCacheDirectory(profile->GetPath(), &cache_base_path);
  base::FilePath cache_root_path =
      cache_base_path.Append(ash::kDriveCacheDirname);
  static const base::FilePath::CharType kFileCacheVersionDir[] =
      FILE_PATH_LITERAL("v1");
  return cache_root_path.Append(kFileCacheVersionDir);
}

bool IsDriveAvailableForProfile(const Profile* const profile) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // Disable Drive for non-Gaia accounts.
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          ash::switches::kDisableGaiaServices)) {
    return false;
  }
  if (!ash::LoginState::IsInitialized()) {
    return false;
  }
  // Disable Drive for incognito profiles.
  if (profile->IsOffTheRecord()) {
    return false;
  }
  const User* const user = ash::ProfileHelper::Get()->GetUserByProfile(profile);
  if (!user || !user->HasGaiaAccount()) {
    return false;
  }

  return true;
}

bool IsDriveEnabledForProfile(const Profile* const profile) {
  // Disable Drive if preference is set. This can happen with commandline flag
  // --disable-drive or enterprise policy, or with user settings.
  if (profile->GetPrefs()->GetBoolean(prefs::kDisableDrive)) {
    return false;
  }

  return IsDriveAvailableForProfile(profile);
}

bool IsDriveFsBulkPinningEnabled(const Profile* const profile) {
  // Check the "DriveFsBulkPinning" Chrome feature. If this feature is disabled,
  // then it probably means that the kill switch has been activated, and the
  // bulk-pinning feature should not be available.
  if (!base::FeatureList::IsEnabled(ash::features::kDriveFsBulkPinning)) {
    return false;
  }

  // Check the "drivefs.bulk_pinning.visible" boolean pref. If this pref is
  // false, then it probably means that it has been turned down by an enterprise
  // policy, and the bulk-pinning feature should not be available.
  if (profile &&
      !profile->GetPrefs()->GetBoolean(prefs::kDriveFsBulkPinningVisible)) {
    return false;
  }

  // Does the user profile belong to a managed user or not?
  if (!profile || !profile->GetProfilePolicyConnector()->IsManaged()) {
    // Not a managed user. The bulk-pinning feature is available on suitable
    // devices, as controlled by the "FeatureManagementDriveFsBulkPinning"
    // Chrome feature.
    return base::FeatureList::IsEnabled(
        ash::features::kFeatureManagementDriveFsBulkPinning);
  }

  // Managed user. For Googlers, the bulk-pinning feature is available on any
  // kind of device. This allows Googlers to easily test ("dogfood") the
  // bulk-pinning feature.
  //
  // TODO(b/296316774) Revisit this decision for Googlers.
  //
  // Other managed users (non-Googlers) do not have access to the bulk-pinning
  // feature for the time being.
  //
  // TODO(b/296315040) Allow managed users to access the bulk-pinning feature on
  // suitable devices.
  const User* const user = UserManager::Get()->GetActiveUser();
  return user && gaia::IsGoogleInternalAccountEmail(
                     user->GetAccountId().GetUserEmail());
}

bool IsOobeDrivePinningEnabled(const Profile* const profile) {
  return base::FeatureList::IsEnabled(ash::features::kOobeDrivePinning) &&
         ash::features::IsOobeChoobeEnabled() &&
         IsDriveFsBulkPinningEnabled(profile);
}

ConnectionStatusType GetDriveConnectionStatus(Profile* profile) {
  auto* drive_integration_service = GetIntegrationServiceByProfile(profile);
  if (!drive_integration_service) {
    return DRIVE_DISCONNECTED_NOSERVICE;
  }
  auto* network_connection_tracker = content::GetNetworkConnectionTracker();
  if (network_connection_tracker->IsOffline()) {
    return DRIVE_DISCONNECTED_NONETWORK;
  }

  auto connection_type = network::mojom::ConnectionType::CONNECTION_UNKNOWN;
  network_connection_tracker->GetConnectionType(&connection_type,
                                                base::DoNothing());
  const bool is_connection_cellular =
      network::NetworkConnectionTracker::IsConnectionCellular(connection_type);
  const bool disable_sync_over_celluar =
      profile->GetPrefs()->GetBoolean(prefs::kDisableDriveOverCellular);

  if (is_connection_cellular && disable_sync_over_celluar) {
    return DRIVE_CONNECTED_METERED;
  }
  return DRIVE_CONNECTED;
}

bool IsPinnableGDocMimeType(const std::string& mime_type) {
  static const char* const kPinnableGDocMimeTypes[] = {
      "application/vnd.google-apps.document",
      "application/vnd.google-apps.drawing",
      "application/vnd.google-apps.presentation",
      "application/vnd.google-apps.spreadsheet",
  };

  return base::Contains(kPinnableGDocMimeTypes, mime_type);
}

int64_t ComputeDriveFsContentCacheSize(
    const base::FilePath& content_cache_path) {
  int64_t running_size = 0;
  base::FileEnumerator file_iter(content_cache_path,
                                 /*recursive=*/true,
                                 base::FileEnumerator::FILES);
  while (!file_iter.Next().empty()) {
    const base::FileEnumerator::FileInfo& file_info = file_iter.GetInfo();

    // Ignore the `chunks.db*` files when calculating the size of the content
    // cache.
    if (base::StartsWith(file_info.GetName().value(), "chunks.db")) {
      continue;
    }
    running_size += file_info.GetSize();
  }
  LOG(ERROR) << "ComputeDriveFsContentCacheSize: " << running_size;
  return running_size;
}

}  // namespace drive::util
