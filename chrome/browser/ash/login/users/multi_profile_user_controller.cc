// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/users/multi_profile_user_controller.h"

#include <utility>

#include "ash/public/cpp/login_types.h"
#include "base/functional/bind.h"
#include "chrome/browser/ash/login/users/multi_profile_user_controller_delegate.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/sync_preferences/pref_service_syncable.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "google_apis/gaia/gaia_auth_util.h"

namespace ash {

namespace {

std::string SanitizeBehaviorValue(const std::string& value) {
  if (value == MultiProfileUserController::kBehaviorUnrestricted ||
      value == MultiProfileUserController::kBehaviorPrimaryOnly ||
      value == MultiProfileUserController::kBehaviorNotAllowed) {
    return value;
  }

  return std::string(MultiProfileUserController::kBehaviorUnrestricted);
}

bool SetUserAllowedReason(
    MultiProfileUserController::UserAllowedInSessionReason* reason,
    MultiProfileUserController::UserAllowedInSessionReason value) {
  if (reason)
    *reason = value;
  return value == MultiProfileUserController::ALLOWED;
}

}  // namespace

// static
const char MultiProfileUserController::kBehaviorUnrestricted[] = "unrestricted";
const char MultiProfileUserController::kBehaviorPrimaryOnly[] = "primary-only";
const char MultiProfileUserController::kBehaviorNotAllowed[] = "not-allowed";

// Note: this policy value is not a real one an is only returned locally for
// owner users instead of default one kBehaviorUnrestricted.
const char MultiProfileUserController::kBehaviorOwnerPrimaryOnly[] =
    "owner-primary-only";

MultiProfileUserController::MultiProfileUserController(
    MultiProfileUserControllerDelegate* delegate,
    PrefService* local_state)
    : delegate_(delegate), local_state_(local_state) {}

MultiProfileUserController::~MultiProfileUserController() {}

// static
void MultiProfileUserController::RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterDictionaryPref(prefs::kCachedMultiProfileUserBehavior);
}

// static
void MultiProfileUserController::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterStringPref(prefs::kMultiProfileUserBehavior,
                               kBehaviorUnrestricted);
  registry->RegisterBooleanPref(
      prefs::kMultiProfileNeverShowIntro, false,
      user_prefs::PrefRegistrySyncable::SYNCABLE_OS_PREF);
  registry->RegisterBooleanPref(
      prefs::kMultiProfileWarningShowDismissed, false,
      user_prefs::PrefRegistrySyncable::SYNCABLE_OS_PREF);
}

// static
MultiProfileUserController::UserAllowedInSessionReason
MultiProfileUserController::GetPrimaryUserPolicy() {
  user_manager::UserManager* user_manager = user_manager::UserManager::Get();
  CHECK(user_manager);

  const user_manager::User* user = user_manager->GetPrimaryUser();
  if (!user)
    return ALLOWED;

  Profile* profile = ProfileHelper::Get()->GetProfileByUser(user);
  if (!profile)
    return ALLOWED;

  // No user is allowed if the primary user policy forbids it.
  const std::string behavior =
      profile->GetPrefs()->GetString(prefs::kMultiProfileUserBehavior);
  if (behavior == kBehaviorNotAllowed)
    return NOT_ALLOWED_PRIMARY_USER_POLICY_FORBIDS;

  return ALLOWED;
}

// static
MultiProfileUserBehavior MultiProfileUserController::UserBehaviorStringToEnum(
    const std::string& behavior) {
  if (behavior == kBehaviorPrimaryOnly)
    return MultiProfileUserBehavior::PRIMARY_ONLY;
  if (behavior == kBehaviorNotAllowed)
    return MultiProfileUserBehavior::NOT_ALLOWED;
  if (behavior == kBehaviorOwnerPrimaryOnly)
    return MultiProfileUserBehavior::OWNER_PRIMARY_ONLY;

  return MultiProfileUserBehavior::UNRESTRICTED;
}

