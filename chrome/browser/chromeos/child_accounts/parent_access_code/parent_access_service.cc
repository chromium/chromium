// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/child_accounts/parent_access_code/parent_access_service.h"

#include <string>
#include <utility>

#include "base/logging.h"
#include "base/no_destructor.h"
#include "base/timer/timer.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"

namespace chromeos {
namespace parent_access {

namespace {

// Returns true when the device owner is a child.
bool IsDeviceOwnedByChild() {
  AccountId owner_account_id =
      user_manager::UserManager::Get()->GetOwnerAccountId();
  if (owner_account_id.empty())
    return false;
  const user_manager::User* device_owner =
      user_manager::UserManager::Get()->FindUser(owner_account_id);
  CHECK(device_owner);
  return device_owner->IsChild();
}

}  // namespace

// static
void ParentAccessService::RegisterProfilePrefs(PrefRegistrySimple* registry) {
  registry->RegisterDictionaryPref(prefs::kParentAccessCodeConfig);
}

// static
ParentAccessService& ParentAccessService::Get() {
  static base::NoDestructor<ParentAccessService> instance;
  return *instance;
}

// static
bool ParentAccessService::IsApprovalRequired(SupervisedAction action) {
  switch (action) {
    case SupervisedAction::kUpdateClock:
    case SupervisedAction::kUpdateTimezone:
      if (!base::FeatureList::IsEnabled(
              features::kParentAccessCodeForTimeChange)) {
        return false;
      }
      if (user_manager::UserManager::Get()->IsUserLoggedIn())
        return user_manager::UserManager::Get()->GetActiveUser()->IsChild();
      return IsDeviceOwnedByChild();
  }
}

ParentAccessService::ParentAccessService() = default;

ParentAccessService::~ParentAccessService() = default;

bool ParentAccessService::ValidateParentAccessCode(
    const AccountId& account_id,
    const std::string& access_code,
    base::Time validation_time) {
  bool validation_result = false;
  for (const auto& map_entry : config_source_.config_map()) {
    if (!account_id.is_valid() || account_id == map_entry.first) {
      for (const auto& validator : map_entry.second) {
        if (validator->Validate(access_code, validation_time)) {
          validation_result = true;
          break;
        }
      }
    }
    if (validation_result)
      break;
  }

  for (auto& observer : observers_)
    observer.OnAccessCodeValidation(validation_result, account_id);

  return validation_result;
}

void ParentAccessService::LoadConfigForUser(const user_manager::User* user) {
  config_source_.LoadConfigForUser(user);
}

void ParentAccessService::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void ParentAccessService::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

}  // namespace parent_access
}  // namespace chromeos
