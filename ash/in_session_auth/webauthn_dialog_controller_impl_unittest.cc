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
#include "base/functional/callback_helpers.h"
#include "base/test/bind.h"
#include "base/test/test_future.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/window.h"

using ::testing::_;
using ::testing::Return;

namespace ash {
namespace {

class WebAuthNDialogControllerImplTest : public AshTestBase {
 protected:
  // Requests authentication the way the WebAuthn service does, which is what
  // tells the controller which user it is for.
  void ShowDialog(WebAuthNDialogController::FinishCallback finish_callback) {
    Shell::Get()->webauthn_dialog_controller()->ShowAuthenticationDialog(
        source_window_.get(), "example.com", std::move(finish_callback));
  }

  // Gets the controller as far as knowing its user for the tests that drive
  // it directly from there. The mock drops StartAuthSession()'s callback,
  // which leaves the flow here rather than going on to build the dialog.
  void StartRequestFor(MockInSessionAuthDialogClient& client) {
    EXPECT_CALL(client, StartAuthSession);
    ShowDialog(base::DoNothing());
  }

  // AshTestBase:
  void SetUp() override {
    AshTestBase::SetUp();
    source_window_ = CreateToplevelTestWindow();
  }

  void TearDown() override {
    // The window has to go before the Shell that owns its hierarchy.
    source_window_.reset();
    AshTestBase::TearDown();
  }

 private:
  std::unique_ptr<aura::Window> source_window_;
};

TEST_F(WebAuthNDialogControllerImplTest, PinAuthSuccess) {
  WebAuthNDialogController* controller =
      Shell::Get()->webauthn_dialog_controller();
  auto client = std::make_unique<MockInSessionAuthDialogClient>();
  StartRequestFor(*client);

  std::string pin = "123456";

  EXPECT_CALL(*client, AuthenticateUserWithPasswordOrPin(
                           pin, /* authenticated_by_pin = */ true, _))
      .WillOnce([](const std::string& pin, bool authenticated_by_pin,
                   base::OnceCallback<void(bool success)> controller_callback) {
        std::move(controller_callback).Run(true);
      });
  EXPECT_CALL(*client, InvalidateAuthSession);

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
  StartRequestFor(*client);

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
  StartRequestFor(*client);

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
  StartRequestFor(*client);

  std::string password = "abcdef";

  EXPECT_CALL(*client, AuthenticateUserWithPasswordOrPin(
                           password, /* authenticated_by_pin = */ false, _))
      .WillOnce([](const std::string& password, bool authenticated_by_pin,
                   base::OnceCallback<void(bool success)> controller_callback) {
        std::move(controller_callback).Run(true);
      });
  EXPECT_CALL(*client, InvalidateAuthSession);

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
  StartRequestFor(*client);

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

TEST_F(WebAuthNDialogControllerImplTest, ForgetsTheUserWhenTheSessionFails) {
  auto client = std::make_unique<MockInSessionAuthDialogClient>();
  EXPECT_CALL(*client, StartAuthSession)
      .WillOnce([](const AccountId& account_id,
                   base::OnceCallback<void(bool success)> controller_callback) {
        std::move(controller_callback).Run(false);
      });
  EXPECT_CALL(*client, InvalidateAuthSession);

  base::test::TestFuture<bool> result;
  ShowDialog(result.GetCallback());
  EXPECT_FALSE(result.Get());

  // Having given that request up, the controller takes the next one.
  StartRequestFor(*client);
}

TEST_F(WebAuthNDialogControllerImplTest, ForgetsTheUserWhenCancelledEarly) {
  WebAuthNDialogController* controller =
      Shell::Get()->webauthn_dialog_controller();
  auto client = std::make_unique<MockInSessionAuthDialogClient>();
  EXPECT_CALL(*client, StartAuthSession);
  EXPECT_CALL(*client, InvalidateAuthSession);

  base::test::TestFuture<bool> result;
  ShowDialog(result.GetCallback());

  // No dialog has been built yet, so this cancels before there is one.
  controller->Cancel();
  EXPECT_FALSE(result.Get());

  StartRequestFor(*client);
}

TEST_F(WebAuthNDialogControllerImplTest, DropsRepliesToACancelledRequest) {
  WebAuthNDialogController* controller =
      Shell::Get()->webauthn_dialog_controller();
  auto client = std::make_unique<MockInSessionAuthDialogClient>();
  EXPECT_CALL(*client, StartAuthSession)
      .WillOnce([](const AccountId& account_id,
                   base::OnceCallback<void(bool success)> controller_callback) {
        std::move(controller_callback).Run(true);
      });
  EXPECT_CALL(*client, IsFingerprintAuthAvailable).WillOnce(Return(true));
  base::OnceCallback<void(bool)> fingerprint_session;
  EXPECT_CALL(*client, StartFingerprintAuthSession)
      .WillOnce(
          [&fingerprint_session](
              const AccountId& account_id,
              base::OnceCallback<void(bool success)> controller_callback) {
            fingerprint_session = std::move(controller_callback);
          });
  EXPECT_CALL(*client, InvalidateAuthSession);

  base::test::TestFuture<bool> result;
  ShowDialog(result.GetCallback());
  ASSERT_TRUE(fingerprint_session);

  controller->Cancel();
  EXPECT_FALSE(result.Get());

  // The client answers a request the controller has already given up on.
  std::move(fingerprint_session).Run(true);
}

}  // namespace
}  // namespace ash
