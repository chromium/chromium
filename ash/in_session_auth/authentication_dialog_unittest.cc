// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/in_session_auth/authentication_dialog.h"

#include <memory>
#include <optional>
#include <utility>

#include "ash/public/cpp/in_session_auth_token_provider.h"
#include "ash/public/cpp/test/mock_in_session_auth_token_provider.h"
#include "ash/test/ash_test_base.h"
#include "base/memory/raw_ptr.h"
#include "base/test/bind.h"
#include "base/time/time.h"
#include "chromeos/ash/components/cryptohome/auth_factor.h"
#include "chromeos/ash/components/cryptohome/common_types.h"
#include "chromeos/ash/components/cryptohome/cryptohome_parameters.h"
#include "chromeos/ash/components/cryptohome/error_types.h"
#include "chromeos/ash/components/dbus/cryptohome/UserDataAuth.pb.h"
#include "chromeos/ash/components/dbus/userdataauth/userdataauth_client.h"
#include "chromeos/ash/components/login/auth/auth_performer.h"
#include "chromeos/ash/components/login/auth/mock_auth_performer.h"
#include "chromeos/ash/components/login/auth/public/auth_callbacks.h"
#include "chromeos/ash/components/login/auth/public/auth_session_intent.h"
#include "chromeos/ash/components/login/auth/public/authentication_error.h"
#include "chromeos/ash/components/login/auth/public/cryptohome_key_constants.h"
#include "chromeos/ash/components/login/auth/public/session_auth_factors.h"
#include "chromeos/ash/components/osauth/public/common_types.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/strings/ascii.h"
#include "ui/events/event_constants.h"
#include "ui/events/keycodes/keyboard_codes_posix.h"
#include "ui/views/controls/textfield/textfield.h"

namespace ash {

namespace {

using ::cryptohome::KeyLabel;
using ::testing::_;

const char kTestAccount[] = "user@test.com";
const char kExpectedPassword[] = "qwerty";
AuthProofToken kToken = "auth-proof-token";

}  // namespace

class AuthenticationDialogTest : public AshTestBase {
 public:
  AuthenticationDialogTest() = default;

  void SetUp() override {
    AshTestBase::SetUp();
    UserDataAuthClient::InitializeFake();
    auth_token_provider_ = std::make_unique<MockInSessionAuthTokenProvider>();
  }

  void StartAuthSession(std::unique_ptr<UserContext> user_context,
                        bool /*ephemeral*/,
                        AuthSessionIntent /*intent*/,
                        AuthPerformer::StartSessionCallback callback) {
    cryptohome::AuthFactorRef ref{cryptohome::AuthFactorType::kPassword,
                                  KeyLabel(kCryptohomeGaiaKeyLabel)};
    cryptohome::AuthFactorCommonMetadata metadata{};
    cryptohome::AuthFactor factor{std::move(ref), std::move(metadata)};
    user_context->SetSessionAuthFactors(
        SessionAuthFactors{{std::move(factor)}});
    std::move(callback).Run(true, std::move(user_context), std::nullopt);
  }

  void GetAuthToken(std::unique_ptr<UserContext> user_context,
                    InSessionAuthTokenProvider::OnAuthTokenGenerated callback) {
    std::move(callback).Run(kToken, base::Minutes(5));
  }

 protected:
  void CreateAndShowDialog() {
    auto auth_performer =
        std::make_unique<MockAuthPerformer>(UserDataAuthClient::Get());
    auth_performer_ = auth_performer.get();

    EXPECT_CALL(*auth_performer_, StartAuthSession)
        .WillRepeatedly(
            testing::Invoke(this, &AuthenticationDialogTest::StartAuthSession));

    // `dialog_` is a `DialogDelegateView` and will be owned by the
    // underlying widget.
    dialog_ = new AuthenticationDialog(
        base::BindLambdaForTesting([&](bool success,
                                       const AuthProofToken& token,
                                       base::TimeDelta timeout) {
          success_ = success;
          token_ = token;
        }),
        auth_token_provider_.get(), std::move(auth_performer),
        AccountId::FromUserEmail(kTestAccount));

    test_api_ = std::make_unique<AuthenticationDialog::TestApi>(dialog_);

    dialog_->Show();
  }

  void TypePassword(const std::string& password) {
    auto* generator = GetEventGenerator();
    generator->MoveMouseTo(
        test_api_->GetPasswordTextfield()->GetBoundsInScreen().CenterPoint());
    generator->ClickLeftButton();

    for (char c : password) {
      EXPECT_TRUE(absl::ascii_isalpha(static_cast<unsigned char>(c)));
      generator->PressAndReleaseKey(
          static_cast<ui::KeyboardCode>(ui::KeyboardCode::VKEY_A + (c - 'a')),
          ui::EF_NONE);
    }
  }

