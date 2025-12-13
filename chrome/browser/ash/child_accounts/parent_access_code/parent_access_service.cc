// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/child_accounts/parent_access_code/parent_access_service.h"

#include <string>
#include <utility>

#include "ash/public/cpp/child_accounts/parent_access_controller.h"
#include "base/check.h"
#include "base/check_deref.h"
#include "base/check_op.h"
#include "base/containers/contains.h"
#include "base/logging.h"
#include "base/no_destructor.h"
#include "base/timer/timer.h"
#include "base/values.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"

namespace ash {
namespace parent_access {

namespace {

static ParentAccessService* g_instance = nullptr;

// Returns true when the device owner is a child.
bool IsDeviceOwnedByChild() {
  AccountId owner_account_id =
      user_manager::UserManager::Get()->GetOwnerAccountId();
  if (owner_account_id.empty()) {
    LOG(ERROR) << "Device owner could not be determined - will skip parent "
                  "code validation";
    return false;
  }

  const user_manager::User* device_owner =
      user_manager::UserManager::Get()->FindUser(owner_account_id);

  // It looks like reading users from Local State might be failing sometimes.
  // Default to false if ownership is not known to avoid crash.
  // TODO(agawronska): Investigate if it can be improved. Defaulting to false
  // could sometimes lead to skipping parent code validation when child is the
  // device owner.
  if (!device_owner) {
    LOG(ERROR) << "Device owner could not be determined - will skip parent "
                  "code validation";
    return false;
  }
  return device_owner->IsChild();
}

}  // namespace

// static
void ParentAccessService::RegisterProfilePrefs(PrefRegistrySimple* registry) {
  registry->RegisterDictionaryPref(prefs::kParentAccessCodeConfig);
}

// static
ParentAccessService& ParentAccessService::Get() {
  return CHECK_DEREF(g_instance);
}

// static
bool ParentAccessService::IsApprovalRequired(SupervisedAction action) {
  switch (action) {
    case SupervisedAction::kUpdateClock:
    case SupervisedAction::kUpdateTimezone:
      if (user_manager::UserManager::Get()->IsUserLoggedIn()) {
        return user_manager::UserManager::Get()->GetActiveUser()->IsChild();
      }
      return IsDeviceOwnedByChild();
    case SupervisedAction::kAddUser:
      return IsDeviceOwnedByChild();
    case SupervisedAction::kUnlockTimeLimits:
      DCHECK(user_manager::UserManager::Get()->IsUserLoggedIn());
      return true;
  }
}

ParentAccessService::ParentAccessService(PrefService* local_state)
    : config_source_(local_state) {
  CHECK(!g_instance);
  g_instance = this;
}

ParentAccessService::~ParentAccessService() {
  CHECK_EQ(g_instance, this);
  g_instance = nullptr;
}

ParentCodeValidationResult ParentAccessService::ValidateParentAccessCode(
    const AccountId& account_id,
    const std::string& access_code,
    base::Time validation_time) {
  ParentCodeValidationResult result = ParentCodeValidationResult::kInvalid;

  if (config_source_.config_map().empty() ||
      (account_id.is_valid() &&
       !base::Contains(config_source_.config_map(), account_id))) {
    result = ParentCodeValidationResult::kNoConfig;
    NotifyObservers(result, account_id);
    return result;
  }

  for (const auto& map_entry : config_source_.config_map()) {
    if (!account_id.is_valid() || account_id == map_entry.first) {
      for (const auto& validator : map_entry.second) {
        if (validator->Validate(access_code, validation_time)) {
          result = ParentCodeValidationResult::kValid;
          NotifyObservers(result, account_id);
          return result;
        }
      }
    }
  }

  NotifyObservers(result, account_id);
  return result;
}

void ParentAccessService::UpdateConfigForUser(
    const AccountId& account_id,
    std::optional<base::Value::Dict> config) {
  if (config) {
    config_source_.UpdateConfigForUser(account_id, std::move(config.value()));
  } else {
    config_source_.RemoveConfigForUser(account_id);
  }
}

void ParentAccessService::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void ParentAccessService::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void ParentAccessService::NotifyObservers(
    ParentCodeValidationResult validation_result,
    const AccountId& account_id) {
  for (auto& observer : observers_)
    observer.OnAccessCodeValidation(validation_result, account_id);
}

}  // namespace parent_access
}  // namespace ash
