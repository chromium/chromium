// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/settings/supervised_user_cros_settings_provider.h"

#include "base/logging.h"
#include "base/stl_util.h"
#include "base/values.h"
#include "chromeos/settings/cros_settings_names.h"
#include "components/account_id/account_id.h"
#include "components/user_manager/user_manager.h"

namespace chromeos {

SupervisedUserCrosSettingsProvider::SupervisedUserCrosSettingsProvider(
    const CrosSettingsProvider::NotifyObserversCallback& notify_cb)
    : CrosSettingsProvider(notify_cb) {
  child_user_restrictions_[chromeos::kAccountsPrefAllowGuest] =
      base::Value(false);
  child_user_restrictions_[chromeos::kAccountsPrefShowUserNamesOnSignIn] =
      base::Value(true);
  child_user_restrictions_[chromeos::kAccountsPrefAllowNewUser] =
      base::Value(true);
}

SupervisedUserCrosSettingsProvider::~SupervisedUserCrosSettingsProvider() =
    default;

const base::Value* SupervisedUserCrosSettingsProvider::Get(
    const std::string& path) const {
  DCHECK(HandlesSetting(path));
  auto iter = child_user_restrictions_.find(path);
  return &(iter->second);
}

CrosSettingsProvider::TrustedStatus
SupervisedUserCrosSettingsProvider::PrepareTrustedValues(
    const base::Closure& callback) {
  return CrosSettingsProvider::TrustedStatus::TRUSTED;
}

bool SupervisedUserCrosSettingsProvider::HandlesSetting(
    const std::string& path) const {
  if (!user_manager::UserManager::IsInitialized())
    return false;
  auto* user_manager = user_manager::UserManager::Get();
  if (user_manager->GetUsers().empty())
    return false;

  auto* device_owner =
      user_manager->FindUser(user_manager->GetOwnerAccountId());

  if (device_owner && device_owner->IsChild()) {
    return base::Contains(child_user_restrictions_, path);
  }

  return false;
}

}  // namespace chromeos
