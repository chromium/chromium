// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/signin/token_handle_store_factory.h"

#include <memory>

#include "ash/constants/ash_features.h"
#include "base/containers/flat_map.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/weak_ptr.h"
#include "base/no_destructor.h"
#include "chrome/browser/ash/login/signin/token_handle_store_impl.h"
#include "chrome/browser/ash/login/signin/token_handle_util.h"
#include "chrome/browser/browser_process.h"
#include "chromeos/ash/components/login/auth/public/cryptohome_key_constants.h"
#include "components/user_manager/known_user.h"
#include "components/user_manager/user_manager.h"

namespace ash {

TokenHandleStoreFactory::DoesUserHaveGaiaPassword::DoesUserHaveGaiaPassword(
    std::unique_ptr<AuthFactorEditor> factor_editor)
    : factor_editor_(std::move(factor_editor)) {}

TokenHandleStoreFactory::DoesUserHaveGaiaPassword::~DoesUserHaveGaiaPassword() =
    default;

void TokenHandleStoreFactory::DoesUserHaveGaiaPassword::Run(
    const AccountId& account_id,
    base::OnceCallback<void(std::optional<bool>)> callback) {
  CHECK(account_id.is_valid());
  auto [it, inserted] = callbacks_.try_emplace(account_id);
  it->second.push_back(std::move(callback));

  if (!inserted) {
    VLOG(1) << "Request in flight, queueing callback.";
    return;
  }

  const user_manager::User* user =
      user_manager::UserManager::Get()->FindUser(account_id);
  if (!user) {
    LOG(WARNING)
        << "User not found, replying with unknown Gaia password status.";
    auto callbacks = std::move(it->second);
    callbacks_.erase(it);
    for (auto& cb : callbacks) {
      std::move(cb).Run(std::nullopt);
    }
    return;
  }

  factor_editor_->GetAuthFactorsConfiguration(
      std::make_unique<UserContext>(*user),
      base::BindOnce(&TokenHandleStoreFactory::DoesUserHaveGaiaPassword::
                         OnGetAuthFactorConfiguration,
                     weak_factory_.GetWeakPtr()));
}

void TokenHandleStoreFactory::DoesUserHaveGaiaPassword::
    OnGetAuthFactorConfiguration(std::unique_ptr<UserContext> user_context,
                                 std::optional<AuthenticationError> error) {
  CHECK(user_context);
  const AccountId account_id = user_context->GetAccountId();

  std::optional<bool> result = std::nullopt;
  if (!error.has_value()) {
    CHECK(user_context->HasAuthFactorsConfiguration());
    if (const cryptohome::AuthFactor* factor =
            user_context->GetAuthFactorsConfiguration().FindFactorByType(
                cryptohome::AuthFactorType::kPassword);
        factor &&
        factor->ref().label().value() == ash::kCryptohomeGaiaKeyLabel) {
      // User is using their gaia password for authentication.
      result = true;
    } else {
      // User is not using their gaia password for authentication.
      result = false;
    }
  }

  auto it = callbacks_.find(account_id);
  CHECK(it != callbacks_.end());
  auto callbacks = std::move(it->second);
  callbacks_.erase(it);

  for (auto& cb : callbacks) {
    std::move(cb).Run(result);
  }
}

TokenHandleStoreFactory::DoesUserHaveGaiaPassword::
    DoesUserHaveGaiaPasswordCallback
    TokenHandleStoreFactory::DoesUserHaveGaiaPassword::
        CreateRepeatingCallback() {
  return base::BindRepeating(
      &TokenHandleStoreFactory::DoesUserHaveGaiaPassword::Run,
      weak_factory_.GetWeakPtr());
}

TokenHandleStoreFactory::TokenHandleStoreFactory() = default;

TokenHandleStoreFactory::~TokenHandleStoreFactory() = default;

// static
TokenHandleStoreFactory* TokenHandleStoreFactory::Get() {
  static base::NoDestructor<TokenHandleStoreFactory> instance;
  return instance.get();
}

std::unique_ptr<TokenHandleStore>
TokenHandleStoreFactory::CreateTokenHandleStoreImpl() {
  if (!does_user_have_gaia_password_) {
    does_user_have_gaia_password_ = std::make_unique<DoesUserHaveGaiaPassword>(
        std::make_unique<AuthFactorEditor>(UserDataAuthClient::Get()));
  }
  return std::make_unique<TokenHandleStoreImpl>(
      std::make_unique<user_manager::KnownUser>(
          g_browser_process->local_state()),
      does_user_have_gaia_password_->CreateRepeatingCallback());
}

TokenHandleStore* TokenHandleStoreFactory::GetTokenHandleStore() {
  if (token_handle_store_ == nullptr) {
    if (features::IsUseTokenHandleStoreEnabled()) {
      token_handle_store_ = CreateTokenHandleStoreImpl();
    } else {
      token_handle_store_ =
          std::make_unique<TokenHandleUtil>(g_browser_process->local_state());
    }
  }

  return token_handle_store_.get();
}

void TokenHandleStoreFactory::DestroyTokenHandleStore() {
  token_handle_store_.reset();
  does_user_have_gaia_password_.reset();
}

}  // namespace ash
