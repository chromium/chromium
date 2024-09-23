// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/child_accounts/family_user_device_metrics.h"

#include <string>

#include "base/check.h"
#include "base/metrics/histogram_functions.h"
#include "chromeos/ash/components/settings/cros_settings.h"
#include "chromeos/ash/components/settings/cros_settings_names.h"
#include "chromeos/ash/components/settings/cros_settings_provider.h"
#include "components/account_id/account_id.h"
#include "components/session_manager/core/session_manager.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "components/user_manager/user_type.h"

namespace ash {

namespace {

constexpr char kNewUserAddedHistogramName[] = "FamilyUser.NewUserAdded";
constexpr char kDeviceOwnerHistogramName[] = "FamilyUser.DeviceOwner";
constexpr char kFamilyLinkUsersCountHistogramName[] =
    "FamilyUser.FamilyLinkUsersCount";
constexpr char kGaiaUsersCountHistogramName[] = "FamilyUser.GaiaUsersCount";

}  // namespace

FamilyUserDeviceMetrics::FamilyUserDeviceMetrics()
    : user_manager_(user_manager::UserManager::Get()) {
  DCHECK(user_manager_);
  session_manager::SessionManager::Get()->AddObserver(this);
  DeviceSettingsService::Get()->AddObserver(this);
}

FamilyUserDeviceMetrics::~FamilyUserDeviceMetrics() {
  session_manager::SessionManager::Get()->RemoveObserver(this);
  DeviceSettingsService::Get()->RemoveObserver(this);
}

// static
const char* FamilyUserDeviceMetrics::GetNewUserAddedHistogramNameForTest() {
  return kNewUserAddedHistogramName;
}

const char* FamilyUserDeviceMetrics::GetDeviceOwnerHistogramNameForTest() {
  return kDeviceOwnerHistogramName;
}

const char*
FamilyUserDeviceMetrics::GetFamilyLinkUsersCountHistogramNameForTest() {
  return kFamilyLinkUsersCountHistogramName;
}

const char* FamilyUserDeviceMetrics::GetGaiaUsersCountHistogramNameForTest() {
  return kGaiaUsersCountHistogramName;
}

void FamilyUserDeviceMetrics::OnNewDay() {
  const user_manager::UserList& users = user_manager_->GetUsers();
  int family_link_users_count = 0;
  int gaia_users_count = 0;

  for (const user_manager::User* user : users) {
    if (user->HasGaiaAccount())
      gaia_users_count++;

    if (user->IsChild())
      family_link_users_count++;
  }

  base::UmaHistogramCounts100(kFamilyLinkUsersCountHistogramName,
                              family_link_users_count);
  base::UmaHistogramCounts100(kGaiaUsersCountHistogramName, gaia_users_count);

  // If ownership is not established yet, OwnershipStatusChanged() will
  // report the device ownership.
  if (user_manager_->GetOwnerAccountId().is_valid()) {
    base::UmaHistogramBoolean(kDeviceOwnerHistogramName,
                              user_manager_->IsCurrentUserOwner());
  }
}

void FamilyUserDeviceMetrics::OnUserSessionStarted(bool is_primary_user) {
  if (!is_primary_user)
    return;

  if (!user_manager_->IsCurrentUserNew())
    return;

  const user_manager::UserType type =
      user_manager_->GetPrimaryUser()->GetType();

  NewUserAdded new_user_type = NewUserAdded::kOtherUserAdded;
  if (type == user_manager::UserType::kChild) {
    new_user_type = NewUserAdded::kFamilyLinkUserAdded;
  } else if (type == user_manager::UserType::kRegular) {
    new_user_type = NewUserAdded::kRegularUserAdded;
  }

  base::UmaHistogramEnumeration(kNewUserAddedHistogramName, new_user_type);
}

void FamilyUserDeviceMetrics::OwnershipStatusChanged() {
  ReportDeviceOwnership();
}

void FamilyUserDeviceMetrics::ReportDeviceOwnership() {
  const user_manager::User* active_user = user_manager_->GetActiveUser();
  if (!active_user)
    return;

  const CrosSettings* cros_settings = CrosSettings::Get();
  // Schedule a callback if device policy has not yet been verified.
  if (CrosSettingsProvider::TRUSTED !=
      cros_settings->PrepareTrustedValues(
          base::BindOnce(&FamilyUserDeviceMetrics::ReportDeviceOwnership,
                         weak_factory_.GetWeakPtr()))) {
    return;
  }

  std::string owner_email;
  cros_settings->GetString(kDeviceOwner, &owner_email);

  if (owner_email.empty())
    return;

  base::UmaHistogramBoolean(
      kDeviceOwnerHistogramName,
      owner_email == active_user->GetAccountId().GetUserEmail());
}

}  // namespace ash
