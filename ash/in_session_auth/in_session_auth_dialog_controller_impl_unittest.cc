// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/in_session_auth/in_session_auth_dialog_controller_impl.h"

#include "ash/in_session_auth/mock_in_session_auth_dialog_client.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "base/callback.h"
#include "base/test/bind.h"

using ::testing::_;

namespace ash {
namespace {

using InSessionAuthDialogControllerImplTest = AshTestBase;

TEST_F(InSessionAuthDialogControllerImplTest, PinAuthSuccess) {
  InSessionAuthDialogController* controller =
      Shell::Get()->in_session_auth_dialog_controller();
  auto client = std::make_unique<MockInSessionAuthDialogClient>();

  std::string pin = "123456";

  EXPECT_CALL(*client, AuthenticateUserWithPasswordOrPin(
                           pin, /* authenticated_by_pin = */ true, _))
      .WillOnce([](const std::string& pin, bool authenticated_by_pin,
                   base::OnceCallback<void(bool success)> controller_callback) {
        std::move(controller_callback).Run(true);
      });

  absl::optional<bool> view_callback_result;
  controller->AuthenticateUserWithPasswordOrPin(
      pin,
      /*authenticated_by_password=*/true,
      /* View callback will be executed during controller callback. */
      base::BindLambdaForTesting(
          [&view_callback_result](absl::optional<bool> did_auth) {
            view_callback_result = did_auth;
          }));

  EXPECT_TRUE(view_callback_result.has_value());
  EXPECT_TRUE(*view_callback_result);
}

TEST_F(InSessionAuthDialogControllerImplTest, PinAuthFail) {
  InSessionAuthDialogController* controller =
      Shell::Get()->in_session_auth_dialog_controller();
  auto client = std::make_unique<MockInSessionAuthDialogClient>();

  std::string pin = "123456";

  EXPECT_CALL(*client, AuthenticateUserWithPasswordOrPin(
                           pin, /* authenticated_by_pin = */ true, _))
      .WillOnce([](const std::string& pin, bool authenticated_by_pin,
                   base::OnceCallback<void(bool success)> controller_callback) {
        std::move(controller_callback).Run(false);
      });

  absl::optional<bool> view_callback_result;
  controller->AuthenticateUserWithPasswordOrPin(
      pin,
      /*authenticated_by_password=*/true,
      /* View callback will be executed during controller callback. */
      base::BindLambdaForTesting(
          [&view_callback_result](absl::optional<bool> did_auth) {
            view_callback_result = did_auth;
          }));

  EXPECT_TRUE(view_callback_result.has_value());
  EXPECT_FALSE(*view_callback_result);
}

TEST_F(InSessionAuthDialogControllerImplTest, PasswordAuthSuccess) {
  InSessionAuthDialogController* controller =
      Shell::Get()->in_session_auth_dialog_controller();
  auto client = std::make_unique<MockInSessionAuthDialogClient>();

  std::string password = "abcdef";

  EXPECT_CALL(*client, AuthenticateUserWithPasswordOrPin(
                           password, /* authenticated_by_pin = */ false, _))
      .WillOnce([](const std::string& password, bool authenticated_by_pin,
                   base::OnceCallback<void(bool success)> controller_callback) {
        std::move(controller_callback).Run(true);
      });

  absl::optional<bool> view_callback_result;
  controller->AuthenticateUserWithPasswordOrPin(
      password,
      /*authenticated_by_password=*/false,
      /* View callback will be executed during controller callback. */
      base::BindLambdaForTesting(
          [&view_callback_result](absl::optional<bool> did_auth) {
            view_callback_result = did_auth;
          }));

  EXPECT_TRUE(view_callback_result.has_value());
  EXPECT_TRUE(*view_callback_result);
}

TEST_F(InSessionAuthDialogControllerImplTest, PasswordAuthFail) {
  InSessionAuthDialogController* controller =
      Shell::Get()->in_session_auth_dialog_controller();
  auto client = std::make_unique<MockInSessionAuthDialogClient>();

  std::string password = "abcdef";

  EXPECT_CALL(*client, AuthenticateUserWithPasswordOrPin(
                           password, /* authenticated_by_pin = */ false, _))
      .WillOnce([](const std::string& password, bool authenticated_by_pin,
                   base::OnceCallback<void(bool success)> controller_callback) {
        std::move(controller_callback).Run(false);
      });

  absl::optional<bool> view_callback_result;
  controller->AuthenticateUserWithPasswordOrPin(
      password,
      /*authenticated_by_password=*/false,
      /* View callback will be executed during controller callback. */
      base::BindLambdaForTesting(
          [&view_callback_result](absl::optional<bool> did_auth) {
            view_callback_result = did_auth;
          }));

  EXPECT_TRUE(view_callback_result.has_value());
  EXPECT_FALSE(*view_callback_result);
}

}  // namespace
}  // namespace ash
