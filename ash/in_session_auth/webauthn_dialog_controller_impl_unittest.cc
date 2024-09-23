// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/in_session_auth/webauthn_dialog_controller_impl.h"

#include <memory>
#include <string>
#include <utility>

#include "ash/in_session_auth/mock_in_session_auth_dialog_client.h"
#include "ash/public/cpp/webauthn_dialog_controller.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "base/functional/callback.h"
#include "base/test/bind.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;

namespace ash {
namespace {

using WebAuthNDialogControllerImplTest = AshTestBase;

TEST_F(WebAuthNDialogControllerImplTest, PinAuthSuccess) {
  WebAuthNDialogController* controller =
      Shell::Get()->webauthn_dialog_controller();
  auto client = std::make_unique<MockInSessionAuthDialogClient>();

  std::string pin = "123456";

  EXPECT_CALL(*client, AuthenticateUserWithPasswordOrPin(
                           pin, /* authenticated_by_pin = */ true, _))
      .WillOnce([](const std::string& pin, bool authenticated_by_pin,
                   base::OnceCallback<void(bool success)> controller_callback) {
        std::move(controller_callback).Run(true);
      });

  bool result_success = false;
  controller->AuthenticateUserWithPasswordOrPin(
      pin,
      /*authenticated_by_password=*/true,
      /* View callback will be executed during controller callback. */
      base::BindLambdaForTesting(
          [&result_success](bool success, bool can_use_pin) {
            result_success = success;
          }));

  EXPECT_TRUE(result_success);
}

TEST_F(WebAuthNDialogControllerImplTest, PinAuthFail) {
  WebAuthNDialogController* controller =
      Shell::Get()->webauthn_dialog_controller();
  auto client = std::make_unique<MockInSessionAuthDialogClient>();

  std::string pin = "123456";

  EXPECT_CALL(*client, AuthenticateUserWithPasswordOrPin(
                           pin, /* authenticated_by_pin = */ true, _))
      .WillOnce([](const std::string& pin, bool authenticated_by_pin,
                   base::OnceCallback<void(bool success)> controller_callback) {
        std::move(controller_callback).Run(false);
      });
  EXPECT_CALL(*client, CheckPinAuthAvailability(_, _))
      .WillOnce([](const AccountId& account_id,
                   base::OnceCallback<void(bool success)> controller_callback) {
        std::move(controller_callback).Run(true);
      });

  bool result_success = false;
  bool result_can_use_pin = false;
  controller->AuthenticateUserWithPasswordOrPin(
      pin,
      /*authenticated_by_password=*/true,
      /* View callback will be executed during controller callback. */
      base::BindLambdaForTesting([&result_success, &result_can_use_pin](
                                     bool success, bool can_use_pin) {
        result_success = success;
        result_can_use_pin = can_use_pin;
      }));

  EXPECT_FALSE(result_success);
  EXPECT_TRUE(result_can_use_pin);
}

TEST_F(WebAuthNDialogControllerImplTest, PinAuthFailLockout) {
  WebAuthNDialogController* controller =
      Shell::Get()->webauthn_dialog_controller();
  auto client = std::make_unique<MockInSessionAuthDialogClient>();

  std::string pin = "123456";

  EXPECT_CALL(*client, AuthenticateUserWithPasswordOrPin(
                           pin, /* authenticated_by_pin = */ true, _))
      .WillOnce([](const std::string& pin, bool authenticated_by_pin,
                   base::OnceCallback<void(bool success)> controller_callback) {
        std::move(controller_callback).Run(false);
      });
  EXPECT_CALL(*client, CheckPinAuthAvailability(_, _))
      .WillOnce([](const AccountId& account_id,
                   base::OnceCallback<void(bool success)> controller_callback) {
        std::move(controller_callback).Run(false);
      });

  bool result_success = false;
  bool result_can_use_pin = false;
  controller->AuthenticateUserWithPasswordOrPin(
      pin,
      /*authenticated_by_password=*/true,
      /* View callback will be executed during controller callback. */
      base::BindLambdaForTesting([&result_success, &result_can_use_pin](
                                     bool success, bool can_use_pin) {
        result_success = success;
        result_can_use_pin = can_use_pin;
      }));

  EXPECT_FALSE(result_success);
  EXPECT_FALSE(result_can_use_pin);
}

TEST_F(WebAuthNDialogControllerImplTest, PasswordAuthSuccess) {
  WebAuthNDialogController* controller =
      Shell::Get()->webauthn_dialog_controller();
  auto client = std::make_unique<MockInSessionAuthDialogClient>();

  std::string password = "abcdef";

  EXPECT_CALL(*client, AuthenticateUserWithPasswordOrPin(
                           password, /* authenticated_by_pin = */ false, _))
      .WillOnce([](const std::string& password, bool authenticated_by_pin,
                   base::OnceCallback<void(bool success)> controller_callback) {
        std::move(controller_callback).Run(true);
      });

  bool result_success = false;
  controller->AuthenticateUserWithPasswordOrPin(
      password,
      /*authenticated_by_password=*/false,
      /* View callback will be executed during controller callback. */
      base::BindLambdaForTesting(
          [&result_success](bool success, bool can_use_pin) {
            result_success = success;
          }));

  EXPECT_TRUE(result_success);
}

TEST_F(WebAuthNDialogControllerImplTest, PasswordAuthFail) {
  WebAuthNDialogController* controller =
      Shell::Get()->webauthn_dialog_controller();
  auto client = std::make_unique<MockInSessionAuthDialogClient>();

  std::string password = "abcdef";

  EXPECT_CALL(*client, AuthenticateUserWithPasswordOrPin(
                           password, /* authenticated_by_pin = */ false, _))
      .WillOnce([](const std::string& password, bool authenticated_by_pin,
                   base::OnceCallback<void(bool success)> controller_callback) {
        std::move(controller_callback).Run(false);
      });
  EXPECT_CALL(*client, CheckPinAuthAvailability(_, _))
      .WillOnce([](const AccountId& account_id,
                   base::OnceCallback<void(bool success)> controller_callback) {
        std::move(controller_callback).Run(false);
      });

  bool result_success = false;
  controller->AuthenticateUserWithPasswordOrPin(
      password,
      /*authenticated_by_password=*/false,
      /* View callback will be executed during controller callback. */
      base::BindLambdaForTesting(
          [&result_success](bool success, bool can_use_pin) {
            result_success = success;
          }));

  EXPECT_FALSE(result_success);
}

}  // namespace
}  // namespace ash
