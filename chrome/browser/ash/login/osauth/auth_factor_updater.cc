// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/osauth/auth_factor_updater.h"

#include <memory>
#include <optional>
#include <utility>

#include "chrome/browser/ash/login/osauth/auth_factor_migrator.h"
#include "chrome/browser/ash/login/osauth/auth_policy_enforcer.h"
#include "chromeos/ash/components/login/auth/auth_events_recorder.h"
#include "chromeos/ash/components/login/auth/public/authentication_error.h"
#include "chromeos/ash/components/login/auth/public/user_context.h"
#include "chromeos/ash/components/login/auth/recovery/service_constants.h"
#include "components/user_manager/known_user.h"

namespace ash {

AuthFactorUpdater::AuthFactorUpdater(AuthPolicyConnector* connector,
                                     UserDataAuthClient* user_data_auth,
                                     PrefService* local_state)
    : connector_(connector),
      user_data_auth_(user_data_auth),
      local_state_(local_state) {}

AuthFactorUpdater::~AuthFactorUpdater() = default;

void AuthFactorUpdater::Run(std::unique_ptr<UserContext> context,
                            AuthOperationCallback callback) {
  CHECK(!context->GetAuthSessionId().empty());
  CHECK(context->GetAuthorizedIntents().Has(ash::AuthSessionIntent::kDecrypt));

  auth_factor_migrator_ = std::make_unique<AuthFactorMigrator>(
      AuthFactorMigrator::GetMigrationsList(user_data_auth_));
  AuthEventsRecorder::Get()->OnFactorUpdateStarted();
  auth_factor_migrator_->Run(
      std::move(context),
      base::BindOnce(&AuthFactorUpdater::OnMigratorRun,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void AuthFactorUpdater::OnMigratorRun(
    AuthOperationCallback callback,
    std::unique_ptr<UserContext> context,
    std::optional<AuthenticationError> error) {
  if (error.has_value()) {
    LOG(ERROR) << "Failed to run migrations " << error->ToDebugString();
    // Proceed to enforce policies anyways.
  }
  AuthEventsRecorder::Get()->OnMigrationsCompleted();
  auth_policy_enforcer_ = std::make_unique<AuthPolicyEnforcer>(
      connector_, user_data_auth_, local_state_);
  auth_policy_enforcer_->CheckAndEnforcePolicies(std::move(context),
                                                 std::move(callback));
}

}  // namespace ash
