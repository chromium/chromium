// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/login/mock_login_screen_client.h"

#include <memory>
#include <utility>

#include "ash/login/login_screen_controller.h"
#include "ash/shell.h"

namespace ash {

MockLoginScreenClient::MockLoginScreenClient() {
  Shell::Get()->login_screen_controller()->SetClient(this);
}

MockLoginScreenClient::~MockLoginScreenClient() = default;

void MockLoginScreenClient::AuthenticateUserWithPasswordOrPin(
    const AccountId& account_id,
    const std::string& password,
    bool authenticated_by_pin,
    base::OnceCallback<void(bool)> callback) {
  AuthenticateUserWithPasswordOrPin_(account_id, password, authenticated_by_pin,
                                     callback);
  if (authenticate_user_with_password_or_pin_callback_storage_) {
    *authenticate_user_with_password_or_pin_callback_storage_ =
        std::move(callback);
  } else {
    std::move(callback).Run(authenticate_user_callback_result_);
  }
}

void MockLoginScreenClient::AuthenticateUserWithChallengeResponse(
    const AccountId& account_id,
    base::OnceCallback<void(bool)> callback) {
  AuthenticateUserWithChallengeResponse_(account_id, callback);
}

ParentCodeValidationResult MockLoginScreenClient::ValidateParentAccessCode(
    const AccountId& account_id,
    const std::string& code,
    base::Time validation_time) {
  ValidateParentAccessCode_(account_id, code, validation_time);
  return validate_parent_access_code_result_;
}

}  // namespace ash
