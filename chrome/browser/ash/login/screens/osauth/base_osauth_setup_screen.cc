// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/screens/osauth/base_osauth_setup_screen.h"

#include <memory>
#include <utility>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "chrome/browser/ash/login/oobe_screen.h"
#include "chrome/browser/ash/login/screens/base_screen.h"
#include "chrome/browser/ash/login/wizard_context.h"
#include "chromeos/ash/components/cryptohome/auth_factor.h"
#include "chromeos/ash/components/login/auth/public/user_context.h"
#include "chromeos/ash/components/osauth/public/auth_session_storage.h"
#include "chromeos/ash/components/osauth/public/common_types.h"

namespace ash {

BaseOSAuthSetupScreen::BaseOSAuthSetupScreen(OobeScreenId screen_id,
                                             OobeScreenPriority screen_priority)
    : BaseScreen(screen_id, screen_priority) {}

BaseOSAuthSetupScreen::~BaseOSAuthSetupScreen() = default;

void BaseOSAuthSetupScreen::HideImpl() {
  session_refresher_.reset();
}

AuthProofToken BaseOSAuthSetupScreen::GetToken() {
  CHECK(context()->extra_factors_token.has_value());
  return *(context()->extra_factors_token);
}

void BaseOSAuthSetupScreen::KeepAliveAuthSession() {
  session_refresher_ = AuthSessionStorage::Get()->KeepAlive(GetToken());
}

void BaseOSAuthSetupScreen::InspectContextAndContinue(
    InspectContextCallback inspect_callback,
    base::OnceClosure continuation) {
  if (!context()->extra_factors_token.has_value()) {
    std::move(inspect_callback).Run(nullptr);
    return;
  }
  auto token = GetToken();
  CHECK(AuthSessionStorage::Get()->IsValid(token));
  AuthSessionStorage::Get()->BorrowAsync(
      FROM_HERE, token,
      base::BindOnce(
          &BaseOSAuthSetupScreen::InspectContextAndContinueWithContext,
          weak_ptr_factory_.GetWeakPtr(), std::move(inspect_callback),
          std::move(continuation)));
}

void BaseOSAuthSetupScreen::InspectContextAndContinueWithContext(
    InspectContextCallback inspect_callback,
    base::OnceClosure continuation,
    std::unique_ptr<UserContext> user_context) {
  std::move(inspect_callback).Run(user_context.get());
  if (!user_context) {
    return;
  }
  AuthSessionStorage::Get()->Return(GetToken(), std::move(user_context));
  std::move(continuation).Run();
}

void BaseOSAuthSetupScreen::EstablishKnowledgeFactorGuard(
    base::OnceClosure continuation) {
  auto split_continuation = base::SplitOnceCallback(std::move(continuation));
  InspectContextAndContinue(
      base::BindOnce(&BaseOSAuthSetupScreen::CheckForKnowledgeFactorPresence,
                     weak_ptr_factory_.GetWeakPtr(),
                     std::move(split_continuation.first)),
      std::move(split_continuation.second));
}

void BaseOSAuthSetupScreen::CheckForKnowledgeFactorPresence(
    base::OnceClosure continuation,
    UserContext* context) {
  if (!context) {
    std::move(continuation).Run();
    return;
  }
  // Until we support PIN-only setups, check only for Password factor type.
  bool has_knowledge_factor =
      context->GetAuthFactorsConfiguration().HasConfiguredFactor(
          cryptohome::AuthFactorType::kPassword);

  if (!has_knowledge_factor) {
    KeepAliveAuthSession();
  }
}

}  // namespace ash
