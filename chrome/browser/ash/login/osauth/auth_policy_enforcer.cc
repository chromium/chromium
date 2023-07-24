// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/osauth/auth_policy_enforcer.h"

#include <memory>
#include <utility>

#include "chromeos/ash/components/login/auth/auth_factor_editor.h"
#include "chromeos/ash/components/login/auth/public/auth_session_intent.h"
#include "chromeos/ash/components/login/auth/public/user_context.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace ash {

AuthPolicyEnforcer::AuthPolicyEnforcer(AuthPolicyConnector* connector,
                                       UserDataAuthClient* user_data_auth)
    : connector_(connector), user_data_auth_(user_data_auth) {}
AuthPolicyEnforcer::~AuthPolicyEnforcer() = default;

void AuthPolicyEnforcer::CheckAndEnforcePolicies(
    std::unique_ptr<UserContext> context,
    AuthOperationCallback callback) {
  CHECK(!context->GetAuthSessionId().empty());
  CHECK(context->GetAuthorizedIntents().Has(ash::AuthSessionIntent::kDecrypt));

  AuthFactorsSet policy_controlled_factors;
  DetermineAffectedFactors(context->GetAccountId(), policy_controlled_factors);
  if (policy_controlled_factors.Empty()) {
    std::move(callback).Run(std::move(context), absl::nullopt);
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
    absl::optional<AuthenticationError> error) {
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
    editor_->AddRecoveryFactor(
        std::move(context),
        base::BindOnce(&AuthPolicyEnforcer::OnRecoveryUpdated,
                       weak_factory_.GetWeakPtr(), std::move(callback)));
  } else {
    editor_->RemoveRecoveryFactor(
        std::move(context),
        base::BindOnce(&AuthPolicyEnforcer::OnRecoveryUpdated,
                       weak_factory_.GetWeakPtr(), std::move(callback)));
  }
}

void AuthPolicyEnforcer::OnRecoveryUpdated(
    AuthOperationCallback callback,
    std::unique_ptr<UserContext> context,
    absl::optional<AuthenticationError> error) {
  if (error.has_value()) {
    std::move(callback).Run(std::move(context), error);
    return;
  }
  OnPolicesApplied(std::move(context), std::move(callback));
}

void AuthPolicyEnforcer::OnPolicesApplied(std::unique_ptr<UserContext> context,
                                          AuthOperationCallback callback) {
  std::move(callback).Run(std::move(context), absl::nullopt);
}

}  // namespace ash
