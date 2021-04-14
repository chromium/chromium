// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/environment_provider.h"

#include "base/files/file_util.h"
#include "base/optional.h"
#include "base/system/sys_info.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/chromeos/file_manager/path_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profiles_state.h"
#include "chromeos/crosapi/mojom/crosapi.mojom.h"
#include "chromeos/tpm/install_attributes.h"
#include "components/account_id/account_id.h"
#include "components/account_manager_core/account.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"

namespace crosapi {

EnvironmentProvider::EnvironmentProvider() = default;
EnvironmentProvider::~EnvironmentProvider() = default;

mojom::SessionType EnvironmentProvider::GetSessionType() {
  const user_manager::User* const user =
      user_manager::UserManager::Get()->GetPrimaryUser();
  const Profile* const profile =
      chromeos::ProfileHelper::Get()->GetProfileByUser(user);
  if (profile->IsGuestSession()) {
    return mojom::SessionType::kGuestSession;
  }
  if (profiles::IsPublicSession()) {
    return mojom::SessionType::kPublicSession;
  }
  return mojom::SessionType::kRegularSession;
}

mojom::DeviceMode EnvironmentProvider::GetDeviceMode() {
  policy::DeviceMode mode = chromeos::InstallAttributes::Get()->GetMode();
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
  Profile* profile = chromeos::ProfileHelper::Get()->GetProfileByUser(user);
  if (base::SysInfo::IsRunningOnChromeOS()) {
    // Typically /home/chronos/u-<hash>/MyFiles.
    default_paths->documents =
        file_manager::util::GetMyFilesFolderForProfile(profile);
    // Typically /home/chronos/u-<hash>/MyFiles/Downloads.
    default_paths->downloads =
        file_manager::util::GetDownloadsFolderForProfile(profile);
  } else {
    // On developer linux workstations the above functions do path mangling to
    // support multi-signin which gets undone later in ash-specific code. This
    // is awkward for Lacros development, so just provide some defaults.
    base::FilePath home = base::GetHomeDir();
    default_paths->documents = home.Append("Documents");
    default_paths->downloads = home.Append("Downloads");
  }
  return default_paths;
}

std::string EnvironmentProvider::GetDeviceAccountGaiaId() {
  const user_manager::User* const user =
      user_manager::UserManager::Get()->GetPrimaryUser();
  if (!user)
    return std::string();

  const AccountId& account_id = user->GetAccountId();
  if (account_id.GetAccountType() != AccountType::GOOGLE)
    return std::string();

  DCHECK(!account_id.GetGaiaId().empty());
  return account_id.GetGaiaId();
}

base::Optional<account_manager::Account>
EnvironmentProvider::GetDeviceAccount() {
  // Lacros doesn't support Multi-Login. Get the Primary User.
  const user_manager::User* user =
      user_manager::UserManager::Get()->GetPrimaryUser();
  if (!user)
    return base::nullopt;

  const AccountId& account_id = user->GetAccountId();
  switch (account_id.GetAccountType()) {
    case AccountType::ACTIVE_DIRECTORY:
      return base::make_optional(account_manager::Account{
          account_manager::AccountKey{
              account_id.GetObjGuid(),
              account_manager::AccountType::kActiveDirectory},
          user->GetDisplayEmail()});
    case AccountType::GOOGLE:
      return base::make_optional(account_manager::Account{
          account_manager::AccountKey{account_id.GetGaiaId(),
                                      account_manager::AccountType::kGaia},
          user->GetDisplayEmail()});
    case AccountType::UNKNOWN:
      return base::nullopt;
  }
}

void EnvironmentProvider::SetDeviceAccountPolicy(
    const std::string& policy_blob) {
  device_account_policy_blob_ = policy_blob;
}

std::string EnvironmentProvider::GetDeviceAccountPolicy() {
  return device_account_policy_blob_;
}

}  // namespace crosapi
