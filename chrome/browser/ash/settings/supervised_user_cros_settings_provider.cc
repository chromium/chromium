// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/settings/supervised_user_cros_settings_provider.h"

#include <string_view>

#include "base/check.h"
#include "base/containers/contains.h"
#include "base/values.h"
#include "chromeos/ash/components/settings/cros_settings_names.h"
#include "components/account_id/account_id.h"
#include "components/user_manager/user_manager.h"

namespace ash {

SupervisedUserCrosSettingsProvider::SupervisedUserCrosSettingsProvider(
    const CrosSettingsProvider::NotifyObserversCallback& notify_cb)
    : CrosSettingsProvider(notify_cb) {
  child_user_restrictions_.insert_or_assign(kAccountsPrefAllowGuest,
                                            base::Value(false));
  child_user_restrictions_.insert_or_assign(kAccountsPrefShowUserNamesOnSignIn,
                                            base::Value(true));
  child_user_restrictions_.insert_or_assign(kAccountsPrefAllowNewUser,
                                            base::Value(true));
}

SupervisedUserCrosSettingsProvider::~SupervisedUserCrosSettingsProvider() =
    default;

const base::Value* SupervisedUserCrosSettingsProvider::Get(
    std::string_view path) const {
  DCHECK(HandlesSetting(path));
  auto iter = child_user_restrictions_.find(path);
  return &(iter->second);
}

CrosSettingsProvider::TrustedStatus
SupervisedUserCrosSettingsProvider::PrepareTrustedValues(
    base::OnceClosure* callback) {
  return TrustedStatus::TRUSTED;
}

bool SupervisedUserCrosSettingsProvider::HandlesSetting(
    std::string_view path) const {
  if (!user_manager::UserManager::IsInitialized())
    return false;
  auto* user_manager = user_manager::UserManager::Get();
  if (user_manager->GetUsers().empty())
    return false;

  const AccountId owner_account_id = user_manager->GetOwnerAccountId();
  if (!owner_account_id.is_valid()) {
    // Unowned or admin-owned device.
    return false;
  }
  auto* device_owner = user_manager->FindUser(owner_account_id);

  if (device_owner && device_owner->IsChild()) {
    return base::Contains(child_user_restrictions_, path);
  }

  return false;
}

}  // namespace ash
