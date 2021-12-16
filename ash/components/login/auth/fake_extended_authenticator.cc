// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/login/auth/fake_extended_authenticator.h"

#include "ash/components/login/auth/auth_status_consumer.h"
#include "base/notreached.h"
#include "components/account_id/account_id.h"

namespace ash {

FakeExtendedAuthenticator::FakeExtendedAuthenticator(
    AuthStatusConsumer* consumer,
    const UserContext& expected_user_context)
    : consumer_(consumer), expected_user_context_(expected_user_context) {}

FakeExtendedAuthenticator::~FakeExtendedAuthenticator() = default;

void FakeExtendedAuthenticator::SetConsumer(AuthStatusConsumer* consumer) {
  consumer_ = consumer;
}

void FakeExtendedAuthenticator::AuthenticateToCheck(
    const UserContext& context,
    base::OnceClosure success_callback) {
  if (expected_user_context_ == context) {
    if (success_callback)
      std::move(success_callback).Run();
    OnAuthSuccess(context);
    return;
  }

  OnAuthFailure(FAILED_MOUNT,
                AuthFailure(AuthFailure::UNLOCK_FAILED));
}

void FakeExtendedAuthenticator::StartFingerprintAuthSession(
    const AccountId& account_id,
    base::OnceCallback<void(bool)> callback) {
  std::move(callback).Run(expected_user_context_.GetAccountId() == account_id);
}

void FakeExtendedAuthenticator::EndFingerprintAuthSession() {}

void FakeExtendedAuthenticator::AuthenticateWithFingerprint(
    const UserContext& context,
    base::OnceCallback<void(::user_data_auth::CryptohomeErrorCode)> callback) {
  if (expected_user_context_ == context) {
    std::move(callback).Run(::user_data_auth::CryptohomeErrorCode::
                                CRYPTOHOME_ERROR_FINGERPRINT_RETRY_REQUIRED);
    return;
  }

  std::move(callback).Run(
      ::user_data_auth::CryptohomeErrorCode::CRYPTOHOME_ERROR_NOT_SET);
}

void FakeExtendedAuthenticator::AddKey(const UserContext& context,
                                       const cryptohome::KeyDefinition& key,
                                       bool replace_existing,
                                       base::OnceClosure success_callback) {
  NOTREACHED();
}

void FakeExtendedAuthenticator::RemoveKey(const UserContext& context,
                                          const std::string& key_to_remove,
                                          base::OnceClosure success_callback) {
  NOTREACHED();
}

void FakeExtendedAuthenticator::TransformKeyIfNeeded(
    const UserContext& user_context,
    ContextCallback callback) {
  if (callback)
    std::move(callback).Run(user_context);
}

void FakeExtendedAuthenticator::OnAuthSuccess(const UserContext& context) {
  if (consumer_)
    consumer_->OnAuthSuccess(context);
}

void FakeExtendedAuthenticator::OnAuthFailure(AuthState state,
                                              const AuthFailure& error) {
  if (consumer_)
    consumer_->OnAuthFailure(error);
}

}  // namespace ash
