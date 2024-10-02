// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/drive/file_system_util.h"

#include <stddef.h>

#include <memory>
#include <string>
#include <type_traits>
#include <vector>

#include "ash/constants/ash_constants.h"
#include "ash/constants/ash_features.h"
#include "ash/constants/ash_switches.h"
#include "base/command_line.h"
#include "base/containers/fixed_flat_set.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/system/sys_info.h"
#include "chrome/browser/ash/drive/drive_integration_service.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/chrome_paths_internal.h"
#include "chromeos/ash/components/login/login_state/login_state.h"
#include "chromeos/ash/components/network/managed_state.h"
#include "chromeos/ash/components/network/network_handler.h"
#include "chromeos/ash/components/network/network_state.h"
#include "chromeos/ash/components/network/network_state_handler.h"
#include "components/drive/drive_pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "content/public/browser/browser_thread.h"
#include "google_apis/gaia/gaia_auth_util.h"

using content::BrowserThread;

namespace drive::util {

using user_manager::User;
using user_manager::UserManager;

namespace {

ConnectionStatus GetDeviceOnlineStatus() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  using enum ConnectionStatus;

  if (!ash::NetworkHandler::IsInitialized()) {
    VLOG(1) << "GetDeviceOnlineStatus: no network handler";
    return kNoNetwork;
  }

  ash::NetworkStateHandler* const handler =
      ash::NetworkHandler::Get()->network_state_handler();
  DCHECK(handler);

  const ash::NetworkState* const network = handler->DefaultNetwork();
  if (!network) {
    VLOG(1) << "GetDeviceOnlineStatus: no network";
    return kNoNetwork;
  }

  if (!network->IsOnline()) {
    VLOG(1) << "GetDeviceOnlineStatus: not ready: network is "
            << network->connection_state();
    return kNotReady;
  }

  using PortalState = ash::NetworkState::PortalState;
  if (const PortalState portal_state = network->GetPortalState();
      portal_state != PortalState::kOnline) {
    VLOG(1) << "GetDeviceOnlineStatus: not ready: portal is " << portal_state;
    return kNotReady;
  }

  if (handler->default_network_is_metered()) {
    VLOG(1) << "GetDeviceOnlineStatus: metered";
    return kMetered;
  }

  return kConnected;
}

}  // namespace

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

DriveAvailability CheckDriveAvailabilityForProfile(
    const Profile* const profile) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // Disable Drive for non-Gaia accounts.
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          ash::switches::kDisableGaiaServices)) {
    return DriveAvailability::kNotAvailableForAccountType;
  }
  if (!ash::LoginState::IsInitialized()) {
    return DriveAvailability::kNotAvailableForUninitialisedLoginState;
  }
  // Disable Drive for incognito profiles.
  if (profile->IsOffTheRecord()) {
    return DriveAvailability::kNotAvailableInIncognito;
  }
  const User* const user = ash::ProfileHelper::Get()->GetUserByProfile(profile);
  if (!user || !user->HasGaiaAccount()) {
    return DriveAvailability::kNotAvailableForAccountType;
  }

  // Disable Drive if the flag has been passed and it is a test image.
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          ash::switches::kDisableDriveFsForTesting)) {
    base::SysInfo::CrashIfChromeOSNonTestImage();
    return DriveAvailability::kNotAvailableForTestImage;
  }

  return DriveAvailability::kAvailable;
}

bool IsDriveAvailableForProfile(const Profile* const profile) {
  return CheckDriveAvailabilityForProfile(profile) ==
         DriveAvailability::kAvailable;
}

DriveAvailability CheckDriveEnabledAndDriveAvailabilityForProfile(
    const Profile* const profile) {
  // Disable Drive if preference is set. This can happen with commandline flag
  // --disable-drive or enterprise policy, or with user settings.
  if (profile->GetPrefs()->GetBoolean(prefs::kDisableDrive)) {
    return DriveAvailability::kNotAvailableWhenDisableDrivePreferenceSet;
  }

  return CheckDriveAvailabilityForProfile(profile);
}

bool IsDriveEnabledForProfile(const Profile* const profile) {
  return CheckDriveEnabledAndDriveAvailabilityForProfile(profile) ==
         DriveAvailability::kAvailable;
}