  void PressOkButton() {
    auto* generator = GetEventGenerator();
    generator->MoveMouseTo(
        dialog_->GetOkButton()->GetBoundsInScreen().CenterPoint());
    generator->ClickLeftButton();
  }

  std::optional<bool> success_;
  AuthProofToken token_;
  raw_ptr<AuthenticationDialog, AcrossTasksDanglingUntriaged> dialog_;
  std::unique_ptr<MockInSessionAuthTokenProvider> auth_token_provider_;
  raw_ptr<MockAuthPerformer, AcrossTasksDanglingUntriaged> auth_performer_;
  std::unique_ptr<AuthenticationDialog::TestApi> test_api_;
};

TEST_F(AuthenticationDialogTest, CallbackCalledOnCancel) {
  CreateAndShowDialog();
  dialog_->Cancel();
  EXPECT_TRUE(success_.has_value());
  EXPECT_EQ(success_.value(), false);
}

TEST_F(AuthenticationDialogTest, CallbackCalledOnClose) {
  CreateAndShowDialog();
  dialog_->Close();
  EXPECT_TRUE(success_.has_value());
  EXPECT_EQ(success_.value(), false);
}

TEST_F(AuthenticationDialogTest, CorrectPasswordProvided) {
  CreateAndShowDialog();
  TypePassword(kExpectedPassword);

  EXPECT_CALL(*auth_performer_,
              AuthenticateWithPassword(kCryptohomeGaiaKeyLabel,
                                       kExpectedPassword, _, _))
      .WillOnce([](const std::string& key_label, const std::string& password,
                   std::unique_ptr<UserContext> user_context,
                   AuthOperationCallback callback) {
        std::move(callback).Run(std::move(user_context), std::nullopt);
      });

  EXPECT_CALL(*auth_token_provider_, ExchangeForToken)
      .WillOnce(testing::Invoke(this, &AuthenticationDialogTest::GetAuthToken));

  PressOkButton();

  EXPECT_TRUE(success_.has_value());
  EXPECT_TRUE(success_.value());
  EXPECT_EQ(token_, kToken);
}

TEST_F(AuthenticationDialogTest, IncorrectPasswordProvidedThenCorrect) {
  CreateAndShowDialog();
  TypePassword("ytrewq");

  EXPECT_CALL(*auth_performer_,
              AuthenticateWithPassword(kCryptohomeGaiaKeyLabel, _, _, _))
      .WillRepeatedly([](const std::string& key_label,
                         const std::string& password,
                         std::unique_ptr<UserContext> user_context,
                         AuthOperationCallback callback) {
        std::move(callback).Run(
            std::move(user_context),
            password == kExpectedPassword
                ? std::nullopt
                : std::optional<AuthenticationError>{AuthenticationError{
                      cryptohome::ErrorWrapper::CreateFromErrorCodeOnly(
                          user_data_auth::
                              CRYPTOHOME_ERROR_AUTHORIZATION_KEY_NOT_FOUND)}});
      });

  PressOkButton();
  TypePassword(kExpectedPassword);

  EXPECT_CALL(*auth_token_provider_, ExchangeForToken)
      .WillOnce(testing::Invoke(this, &AuthenticationDialogTest::GetAuthToken));

  PressOkButton();

  EXPECT_TRUE(success_.has_value());
  EXPECT_TRUE(success_.value());
  EXPECT_EQ(token_, kToken);
}

TEST_F(AuthenticationDialogTest, AuthSessionRestartedWhenExpired) {
  CreateAndShowDialog();
  TypePassword(kExpectedPassword);

  int number_of_calls = 0;
  EXPECT_CALL(*auth_performer_,
              AuthenticateWithPassword(kCryptohomeGaiaKeyLabel,
                                       kExpectedPassword, _, _))
      .WillRepeatedly(
          [&number_of_calls](const std::string& key_label,
                             const std::string& password,
                             std::unique_ptr<UserContext> user_context,
                             AuthOperationCallback callback) {
            std::move(callback).Run(
                std::move(user_context),
                number_of_calls++
                    ? std::nullopt
                    : std::optional<AuthenticationError>{AuthenticationError{
                          cryptohome::ErrorWrapper::CreateFromErrorCodeOnly(
                              user_data_auth::
                                  CRYPTOHOME_INVALID_AUTH_SESSION_TOKEN)}});
          });

  EXPECT_CALL(*auth_token_provider_, ExchangeForToken)
      .WillOnce(testing::Invoke(this, &AuthenticationDialogTest::GetAuthToken));

  PressOkButton();

  EXPECT_TRUE(success_.has_value());
  EXPECT_TRUE(success_.value());
  EXPECT_EQ(token_, kToken);
}

}  // namespace ash
