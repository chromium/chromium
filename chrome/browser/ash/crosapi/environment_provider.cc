// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/environment_provider.h"

#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/system/sys_info.h"
#include "chrome/browser/ash/drive/drive_integration_service.h"
#include "chrome/browser/ash/file_manager/path_util.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profiles_state.h"
#include "chrome/browser/web_applications/preinstalled_web_app_config_utils.h"
#include "chromeos/ash/components/dbus/cros_disks/cros_disks_client.h"
#include "chromeos/ash/components/install_attributes/install_attributes.h"
#include "chromeos/crosapi/mojom/crosapi.mojom.h"
#include "chromeos/crosapi/mojom/policy_namespace.mojom.h"
#include "components/account_id/account_id.h"
#include "components/account_manager_core/account.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "components/user_manager/user_type.h"
#include "crypto/nss_util_internal.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace crosapi {

EnvironmentProvider::EnvironmentProvider() = default;
EnvironmentProvider::~EnvironmentProvider() = default;

mojom::SessionType EnvironmentProvider::GetSessionType() {
  const user_manager::User* const user =
      user_manager::UserManager::Get()->GetPrimaryUser();
  const Profile* const profile =
      ash::ProfileHelper::Get()->GetProfileByUser(user);
  if (profile->IsGuestSession()) {
    return mojom::SessionType::kGuestSession;
  }
  if (profiles::IsPublicSession()) {
    return mojom::SessionType::kPublicSession;
  }
  if (user->GetType() == user_manager::USER_TYPE_WEB_KIOSK_APP) {
    return mojom::SessionType::kWebKioskSession;
  }
  if (user->GetType() == user_manager::USER_TYPE_KIOSK_APP) {
    return mojom::SessionType::kAppKioskSession;
  }
  if (user->GetType() == user_manager::USER_TYPE_CHILD) {
    return mojom::SessionType::kChildSession;
  }
  return mojom::SessionType::kRegularSession;
}

mojom::DeviceMode EnvironmentProvider::GetDeviceMode() {
  policy::DeviceMode mode = ash::InstallAttributes::Get()->GetMode();
  switch (mode) {
    case policy::DEVICE_MODE_PENDING:
      // "Pending" is an internal detail of InstallAttributes and doesn't need
      // its own mojom value.
      return mojom::DeviceMode::kNotSet;
    case policy::DEVICE_MODE_NOT_SET:
      return mojom::DeviceMode::kNotSet;
    case policy::DEVICE_MODE_CONSUMER:
      return mojom::DeviceMode::kConsumer;
    case policy::DEVICE_MODE_ENTERPRISE:
      return mojom::DeviceMode::kEnterprise;
    case policy::DEVICE_MODE_ENTERPRISE_AD:
      return mojom::DeviceMode::kEnterpriseActiveDirectory;
    case policy::DEPRECATED_DEVICE_MODE_LEGACY_RETAIL_MODE:
      return mojom::DeviceMode::kLegacyRetailMode;
    case policy::DEVICE_MODE_CONSUMER_KIOSK_AUTOLAUNCH:
      return mojom::DeviceMode::kConsumerKioskAutolaunch;
    case policy::DEVICE_MODE_DEMO:
      return mojom::DeviceMode::kDemo;
  }
}

