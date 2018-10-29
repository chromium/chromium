// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/login/mock_login_screen_client.h"

#include <memory>
#include <utility>

#include "ash/login/login_screen_controller.h"
#include "ash/shell.h"

namespace ash {

MockLoginScreenClient::MockLoginScreenClient() : binding_(this) {}

MockLoginScreenClient::~MockLoginScreenClient() = default;

mojom::LoginScreenClientPtr MockLoginScreenClient::CreateInterfacePtrAndBind() {
  mojom::LoginScreenClientPtr ptr;
  binding_.Bind(mojo::MakeRequest(&ptr));
  return ptr;
}

void MockLoginScreenClient::AuthenticateUserWithPasswordOrPin(
    const AccountId& account_id,
    const std::string& password,
    bool authenticated_by_pin,
    AuthenticateUserWithPasswordOrPinCallback callback) {
  AuthenticateUserWithPasswordOrPin_(account_id, password, authenticated_by_pin,
                                     callback);
  if (authenticate_user_with_password_or_pin_callback_storage_) {
    *authenticate_user_with_password_or_pin_callback_storage_ =
        std::move(callback);
  } else {
    std::move(callback).Run(authenticate_user_callback_result_);
  }
}

void MockLoginScreenClient::AuthenticateUserWithExternalBinary(
    const AccountId& account_id,
    AuthenticateUserWithExternalBinaryCallback callback) {
  AuthenticateUserWithExternalBinary_(account_id, callback);
  if (authenticate_user_with_external_binary_callback_storage_) {
    *authenticate_user_with_external_binary_callback_storage_ =
        std::move(callback);
  } else {
    std::move(callback).Run(authenticate_user_callback_result_);
  }
}

void MockLoginScreenClient::EnrollUserWithExternalBinary(
    EnrollUserWithExternalBinaryCallback callback) {
  EnrollUserWithExternalBinary_(callback);
  if (enroll_user_with_external_binary_callback_storage_) {
    *enroll_user_with_external_binary_callback_storage_ = std::move(callback);
  } else {
    std::move(callback).Run(authenticate_user_callback_result_);
  }
}

std::unique_ptr<MockLoginScreenClient> BindMockLoginScreenClient() {
  auto client = std::make_unique<MockLoginScreenClient>();
  Shell::Get()->login_screen_controller()->SetClient(
      client->CreateInterfacePtrAndBind());
  return client;
}

}  // namespace ash
