// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/users/multi_profile_user_controller.h"

#include <utility>

#include "ash/public/cpp/login_types.h"
#include "base/functional/bind.h"
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

// TODO(b/278643115) Remove the using when moved.
using user_manager::kMultiProfileUserBehaviorPref;
using user_manager::MultiUserSignInPolicy;
using user_manager::MultiUserSignInPolicyToPrefValue;
using user_manager::ParseMultiUserSignInPolicyPref;

namespace {

bool SetUserAllowedReason(
    MultiProfileUserController::UserAllowedInSessionReason* reason,
    MultiProfileUserController::UserAllowedInSessionReason value) {
  if (reason) {
    *reason = value;
  }
  return value == MultiProfileUserController::ALLOWED;
}

}  // namespace

MultiProfileUserController::MultiProfileUserController(
    PrefService* local_state,
    user_manager::UserManager* user_manager)
    : local_state_(local_state), user_manager_(user_manager) {}

MultiProfileUserController::~MultiProfileUserController() = default;

// static
void MultiProfileUserController::RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterDictionaryPref(prefs::kCachedMultiProfileUserBehavior);
}

// static
void MultiProfileUserController::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterStringPref(kMultiProfileUserBehaviorPref,
                               std::string(MultiUserSignInPolicyToPrefValue(
                                   MultiUserSignInPolicy::kUnrestricted)));
  registry->RegisterBooleanPref(
      prefs::kMultiProfileNeverShowIntro, false,
      user_prefs::PrefRegistrySyncable::SYNCABLE_OS_PREF);
  registry->RegisterBooleanPref(
      prefs::kMultiProfileWarningShowDismissed, false,
      user_prefs::PrefRegistrySyncable::SYNCABLE_OS_PREF);
}

void MultiProfileUserController::Shutdown() {
  pref_watchers_.clear();
}

MultiProfileUserController::UserAllowedInSessionReason
MultiProfileUserController::GetPrimaryUserPolicy() const {
  const user_manager::User* user = user_manager_->GetPrimaryUser();
  if (!user) {
    return ALLOWED;
  }

  Profile* profile = ProfileHelper::Get()->GetProfileByUser(user);
  if (!profile) {
    return ALLOWED;
  }

  // No user is allowed if the primary user policy forbids it.
  auto value = ParseMultiUserSignInPolicyPref(
      profile->GetPrefs()->GetString(kMultiProfileUserBehaviorPref));
  if (value == MultiUserSignInPolicy::kNotAllowed) {
    return NOT_ALLOWED_PRIMARY_USER_POLICY_FORBIDS;
  }

  return ALLOWED;
}

bool MultiProfileUserController::IsUserAllowedInSession(
    const std::string& user_email,
    MultiProfileUserController::UserAllowedInSessionReason* reason) const {
  const user_manager::User* primary_user = user_manager_->GetPrimaryUser();
  std::string primary_user_email;
  if (primary_user) {
    primary_user_email = primary_user->GetAccountId().GetUserEmail();
  }

  // Always allow if there is no primary user or user being checked is the
  // primary user.
  if (primary_user_email.empty() || primary_user_email == user_email) {
    return SetUserAllowedReason(reason, ALLOWED);
  }

  UserAllowedInSessionReason primary_user_policy = GetPrimaryUserPolicy();
  if (primary_user_policy != ALLOWED) {
    return SetUserAllowedReason(reason, primary_user_policy);
  }

  // The user must have 'unrestricted' policy to be a secondary user.
  const auto policy = GetCachedValue(user_email);
  return SetUserAllowedReason(reason,
                              policy == MultiUserSignInPolicy::kUnrestricted
                                  ? ALLOWED
                                  : NOT_ALLOWED_POLICY_FORBIDS);
}

void MultiProfileUserController::StartObserving(Profile* user_profile) {
  // Profile name could be empty during tests.
  if (user_profile->GetProfileUserName().empty()) {
    return;
  }

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
    std::string_view user_email) {
  ScopedDictPrefUpdate update(local_state_,
                              prefs::kCachedMultiProfileUserBehavior);
  update->Remove(user_email);
}

MultiUserSignInPolicy MultiProfileUserController::GetCachedValue(
    std::string_view user_email) const {
  const base::Value::Dict& dict =
      local_state_->GetDict(prefs::kCachedMultiProfileUserBehavior);

  const std::string* value = dict.FindString(user_email);
  if (!value) {
    return MultiUserSignInPolicy::kUnrestricted;
  }

  return ParseMultiUserSignInPolicyPref(*value).value_or(
      MultiUserSignInPolicy::kUnrestricted);
}

void MultiProfileUserController::SetCachedValue(std::string_view user_email,
                                                MultiUserSignInPolicy policy) {
  ScopedDictPrefUpdate update(local_state_,
                              prefs::kCachedMultiProfileUserBehavior);
  update->Set(user_email, MultiUserSignInPolicyToPrefValue(policy));
}

void MultiProfileUserController::CheckSessionUsers() {
  for (const user_manager::User* user : user_manager_->GetLoggedInUsers()) {
    const std::string& user_email = user->GetAccountId().GetUserEmail();
    if (!IsUserAllowedInSession(user_email, /*reason=*/nullptr)) {
      user_manager_->NotifyUserNotAllowed(user_email);
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
    auto policy = ParseMultiUserSignInPolicyPref(
        prefs->GetString(kMultiProfileUserBehaviorPref));
    SetCachedValue(user_email,
                   policy.value_or(MultiUserSignInPolicy::kUnrestricted));
  }

  CheckSessionUsers();
}

}  // namespace ash