bool MultiProfileUserController::IsUserAllowedInSession(
    const std::string& user_email,
    MultiProfileUserController::UserAllowedInSessionReason* reason) const {
  user_manager::UserManager* user_manager = user_manager::UserManager::Get();
  CHECK(user_manager);

  const user_manager::User* primary_user = user_manager->GetPrimaryUser();
  std::string primary_user_email;
  if (primary_user)
    primary_user_email = primary_user->GetAccountId().GetUserEmail();

  // Always allow if there is no primary user or user being checked is the
  // primary user.
  if (primary_user_email.empty() || primary_user_email == user_email)
    return SetUserAllowedReason(reason, ALLOWED);

  UserAllowedInSessionReason primary_user_policy = GetPrimaryUserPolicy();
  if (primary_user_policy != ALLOWED)
    return SetUserAllowedReason(reason, primary_user_policy);

  // The user must have 'unrestricted' policy to be a secondary user.
  const std::string behavior = GetCachedValue(user_email);
  return SetUserAllowedReason(reason, behavior == kBehaviorUnrestricted
                                          ? ALLOWED
                                          : NOT_ALLOWED_POLICY_FORBIDS);
}

void MultiProfileUserController::StartObserving(Profile* user_profile) {
  // Profile name could be empty during tests.
  if (user_profile->GetProfileUserName().empty())
    return;

  std::unique_ptr<PrefChangeRegistrar> registrar(new PrefChangeRegistrar);
  registrar->Init(user_profile->GetPrefs());
  registrar->Add(
      prefs::kMultiProfileUserBehavior,
      base::BindRepeating(&MultiProfileUserController::OnUserPrefChanged,
                          base::Unretained(this), user_profile));
  pref_watchers_.push_back(std::move(registrar));

  OnUserPrefChanged(user_profile);
}

void MultiProfileUserController::RemoveCachedValues(
    const std::string& user_email) {
  ScopedDictPrefUpdate update(local_state_,
                              prefs::kCachedMultiProfileUserBehavior);
  update->Remove(user_email);
}

std::string MultiProfileUserController::GetCachedValue(
    const std::string& user_email) const {
  const base::Value::Dict& dict =
      local_state_->GetDict(prefs::kCachedMultiProfileUserBehavior);

  const std::string* value = dict.FindString(user_email);
  if (value)
    return SanitizeBehaviorValue(*value);

  return std::string(kBehaviorUnrestricted);
}

void MultiProfileUserController::SetCachedValue(const std::string& user_email,
                                                const std::string& behavior) {
  ScopedDictPrefUpdate update(local_state_,
                              prefs::kCachedMultiProfileUserBehavior);
  update->Set(user_email, SanitizeBehaviorValue(behavior));
}

void MultiProfileUserController::CheckSessionUsers() {
  const user_manager::UserList& users =
      user_manager::UserManager::Get()->GetLoggedInUsers();
  for (user_manager::UserList::const_iterator it = users.begin();
       it != users.end(); ++it) {
    if (!IsUserAllowedInSession((*it)->GetAccountId().GetUserEmail(),
                                nullptr)) {
      delegate_->OnUserNotAllowed((*it)->GetAccountId().GetUserEmail());
      return;
    }
  }
}

void MultiProfileUserController::OnUserPrefChanged(Profile* user_profile) {
  std::string user_email = user_profile->GetProfileUserName();
  CHECK(!user_email.empty());
  user_email = gaia::CanonicalizeEmail(user_email);

  PrefService* prefs = user_profile->GetPrefs();
  if (prefs->FindPreference(prefs::kMultiProfileUserBehavior)
          ->IsDefaultValue()) {
    // Migration code to clear cached default behavior.
    // TODO(xiyuan): Remove this after M35.
    ScopedDictPrefUpdate update(local_state_,
                                prefs::kCachedMultiProfileUserBehavior);
    update->Remove(user_email);
  } else {
    const std::string behavior =
        prefs->GetString(prefs::kMultiProfileUserBehavior);
    SetCachedValue(user_email, behavior);
  }

  CheckSessionUsers();
}

}  // namespace ash
