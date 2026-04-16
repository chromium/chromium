// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/auth_factors_policy/local_auth_factors_policy_controller.h"

#include <memory>
#include <optional>

#include "ash/constants/ash_pref_names.h"
#include "ash/public/cpp/reauth_reason.h"
#include "base/check.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_forward.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/no_destructor.h"
#include "chrome/browser/ash/login/quick_unlock/quick_unlock_utils.h"
#include "chrome/browser/ash/login/reauth_stats.h"
#include "chromeos/ash/components/login/auth/auth_factor_editor.h"
#include "chromeos/ash/components/login/auth/public/authentication_error.h"
#include "chromeos/ash/components/login/auth/public/user_context.h"
#include "chromeos/ash/components/osauth/public/auth_policy_connector.h"
#include "chromeos/ash/components/osauth/public/auth_policy_utils.h"
#include "chromeos/ash/components/osauth/public/common_types.h"
#include "chromeos/ash/services/auth_factor_config/auth_factor_config_utils.h"
#include "components/account_id/account_id.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_service.h"
#include "components/session_manager/core/session.h"
#include "components/session_manager/core/session_manager.h"
#include "components/session_manager/session_manager_types.h"
#include "components/user_manager/user_manager.h"

namespace ash {

namespace {

base::RepeatingClosure& GetOnPrefProcessedClosure() {
  static base::NoDestructor<base::RepeatingClosure> on_pref_processed;
  return *on_pref_processed;
}

}  // namespace

// static
void LocalAuthFactorsPolicyController::SetPrefProcessedCallbackForTesting(
    base::RepeatingClosure on_pref_processed) {
  GetOnPrefProcessedClosure() = std::move(on_pref_processed);
}

PrefService& LocalAuthFactorsPolicyController::prefs() {
  return *pref_change_registrar_.prefs();
}

LocalAuthFactorsPolicyController::LocalAuthFactorsPolicyController(
    PrefService& pref_service,
    const AccountId& account_id)
    : account_id_(account_id) {
  pref_change_registrar_.Init(&pref_service);
  // `base::Unretained(this)` is safe as `this` outlives the registrar.
  pref_change_registrar_.Add(
      ash::prefs::kAllowedLocalAuthFactors,
      base::BindRepeating(
          &LocalAuthFactorsPolicyController::OnAllowedAuthFactorsPrefUpdated,
          base::Unretained(this)));
  pref_change_registrar_.Add(
      ash::prefs::kQuickUnlockModeAllowlist,
      base::BindRepeating(
          &LocalAuthFactorsPolicyController::OnAllowedAuthFactorsPrefUpdated,
          base::Unretained(this)));
  OnAllowedAuthFactorsPrefUpdated();
}

LocalAuthFactorsPolicyController::~LocalAuthFactorsPolicyController() = default;

void LocalAuthFactorsPolicyController::OnAllowedAuthFactorsPrefUpdated() {
  base::ScopedClosureRunner pref_processed_runner(GetOnPrefProcessedClosure());
  if (!prefs().IsManagedPreference(ash::prefs::kAllowedLocalAuthFactors)) {
    // If the pref is not managed, it means the admin has not set a policy, and
    // thus no action is needed from this handler. Also, it prevents unintended
    // behavior if the preference were to be modified by non-policy means.
    return;
  }

  auto allowed_auth_factors_set = GetAllowedAuthFactors();

  // Early return in case the policy is not restricting local auth factors.
  if (!allowed_auth_factors_set.has_value() ||
      !allowed_auth_factors_set->empty()) {
    return;
  }

  // We need to determine whether the user has a local knowledge factor setup or
  // not to decide whether to show online reauth on lockscreen.
  auto user_context = std::make_unique<ash::UserContext>();
  user_context->SetAccountId(account_id_);
  GetAuthFactorEditor()->GetAuthFactorsConfiguration(
      std::move(user_context),
      base::BindOnce(
          &LocalAuthFactorsPolicyController::OnGetAuthFactorsConfiguration,
          weak_factory_.GetWeakPtr(), std::move(pref_processed_runner)));
}

void LocalAuthFactorsPolicyController::OnGetAuthFactorsConfiguration(
    base::ScopedClosureRunner pref_processed_runner,
    std::unique_ptr<ash::UserContext> user_context,
    std::optional<ash::AuthenticationError> error) {
  if (error.has_value()) {
    LOG(ERROR) << "Failed to get auth factors: "
               << error->get_cryptohome_error();
    return;
  }
  CHECK(user_context);
  const auto& config = user_context->GetAuthFactorsConfiguration();
  auto* password_factor =
      config.FindFactorByType(cryptohome::AuthFactorType::kPassword);
  auto* pin_factor = config.FindFactorByType(cryptohome::AuthFactorType::kPin);

  bool pin_is_secondary_and_allowed =
      pin_factor && password_factor &&
      ash::auth::IsGaiaPassword(*password_factor) &&
      !ash::quick_unlock::IsPinDisabledByPolicy(
          pref_change_registrar_.prefs(), ash::quick_unlock::Purpose::kUnlock);

  bool has_local_auth_factors =
      (pin_factor && !pin_is_secondary_and_allowed) ||
      (password_factor && ash::auth::IsLocalPassword(*password_factor));

  if (has_local_auth_factors) {
    user_manager::UserManager::Get()->SaveForceOnlineSignin(
        user_context->GetAccountId(), /*force_online_signin=*/true);
    ash::RecordReauthReason(user_context->GetAccountId(),
                            ash::ReauthReason::kForcedByLocalAuthFactorsPolicy);
  }
  VLOG(1) << "Local auth factors check. Forced online signin: "
          << has_local_auth_factors;
}

AuthFactorEditor* LocalAuthFactorsPolicyController::GetAuthFactorEditor() {
  if (!auth_factor_editor_) {
    auth_factor_editor_ =
        std::make_unique<ash::AuthFactorEditor>(ash::UserDataAuthClient::Get());
  }
  return auth_factor_editor_.get();
}

std::optional<ash::AuthFactorsSet>
LocalAuthFactorsPolicyController::GetAllowedAuthFactors() {
  auto& allowed_auth_factors =
      prefs().GetList(ash::prefs::kAllowedLocalAuthFactors);
  return ash::GetAuthFactorsSetFromPolicyList(&allowed_auth_factors);
}

}  // namespace ash