mojom::DefaultPathsPtr EnvironmentProvider::GetDefaultPaths() {
  mojom::DefaultPathsPtr default_paths = mojom::DefaultPaths::New();
  // The default paths belong to ash's primary user profile. Lacros does not
  // support multi-signin.
  const user_manager::User* user =
      user_manager::UserManager::Get()->GetPrimaryUser();
  Profile* profile = ash::ProfileHelper::Get()->GetProfileByUser(user);

  default_paths->user_nss_database =
      crypto::GetSoftwareNSSDBPath(profile->GetPath());

  if (base::SysInfo::IsRunningOnChromeOS()) {
    // Typically /home/chronos/u-<hash>/MyFiles.
    default_paths->documents =
        file_manager::util::GetMyFilesFolderForProfile(profile);
    // Typically /home/chronos/u-<hash>/MyFiles/Downloads.
    default_paths->downloads =
        file_manager::util::GetDownloadsFolderForProfile(profile);
    auto* integration_service =
        drive::DriveIntegrationServiceFactory::FindForProfile(profile);
    if (integration_service && integration_service->is_enabled() &&
        integration_service->IsMounted()) {
      default_paths->drivefs = integration_service->GetMountPointPath();
    }
    default_paths->android_files =
        base::FilePath(file_manager::util::GetAndroidFilesPath());
    default_paths->linux_files =
        file_manager::util::GetCrostiniMountDirectory(profile);
    base::FilePath ash_resources;
    if (base::PathService::Get(base::DIR_ASSETS, &ash_resources))
      default_paths->ash_resources = ash_resources;
  } else {
    // On developer linux workstations the above functions do path mangling to
    // support multi-signin which gets undone later in ash-specific code. This
    // is awkward for Lacros development, so just provide some defaults.
    base::FilePath home = base::GetHomeDir();
    default_paths->documents = home.Append("Documents");
    default_paths->downloads = home.Append("Downloads");
    default_paths->drivefs = home.Append("Drive");
    default_paths->android_files = home.Append("Android");
    default_paths->linux_files = home.Append("Crostini");
    default_paths->ash_resources = home.Append("Ash");
  }

  // CrosDisksClient already has a convention for its removable media directory
  // when running on Linux workstations.
  default_paths->removable_media =
      ash::CrosDisksClient::GetRemovableDiskMountPoint();

  // Ash expects to find shared files in the share cache.
  default_paths->share_cache =
      file_manager::util::GetShareCacheFilePath(profile);

  default_paths->preinstalled_web_app_config =
      web_app::GetPreinstalledWebAppConfigDirFromCommandLine(profile);
  default_paths->preinstalled_web_app_extra_config =
      web_app::GetPreinstalledWebAppExtraConfigDirFromCommandLine(profile);

  return default_paths;
}

absl::optional<account_manager::Account>
EnvironmentProvider::GetDeviceAccount() {
  // Lacros doesn't support Multi-Login. Get the Primary User.
  const user_manager::User* user =
      user_manager::UserManager::Get()->GetPrimaryUser();
  if (!user)
    return absl::nullopt;

  const AccountId& account_id = user->GetAccountId();
  switch (account_id.GetAccountType()) {
    case AccountType::ACTIVE_DIRECTORY:
      return absl::make_optional(account_manager::Account{
          account_manager::AccountKey{
              account_id.GetObjGuid(),
              account_manager::AccountType::kActiveDirectory},
          user->GetDisplayEmail()});
    case AccountType::GOOGLE:
      return absl::make_optional(account_manager::Account{
          account_manager::AccountKey{account_id.GetGaiaId(),
                                      account_manager::AccountType::kGaia},
          user->GetDisplayEmail()});
    case AccountType::UNKNOWN:
      return absl::nullopt;
  }
}

void EnvironmentProvider::SetDeviceAccountPolicy(
    const std::string& policy_blob) {
  device_account_policy_blob_ = policy_blob;
}

std::string EnvironmentProvider::GetDeviceAccountPolicy() {
  return device_account_policy_blob_;
}

const policy::ComponentPolicyMap&
EnvironmentProvider::GetDeviceAccountComponentPolicy() {
  return component_policy_;
}

void EnvironmentProvider::SetDeviceAccountComponentPolicy(
    policy::ComponentPolicyMap component_policy) {
  component_policy_ = std::move(component_policy);
}

base::Time EnvironmentProvider::GetLastPolicyFetchAttemptTimestamp() {
  return last_policy_fetch_attempt_;
}

void EnvironmentProvider::SetLastPolicyFetchAttemptTimestamp(
    const base::Time& timestamp) {
  last_policy_fetch_attempt_ = timestamp;
}

}  // namespace crosapi
