// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/osauth/profile_prefs_auth_policy_connector.h"

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "base/containers/contains.h"
#include "base/notreached.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "components/prefs/pref_service.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"

namespace ash {

namespace {

bool IsUserManaged(const AccountId& account) {
  const user_manager::User* user =
      user_manager::UserManager::Get()->FindUser(account);
  CHECK(user);
  Profile* profile = ash::ProfileHelper::Get()->GetProfileByUser(user);
  CHECK(profile);
  return profile->GetProfilePolicyConnector()->IsManaged();
}

PrefService* GetPrefsForUser(const AccountId& account) {
  const user_manager::User* user =
      user_manager::UserManager::Get()->FindUser(account);
  CHECK(user);
  Profile* profile = ash::ProfileHelper::Get()->GetProfileByUser(user);
  CHECK(profile);
  return profile->GetPrefs();
}

}  // namespace

ProfilePrefsAuthPolicyConnector::ProfilePrefsAuthPolicyConnector() = default;

ProfilePrefsAuthPolicyConnector::~ProfilePrefsAuthPolicyConnector() = default;

void ProfilePrefsAuthPolicyConnector::SetLoginScreenAuthPolicyConnector(
    AuthPolicyConnector* connector) {
  login_screen_connector_ = connector;
}

std::optional<bool> ProfilePrefsAuthPolicyConnector::GetRecoveryInitialState(
    const AccountId& account) {
  return !IsUserManaged(account);
}

std::optional<bool> ProfilePrefsAuthPolicyConnector::GetRecoveryDefaultState(
    const AccountId& account) {
  if (IsUserManaged(account)) {
    return GetPrefsForUser(account)->GetBoolean(
        ash::prefs::kRecoveryFactorBehavior);
  }

  // For non-managed users the default state is "disabled".
  return false;
}

std::optional<bool> ProfilePrefsAuthPolicyConnector::GetRecoveryMandatoryState(
    const AccountId& account) {
  if (!IsUserManaged(account)) {
    return std::nullopt;
  }
  auto* prefs = GetPrefsForUser(account);
  auto* pref = prefs->FindPreference(prefs::kRecoveryFactorBehavior);
  if (!pref || !pref->IsManaged() || pref->IsRecommended()) {
    return std::nullopt;
  }
  return pref->GetValue()->GetBool();
}

bool ProfilePrefsAuthPolicyConnector::IsAuthFactorManaged(
    const AccountId& account,
    AshAuthFactor auth_factor) {
  auto* prefs = GetPrefsForUser(account);
  switch (auth_factor) {
    case AshAuthFactor::kRecovery: {
      return prefs->IsManagedPreference(prefs::kRecoveryFactorBehavior);
    }
    case AshAuthFactor::kLegacyPin: {
      return prefs->IsManagedPreference(prefs::kQuickUnlockModeAllowlist) ||
             prefs->IsManagedPreference(prefs::kWebAuthnFactors);
    }
    default:
      NOTIMPLEMENTED();
  }
  return false;
}

bool ProfilePrefsAuthPolicyConnector::IsAuthFactorUserModifiable(
    const AccountId& account,
    AshAuthFactor auth_factor) {
  auto* prefs = GetPrefsForUser(account);
  switch (auth_factor) {
    case AshAuthFactor::kRecovery: {
      return prefs->IsUserModifiablePreference(prefs::kRecoveryFactorBehavior);
    }
    case AshAuthFactor::kLegacyPin: {
      // Lists of factors that are allowed for some purpose.
      const base::Value::List* pref_lists[] = {
          &prefs->GetList(prefs::kQuickUnlockModeAllowlist),
          &prefs->GetList(prefs::kWebAuthnFactors),
      };

      // Values in factor lists that match PINs.
      const base::Value pref_list_values[] = {
          base::Value("all"),
          base::Value("PIN"),
      };

      for (const auto* pref_list : pref_lists) {
        for (const auto& pref_list_value : pref_list_values) {
          if (base::Contains(*pref_list, pref_list_value)) {
            return true;
          }
        }
      }
      return false;
    }
    default:
      NOTIMPLEMENTED();
  }
  return false;
}

void ProfilePrefsAuthPolicyConnector::OnShutdown() {
  login_screen_connector_ = nullptr;
}

}  // namespace ash
