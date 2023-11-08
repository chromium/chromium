// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/screens/osauth/base_osauth_setup_screen.h"

#include <memory>
#include <utility>

#include "ash/constants/ash_features.h"
#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "chrome/browser/ash/login/quick_unlock/quick_unlock_factory.h"
#include "chrome/browser/ash/login/quick_unlock/quick_unlock_storage.h"
#include "chrome/browser/ash/login/screens/base_screen.h"
#include "chrome/browser/ash/login/wizard_context.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chromeos/ash/components/osauth/public/auth_session_storage.h"

namespace ash {

BaseOSAuthSetupScreen::BaseOSAuthSetupScreen(OobeScreenId screen_id,
                                             OobeScreenPriority screen_priority)
    : BaseScreen(screen_id, screen_priority) {}

BaseOSAuthSetupScreen::~BaseOSAuthSetupScreen() = default;

void BaseOSAuthSetupScreen::HideImpl() {
  if (!ash::features::ShouldUseAuthSessionStorage()) {
    StoreQuickUnlockContext();
  }
  session_refresher_.reset();
}

void BaseOSAuthSetupScreen::EnsureQuickUnlockToken() {
  CHECK(!ash::features::ShouldUseAuthSessionStorage());
  if (quick_unlock_token_) {
    return;
  }
  CHECK(context()->extra_factors_auth_session);

  quick_unlock::QuickUnlockStorage* quick_unlock_storage =
      quick_unlock::QuickUnlockFactory::GetForProfile(
          ProfileManager::GetActiveUserProfile());
  CHECK(quick_unlock_storage);
  quick_unlock_token_ = quick_unlock_storage->CreateAuthToken(
      *context()->extra_factors_auth_session);
}

void BaseOSAuthSetupScreen::StoreQuickUnlockContext() {
  CHECK(!ash::features::ShouldUseAuthSessionStorage());
  if (!quick_unlock_token_) {
    return;
  }
  quick_unlock::QuickUnlockStorage* quick_unlock_storage =
      quick_unlock::QuickUnlockFactory::GetForProfile(
          ProfileManager::GetActiveUserProfile());
  if (!quick_unlock_storage->GetUserContext(*quick_unlock_token_)) {
    // Context already expired
    quick_unlock_token_ = absl::nullopt;
    return;
  }
  context()->extra_factors_auth_session = std::make_unique<UserContext>(
      *quick_unlock_storage->GetUserContext(*quick_unlock_token_));
  quick_unlock_storage->ReplaceUserContext(*quick_unlock_token_,
                                           std::make_unique<UserContext>());
  quick_unlock_token_ = absl::nullopt;
}

AuthProofToken BaseOSAuthSetupScreen::GetToken() {
  if (!ash::features::ShouldUseAuthSessionStorage()) {
    EnsureQuickUnlockToken();
    return *quick_unlock_token_;
  }
  CHECK(context()->extra_factors_token.has_value());
  return *(context()->extra_factors_token);
}

void BaseOSAuthSetupScreen::KeepAliveAuthSession() {
  if (!ash::features::ShouldUseAuthSessionStorage()) {
    return;
  }
  session_refresher_ = AuthSessionStorage::Get()->KeepAlive(GetToken());
}

void BaseOSAuthSetupScreen::InspectContextAndContinue(
    InspectContextCallback inspect_callback,
    base::OnceClosure continuation) {
  if (!ash::features::ShouldUseAuthSessionStorage()) {
    EnsureQuickUnlockToken();
    quick_unlock::QuickUnlockStorage* quick_unlock_storage =
        quick_unlock::QuickUnlockFactory::GetForProfile(
            ProfileManager::GetActiveUserProfile());
    UserContext* context =
        quick_unlock_storage->GetUserContext(*quick_unlock_token_);
    std::move(inspect_callback).Run(context);
    if (context) {
      std::move(continuation).Run();
    }
    return;
  }
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