bool IsDriveFsBulkPinningAvailable(const Profile* const profile) {
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

  // For Googlers, the bulk-pinning feature is available on any kind of device.
  if (UserManager::IsInitialized()) {
    if (const User* const user = UserManager::Get()->GetActiveUser();
        user && gaia::IsGoogleInternalAccountEmail(
                    user->GetAccountId().GetUserEmail())) {
      return true;
    }
  }

  // For other users (non-Googlers), the bulk-pinning feature is available on
  // suitable devices, as controlled by the
  // "FeatureManagementDriveFsBulkPinning" Chrome feature.
  return base::FeatureList::IsEnabled(
      ash::features::kFeatureManagementDriveFsBulkPinning);
}

bool IsDriveFsBulkPinningAvailable() {
  return IsDriveFsBulkPinningAvailable(ProfileManager::GetActiveUserProfile());
}

bool IsOobeDrivePinningAvailable(const Profile* const profile) {
  const bool b = IsOobeDrivePinningScreenEnabled() &&
                 IsDriveFsBulkPinningAvailable(profile);
  VLOG(1) << "IsOobeDrivePinningAvailable() returned " << b;
  return b;
}

bool IsOobeDrivePinningAvailable() {
  return IsOobeDrivePinningAvailable(ProfileManager::GetActiveUserProfile());
}

// To ensure that the DrivePinningScreen is always available to the wizard,
// regardless of the current user profile, check this to add the
// DrivePinningScreen to the screen_manager when initializing the
// wizardController.
bool IsOobeDrivePinningScreenEnabled() {
  return base::FeatureList::IsEnabled(ash::features::kOobeDrivePinning) &&
         ash::features::IsOobeChoobeEnabled();
}

bool IsDriveFsMirrorSyncAvailable(const Profile* const profile) {
  return base::FeatureList::IsEnabled(ash::features::kDriveFsMirroring);
}

std::ostream& operator<<(std::ostream& out, const ConnectionStatus status) {
  switch (status) {
#define PRINT(s)               \
  case ConnectionStatus::k##s: \
    return out << #s;
    PRINT(NoService)
    PRINT(NoNetwork)
    PRINT(NotReady)
    PRINT(Metered)
    PRINT(Connected)
#undef PRINT
  }

  return out << "ConnectionStatus("
             << static_cast<std::underlying_type_t<ConnectionStatus>>(status)
             << ")";
}

// For testing.
static ConnectionStatus connection_status_for_testing;
static bool has_connection_status_for_testing = false;

void SetDriveConnectionStatusForTesting(const ConnectionStatus status) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  VLOG(1) << "SetDriveConnectionStatusForTesting: " << status;
  connection_status_for_testing = status;
  has_connection_status_for_testing = true;
}

ConnectionStatus GetDriveConnectionStatus(Profile* const profile,
                                          bool* is_online) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  using enum ConnectionStatus;

  if (has_connection_status_for_testing) {
    VLOG(1) << "GetDriveConnectionStatus: for testing: "
            << connection_status_for_testing;
    return connection_status_for_testing;
  }

  ConnectionStatus online_status = GetDeviceOnlineStatus();
  if (is_online != nullptr) {
    *is_online = online_status == kConnected || online_status == kMetered;
  }

  if (!GetIntegrationServiceByProfile(profile)) {
    VLOG(1) << "GetDriveConnectionStatus: no Drive integration service";
    return kNoService;
  }

  DCHECK(profile);
  if (online_status == kMetered &&
      !profile->GetPrefs()->GetBoolean(prefs::kDisableDriveOverCellular)) {
    VLOG(1) << "GetDriveConnectionStatus: metered, but still enabled";
    return kConnected;
  }

  VLOG(1) << "GetDriveConnectionStatus: " << online_status;
  return online_status;
}

bool IsPinnableGDocMimeType(const std::string& mime_type) {
  constexpr auto kPinnableGDocMimeTypes =
      base::MakeFixedFlatSet<std::string_view>({
          "application/vnd.google-apps.document",
          "application/vnd.google-apps.drawing",
          "application/vnd.google-apps.presentation",
          "application/vnd.google-apps.spreadsheet",
      });

  return kPinnableGDocMimeTypes.contains(mime_type);
}

int64_t ComputeDriveFsContentCacheSize(const base::FilePath& path) {
  int64_t blocks = 0;

  using base::FileEnumerator;
  FileEnumerator it(path, true, FileEnumerator::FILES);
  while (!it.Next().empty()) {
    const FileEnumerator::FileInfo& info = it.GetInfo();

    // Skip the `chunks.db*` files.
    if (base::StartsWith(info.GetName().value(), "chunks.db")) {
      continue;
    }

    blocks += info.stat().st_blocks;
  }

  const int64_t size = blocks << 9;
  VLOG(1) << "DriveFs cache: " << (size >> 20) << " M";
  return size;
}

}  // namespace drive::util
