// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/osauth/auth_policy_enforcer.h"

#include <memory>
#include <optional>
#include <utility>

#include "chromeos/ash/components/login/auth/auth_factor_editor.h"
#include "chromeos/ash/components/login/auth/public/auth_session_intent.h"
#include "chromeos/ash/components/login/auth/public/authentication_error.h"
#include "chromeos/ash/components/login/auth/public/user_context.h"
#include "components/user_manager/known_user.h"

namespace ash {

AuthPolicyEnforcer::AuthPolicyEnforcer(AuthPolicyConnector* connector,
                                       UserDataAuthClient* user_data_auth,
                                       PrefService* local_state)
    : connector_(connector),
      user_data_auth_(user_data_auth),
      local_state_(local_state) {}
AuthPolicyEnforcer::~AuthPolicyEnforcer() = default;

void AuthPolicyEnforcer::CheckAndEnforcePolicies(
    std::unique_ptr<UserContext> context,
    AuthOperationCallback callback) {
  CHECK(!context->GetAuthSessionId().empty());
  CHECK(context->GetAuthorizedIntents().Has(ash::AuthSessionIntent::kDecrypt));

  AuthFactorsSet policy_controlled_factors;
  DetermineAffectedFactors(context->GetAccountId(), policy_controlled_factors);
  if (policy_controlled_factors.empty()) {
    std::move(callback).Run(std::move(context), std::nullopt);
    return;
  }

  editor_ = std::make_unique<AuthFactorEditor>(user_data_auth_);
  if (!context->HasAuthFactorsConfiguration()) {
    editor_->GetAuthFactorsConfiguration(
        std::move(context),
        base::BindOnce(&AuthPolicyEnforcer::OnAuthFactorConfigurationLoaded,
                       weak_factory_.GetWeakPtr(), std::move(callback)));
    return;
  }
  EnforceRecoveryPolicies(std::move(context), std::move(callback));
}

void AuthPolicyEnforcer::DetermineAffectedFactors(const AccountId& account_id,
                                                  AuthFactorsSet& out_factors) {
  if (connector_->GetRecoveryMandatoryState(account_id).has_value()) {
    out_factors.Put(AshAuthFactor::kRecovery);
  }
}

void AuthPolicyEnforcer::OnAuthFactorConfigurationLoaded(
    AuthOperationCallback callback,
    std::unique_ptr<UserContext> context,
    std::optional<AuthenticationError> error) {
  if (error.has_value()) {
    std::move(callback).Run(std::move(context), error);
    return;
  }
  EnforceRecoveryPolicies(std::move(context), std::move(callback));
}

void AuthPolicyEnforcer::EnforceRecoveryPolicies(
    std::unique_ptr<UserContext> context,
    AuthOperationCallback callback) {
  auto mandatory_recovery =
      connector_->GetRecoveryMandatoryState(context->GetAccountId());
  if (!mandatory_recovery.has_value()) {
    OnPolicesApplied(std::move(context), std::move(callback));
    return;
  }
  bool has_recovery =
      context->GetAuthFactorsConfiguration().HasConfiguredFactor(
          cryptohome::AuthFactorType::kRecovery);
  if (has_recovery == *mandatory_recovery) {
    OnPolicesApplied(std::move(context), std::move(callback));
    return;
  }

  if (*mandatory_recovery) {
    LOG(WARNING) << "Recovery factor mandated by policy";
    if (context->GetDeviceId().empty()) {
      user_manager::KnownUser known_user(local_state_);
      std::string device_id = known_user.GetDeviceId(context->GetAccountId());
      if (device_id.empty()) {
        LOG(ERROR) << "Can not enforce recovery factor: no device ID.";
        OnPolicesApplied(std::move(context), std::move(callback));
        return;
      }
      context->SetDeviceId(device_id);
    }
    editor_->AddRecoveryFactor(
        std::move(context),
        base::BindOnce(&AuthPolicyEnforcer::OnRecoveryUpdated,
                       weak_factory_.GetWeakPtr(), std::move(callback)));
  } else {
    LOG(WARNING) << "Recovery factor prohibited by policy";
    editor_->RemoveRecoveryFactor(
        std::move(context),
        base::BindOnce(&AuthPolicyEnforcer::OnRecoveryUpdated,
                       weak_factory_.GetWeakPtr(), std::move(callback)));
  }
}

void AuthPolicyEnforcer::OnRecoveryUpdated(
    AuthOperationCallback callback,
    std::unique_ptr<UserContext> context,
    std::optional<AuthenticationError> error) {
  if (error.has_value()) {
    LOG(ERROR) << "Failed to enforce recovery factor policy "
               << error->ToDebugString();
    std::move(callback).Run(std::move(context), error);
    return;
  }
  LOG(WARNING) << "Recovery factor policy applied";
  OnPolicesApplied(std::move(context), std::move(callback));
}

void AuthPolicyEnforcer::OnPolicesApplied(std::unique_ptr<UserContext> context,
                                          AuthOperationCallback callback) {
  std::move(callback).Run(std::move(context), std::nullopt);
}

}  // namespace ash
