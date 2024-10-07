// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/auth/active_session_auth_controller_impl.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/test/ash_test_base.h"
#include "base/functional/overloaded.h"
#include "base/test/run_until.h"
#include "base/test/test_future.h"
#include "chromeos/ash/components/cryptohome/system_salt_getter.h"
#include "chromeos/ash/components/dbus/userdataauth/fake_cryptohome_misc_client.h"
#include "chromeos/ash/components/dbus/userdataauth/fake_userdataauth_client.h"
#include "chromeos/ash/components/login/auth/public/cryptohome_key_constants.h"
#include "chromeos/ash/components/osauth/impl/request/password_manager_auth_request.h"
#include "chromeos/ash/components/osauth/impl/request/settings_auth_request.h"
#include "chromeos/ash/components/osauth/impl/request/webauthn_auth_request.h"
#include "chromeos/ash/components/osauth/public/auth_parts.h"
#include "chromeos/ash/components/osauth/public/request/auth_request.h"
#include "components/user_manager/fake_user_manager.h"
#include "components/user_manager/known_user.h"
#include "components/user_manager/user_manager.h"
#include "third_party/cros_system_api/dbus/cryptohome/dbus-constants.h"
#include "ui/base/l10n/l10n_util.h"

namespace ash {

namespace {

constexpr char kUserEmail[] = "expected_email@example.com";
constexpr char kExpectedPassword[] = "expected_password";
constexpr char kExpectedPin[] = "123456";
constexpr char kExpectedSalt[] = "test salt";
constexpr char kRpId[] = "example.com";

}  // namespace

enum class TestVariant {
  kWebAuthN,
  kPasswordManager,
  kSettings,
};

class ActiveSessionAuthControllerTest
    : public NoSessionAshTestBase,
      public testing::WithParamInterface<TestVariant> {
 public:
  using TokenBasedCallback =
      base::test::TestFuture<bool, const ash::AuthProofToken&, base::TimeDelta>;
  using WebAuthNCallback = base::test::TestFuture<bool>;
  using OnAuthComplete = std::variant<std::unique_ptr<TokenBasedCallback>,
                                      std::unique_ptr<WebAuthNCallback>>;

  void SetUp() override {
    InitializeUserManager();
    AddUserToUserManager();
    SystemSaltGetter::Initialize();
    CryptohomeMiscClient::InitializeFake();
    UserDataAuthClient::InitializeFake();
    auth_parts_ = AuthParts::Create(&local_state_);

    AshTestBase::SetUp();

    GetSessionControllerClient()->DisableAutomaticallyProvideSigninPref();
    GetSessionControllerClient()->Reset();
    GetSessionControllerClient()->AddUserSession(
        kUserEmail, user_manager::UserType::kRegular);
    GetSessionControllerClient()->SetSessionState(
        session_manager::SessionState::ACTIVE);
  }

  void TearDown() override {
    Shell::Get()->session_controller()->ClearUserSessionsForTest();

    auth_parts_.reset();
    user_manager_->Destroy();
    user_manager_.reset();
    SystemSaltGetter::Shutdown();
    CryptohomeMiscClient::Shutdown();
    UserDataAuthClient::Shutdown();

    AshTestBase::TearDown();
  }

  void InitializeUserManager() {
    user_manager::UserManagerImpl::RegisterPrefs(local_state_.registry());
    user_manager_ =
        std::make_unique<user_manager::FakeUserManager>(&local_state_);
    user_manager_->Initialize();
  }

  void AddUserToUserManager() {
    account_id_ = AccountId::FromUserEmail(kUserEmail);
    const user_manager::User* user = user_manager_->AddUser(account_id_);
    user_manager_->UserLoggedIn(account_id_, user->username_hash(), false,
                                false);
    user_manager_->SetUserCryptohomeDataEphemeral(account_id_, false);
  }

  std::string HashPassword(const std::string& unhashed_password) {
    Key key(std::move(unhashed_password));
    key.Transform(Key::KEY_TYPE_SALTED_SHA256_TOP_HALF,
                  SystemSaltGetter::ConvertRawSaltToHexString(
                      FakeCryptohomeMiscClient::GetStubSystemSalt()));
    return key.GetSecret();
  }

  std::string HashPin(const std::string& unhashed_pin) {
    Key key(std::move(unhashed_pin));
    key.Transform(Key::KEY_TYPE_SALTED_PBKDF2_AES256_1234, kExpectedSalt);
    return key.GetSecret();
  }

  void AddGaiaPassword(const AccountId& user, const std::string& password) {
    auto account_identifier =
        cryptohome::CreateAccountIdentifierFromAccountId(user);

    FakeUserDataAuthClient::TestApi::Get()->AddExistingUser(account_identifier);

    // Hash the password, as only hashed passwords appear at the userdataauth
    // level.
    Key key(HashPassword(password));

    user_data_auth::AuthFactor auth_factor;
    user_data_auth::AuthInput auth_input;

    auth_factor.set_label(ash::kCryptohomeGaiaKeyLabel);
    auth_factor.set_type(user_data_auth::AUTH_FACTOR_TYPE_PASSWORD);

    auth_input.mutable_password_input()->set_secret(key.GetSecret());

    // Add the password key to the user.
    FakeUserDataAuthClient::TestApi::Get()->AddAuthFactor(
        account_identifier, auth_factor, auth_input);
  }

  void AddCryptohomePin(const AccountId& user, const std::string& pin) {
    auto account_identifier =
        cryptohome::CreateAccountIdentifierFromAccountId(user);

    // Hash the pin, as only hashed secrets appear at the userdataauth
    // level.
    Key key(HashPin(pin));

    // Add the pin key to the user.
    user_data_auth::AuthFactor auth_factor;
    user_data_auth::AuthInput auth_input;

    auth_factor.set_label(ash::kCryptohomePinLabel);
    auth_factor.set_type(user_data_auth::AUTH_FACTOR_TYPE_PIN);

    auth_input.mutable_password_input()->set_secret(key.GetSecret());

    FakeUserDataAuthClient::TestApi::Get()->AddAuthFactor(
        account_identifier, auth_factor, auth_input);
  }

  OnAuthComplete ShowAuthDialogForVariant(TestVariant variant) {
    auto make_password_manager_request = []() {
      auto future = std::make_unique<TokenBasedCallback>();

      Shell::Get()->active_session_auth_controller()->ShowAuthDialog(
          std::make_unique<PasswordManagerAuthRequest>(u"",
                                                       future->GetCallback()));

      return OnAuthComplete{std::move(future)};
    };

    auto make_settings_request = []() {
      auto future = std::make_unique<TokenBasedCallback>();

      Shell::Get()->active_session_auth_controller()->ShowAuthDialog(
          std::make_unique<SettingsAuthRequest>(future->GetCallback()));

      return OnAuthComplete{std::move(future)};
    };

    auto make_webauthn_request = []() {
      auto future = std::make_unique<WebAuthNCallback>();

      Shell::Get()->active_session_auth_controller()->ShowAuthDialog(
          std::make_unique<WebAuthNAuthRequest>(kRpId, future->GetCallback()));

      return OnAuthComplete{std::move(future)};
    };

    switch (variant) {
      case TestVariant::kWebAuthN:
        return make_webauthn_request();
      case TestVariant::kPasswordManager:
        return make_password_manager_request();
      case TestVariant::kSettings:
        return make_settings_request();
    }
  }

 protected:
  AccountId account_id_;
  TestingPrefServiceSimple local_state_;
  std::unique_ptr<user_manager::FakeUserManager> user_manager_;
  std::unique_ptr<AuthParts> auth_parts_;
};

// Tests that the StartAuthSession call to cryptohome includes the correct
// account id.
TEST_P(ActiveSessionAuthControllerTest,
       StartAuthSessionCalledWithCorrectAccountIdAndReturnsPasswordFactor) {
  AddGaiaPassword(account_id_, kExpectedPassword);

  auto* controller = static_cast<ActiveSessionAuthControllerImpl*>(
      Shell::Get()->active_session_auth_controller());

  ShowAuthDialogForVariant(GetParam());

  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(
      FakeUserDataAuthClient::Get()
          ->WasCalled<FakeUserDataAuthClient::Operation::kStartAuthSession>());
  auto start_auth_session_request =
      FakeUserDataAuthClient::Get()
          ->GetLastRequest<
              FakeUserDataAuthClient::Operation::kStartAuthSession>();
  EXPECT_EQ(start_auth_session_request.account_id().account_id(), kUserEmail);
  AuthFactorSet available_factors =
      ActiveSessionAuthControllerImpl::TestApi(controller)
          .GetAvailableFactors();
  EXPECT_EQ(1u, available_factors.size());
  EXPECT_TRUE(available_factors.Has(AuthInputType::kPassword));
}

// Tests that the ListAuthFactors call to cryptohome includes the correct
// account id and returns the password and pin factors.
TEST_P(ActiveSessionAuthControllerTest, StartAuthSessionReturnsPasswordAndPin) {
  AddGaiaPassword(account_id_, kExpectedPassword);
  AddCryptohomePin(account_id_, kExpectedPin);

  auto* controller = static_cast<ActiveSessionAuthControllerImpl*>(
      Shell::Get()->active_session_auth_controller());

  ShowAuthDialogForVariant(GetParam());

  // Await show.
  base::RunLoop().RunUntilIdle();

  AuthFactorSet available_factors =
      ActiveSessionAuthControllerImpl::TestApi(controller)
          .GetAvailableFactors();

  EXPECT_EQ(2u, available_factors.size());
  EXPECT_TRUE(available_factors.Has(AuthInputType::kPassword));
  EXPECT_TRUE(available_factors.Has(AuthInputType::kPin));
}

// Tests that the AuthenticateAuthFactor call to cryptohome includes the
// correct account id and password, and that the `OnAuthComplete` callback
// is called with correct parameters.
TEST_P(ActiveSessionAuthControllerTest, SubmitPassword) {
  AddGaiaPassword(account_id_, kExpectedPassword);

  auto* controller = static_cast<ActiveSessionAuthControllerImpl*>(
      Shell::Get()->active_session_auth_controller());

  auto future = ShowAuthDialogForVariant(GetParam());

  // Await show.
  base::RunLoop().RunUntilIdle();

  ActiveSessionAuthControllerImpl::TestApi(controller)
      .SubmitPassword(kExpectedPassword);

  // Await authentication.
  base::RunLoop().RunUntilIdle();

  auto authenticate_auth_factor_request =
      FakeUserDataAuthClient::Get()
          ->GetLastRequest<
              FakeUserDataAuthClient::Operation::kAuthenticateAuthFactor>();

  EXPECT_EQ(
      authenticate_auth_factor_request.auth_input().password_input().secret(),
      HashPassword(kExpectedPassword));

  std::visit(base::Overloaded([](auto&& arg) {
               EXPECT_TRUE(arg->IsReady());
               EXPECT_EQ(arg->template Get<bool>(), true);
             }),
             future);
}

// Tests that the AuthenticateAuthFactor call to cryptohome includes the
// correct account id and password, and that the `OnAuthComplete` callback
// is not called with wrong credentials.
TEST_P(ActiveSessionAuthControllerTest, WrongPassword) {
  AddGaiaPassword(account_id_, kExpectedPassword);

  auto* controller = static_cast<ActiveSessionAuthControllerImpl*>(
      Shell::Get()->active_session_auth_controller());

  auto future = ShowAuthDialogForVariant(GetParam());

  // Await show.
  base::RunLoop().RunUntilIdle();

  FakeUserDataAuthClient::Get()->SetNextOperationError(
      FakeUserDataAuthClient::Operation::kAuthenticateAuthFactor,
      cryptohome::ErrorWrapper::CreateFromErrorCodeOnly(
          user_data_auth::CRYPTOHOME_ERROR_AUTHORIZATION_KEY_FAILED));

  ActiveSessionAuthControllerImpl::TestApi(controller)
      .SubmitPassword(kExpectedPassword);

  // Await authentication.
  base::RunLoop().RunUntilIdle();

  auto authenticate_auth_factor_request =
      FakeUserDataAuthClient::Get()
          ->GetLastRequest<
              FakeUserDataAuthClient::Operation::kAuthenticateAuthFactor>();

  EXPECT_EQ(
      authenticate_auth_factor_request.auth_input().password_input().secret(),
      HashPassword(kExpectedPassword));

  std::visit(base::Overloaded([](auto&& arg) { EXPECT_FALSE(arg->IsReady()); }),
             future);
}

// Tests that the AuthenticateAuthFactor call to cryptohome includes the
// correct account id and pin, and that the `OnAuthComplete` callback
// is called with the correct credentials.
TEST_P(ActiveSessionAuthControllerTest, SubmitPin) {
  AddGaiaPassword(account_id_, kExpectedPassword);
  AddCryptohomePin(account_id_, kExpectedPin);

  user_manager::KnownUser known_user(Shell::Get()->local_state());
  known_user.SetStringPref(account_id_, prefs::kQuickUnlockPinSalt,
                           kExpectedSalt);

  auto* controller = static_cast<ActiveSessionAuthControllerImpl*>(
      Shell::Get()->active_session_auth_controller());

  auto future = ShowAuthDialogForVariant(GetParam());

  // Await show.
  base::RunLoop().RunUntilIdle();

  ActiveSessionAuthControllerImpl::TestApi(controller).SubmitPin(kExpectedPin);

  // Await authentication.
  base::RunLoop().RunUntilIdle();

  auto authenticate_auth_factor_request =
      FakeUserDataAuthClient::Get()
          ->GetLastRequest<
              FakeUserDataAuthClient::Operation::kAuthenticateAuthFactor>();

  EXPECT_EQ(authenticate_auth_factor_request.auth_input().pin_input().secret(),
            HashPin(kExpectedPin));

  std::visit(base::Overloaded([](auto&& arg) {
               EXPECT_TRUE(arg->IsReady());
               EXPECT_EQ(arg->template Get<bool>(), true);
             }),
             future);
}

// Tests that the AuthenticateAuthFactor call to cryptohome includes the
// account id and pin, and that the `OnAuthComplete` callback
// is not called with a wrong credentials error reply.
TEST_P(ActiveSessionAuthControllerTest, WrongPin) {
  AddGaiaPassword(account_id_, kExpectedPassword);
  AddCryptohomePin(account_id_, kExpectedPin);

  user_manager::KnownUser known_user(Shell::Get()->local_state());
  known_user.SetStringPref(account_id_, prefs::kQuickUnlockPinSalt,
                           kExpectedSalt);

  auto* controller = static_cast<ActiveSessionAuthControllerImpl*>(
      Shell::Get()->active_session_auth_controller());

  auto future = ShowAuthDialogForVariant(GetParam());

  // Await show.
  base::RunLoop().RunUntilIdle();

  FakeUserDataAuthClient::Get()->SetNextOperationError(
      FakeUserDataAuthClient::Operation::kAuthenticateAuthFactor,
      cryptohome::ErrorWrapper::CreateFromErrorCodeOnly(
          user_data_auth::CRYPTOHOME_ERROR_AUTHORIZATION_KEY_FAILED));

  ActiveSessionAuthControllerImpl::TestApi(controller).SubmitPin(kExpectedPin);

  // Await authentication.
  base::RunLoop().RunUntilIdle();

  auto authenticate_auth_factor_request =
      FakeUserDataAuthClient::Get()
          ->GetLastRequest<
              FakeUserDataAuthClient::Operation::kAuthenticateAuthFactor>();

  EXPECT_EQ(authenticate_auth_factor_request.auth_input().pin_input().secret(),
            HashPin(kExpectedPin));

  std::visit(base::Overloaded([](auto&& arg) { EXPECT_FALSE(arg->IsReady()); }),
             future);
}

// Tests that the AuthenticateAuthFactor calls to cryptohome are
// correctly formed when pin and password authentication are both
// tried.
TEST_P(ActiveSessionAuthControllerTest, BadPinThenGoodPassword) {
  AddGaiaPassword(account_id_, kExpectedPassword);
  AddCryptohomePin(account_id_, kExpectedPin);
  const std::string bad_pin = "bad_pin";

  user_manager::KnownUser known_user(Shell::Get()->local_state());
  known_user.SetStringPref(account_id_, prefs::kQuickUnlockPinSalt,
                           kExpectedSalt);

  auto* controller = static_cast<ActiveSessionAuthControllerImpl*>(
      Shell::Get()->active_session_auth_controller());

  auto future = ShowAuthDialogForVariant(GetParam());

  // Await show.
  base::RunLoop().RunUntilIdle();

  // Await authentication with pin.
  FakeUserDataAuthClient::TestApi::Get()->set_enable_auth_check(true);
  ActiveSessionAuthControllerImpl::TestApi(controller).SubmitPin(bad_pin);
  base::RunLoop().RunUntilIdle();

  auto authenticate_auth_factor_request =
      FakeUserDataAuthClient::Get()
          ->GetLastRequest<
              FakeUserDataAuthClient::Operation::kAuthenticateAuthFactor>();

  EXPECT_EQ(authenticate_auth_factor_request.auth_input().pin_input().secret(),
            HashPin(bad_pin));

  std::visit(base::Overloaded([](auto&& arg) { EXPECT_FALSE(arg->IsReady()); }),
             future);

  // Await authentication with password.
  ActiveSessionAuthControllerImpl::TestApi(controller)
      .SubmitPassword(kExpectedPassword);
  base::RunLoop().RunUntilIdle();

  authenticate_auth_factor_request =
      FakeUserDataAuthClient::Get()
          ->GetLastRequest<
              FakeUserDataAuthClient::Operation::kAuthenticateAuthFactor>();

  EXPECT_EQ(
      authenticate_auth_factor_request.auth_input().password_input().secret(),
      HashPassword(kExpectedPassword));

  std::visit(base::Overloaded([](auto&& arg) {
               EXPECT_TRUE(arg->IsReady());
               EXPECT_EQ(arg->template Get<bool>(), true);
             }),
             future);
}

// Check the format and content of pin lockout status message.
TEST_P(ActiveSessionAuthControllerTest, PinLockoutMessage) {
  AddGaiaPassword(account_id_, kExpectedPassword);
  AddCryptohomePin(account_id_, kExpectedPin);
  const std::string bad_pin = "bad_pin";

  user_manager::KnownUser known_user(Shell::Get()->local_state());
  known_user.SetStringPref(account_id_, prefs::kQuickUnlockPinSalt,
                           kExpectedSalt);

  auto* controller = static_cast<ActiveSessionAuthControllerImpl*>(
      Shell::Get()->active_session_auth_controller());
  auto test_api = ActiveSessionAuthControllerImpl::TestApi(controller);

  ShowAuthDialogForVariant(GetParam());

  // Await show.
  ASSERT_TRUE(base::test::RunUntil([&]() { return controller->IsShown(); }));

  const base::TimeDelta in_a_while = base::Seconds(60);
  test_api.SetPinStatus(std::make_unique<cryptohome::PinStatus>(in_a_while));

  EXPECT_EQ(
      test_api.GetPinStatusMessage(),
      l10n_util::GetStringFUTF16(IDS_ASH_IN_SESSION_AUTH_PIN_DELAY_REQUIRED,
                                 u"1 minute, 0 seconds"));

  test_api.SetPinStatus(
      std::make_unique<cryptohome::PinStatus>(base::TimeDelta::Max()));
  EXPECT_EQ(
      test_api.GetPinStatusMessage(),
      l10n_util::GetStringUTF16(IDS_ASH_IN_SESSION_AUTH_PIN_TOO_MANY_ATTEMPTS));
}

// Tests that the OnAuthCancel callback is called with the correct
// parameters.
TEST_P(ActiveSessionAuthControllerTest, OnAuthCancel) {
  AddGaiaPassword(account_id_, kExpectedPassword);

  auto* controller = static_cast<ActiveSessionAuthControllerImpl*>(
      Shell::Get()->active_session_auth_controller());

  auto future = ShowAuthDialogForVariant(GetParam());

  // Await show.
  base::RunLoop().RunUntilIdle();

  ActiveSessionAuthControllerImpl::TestApi(controller).Close();

  // Await close.
  base::RunLoop().RunUntilIdle();

  std::visit(base::Overloaded(
                 [](std::unique_ptr<WebAuthNCallback>& callback) {
                   EXPECT_TRUE(callback->IsReady());
                   EXPECT_FALSE(callback->Get<bool>());
                 },
                 [](std::unique_ptr<TokenBasedCallback>& callback) {
                   EXPECT_TRUE(callback->IsReady());
                   EXPECT_FALSE(callback->Get<bool>());
                   EXPECT_EQ(callback->Get<1>(), std::string{});
                 }),
             future);
}

INSTANTIATE_TEST_SUITE_P(All,
                         ActiveSessionAuthControllerTest,
                         testing::Values(TestVariant::kWebAuthN,
                                         TestVariant::kSettings,
                                         TestVariant::kPasswordManager));

}  // namespace ash
