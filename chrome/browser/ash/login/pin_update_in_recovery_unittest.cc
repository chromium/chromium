// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>

#include "ash/constants/ash_pref_names.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "chrome/browser/ash/login/quick_unlock/auth_token.h"
#include "chrome/browser/ash/login/quick_unlock/pin_backend.h"
#include "chrome/browser/ash/login/quick_unlock/quick_unlock_factory.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chromeos/ash/components/cryptohome/system_salt_getter.h"
#include "chromeos/ash/components/dbus/userdataauth/cryptohome_misc_client.h"
#include "chromeos/ash/components/dbus/userdataauth/fake_userdataauth_client.h"
#include "chromeos/ash/components/login/auth/public/cryptohome_key_constants.h"
#include "chromeos/ash/components/osauth/public/auth_session_storage.h"
#include "chromeos/ash/components/osauth/public/common_types.h"
#include "chromeos/ash/services/auth_factor_config/auth_factor_config.h"
#include "chromeos/ash/services/auth_factor_config/in_process_instances.h"
#include "chromeos/ash/services/auth_factor_config/pin_factor_editor.h"
#include "chromeos/ash/services/auth_factor_config/public/mojom/auth_factor_config.mojom-forward.h"
#include "chromeos/ash/services/auth_factor_config/public/mojom/auth_factor_config.mojom-shared.h"
#include "components/prefs/testing_pref_service.h"
#include "components/user_manager/fake_user_manager.h"
#include "components/user_manager/known_user.h"
#include "components/user_manager/user_manager_impl.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

constexpr char kUserEmail[] = "expected_email@example.com";
constexpr char kOldPin[] = "123456";
constexpr char kNewPin[] = "234567";

class PinFactorEditorTest : public testing::Test {
 public:
  PinFactorEditorTest() = default;
  PinFactorEditorTest(const PinFactorEditorTest&) = delete;
  PinFactorEditorTest& operator=(const PinFactorEditorTest&) = delete;
  ~PinFactorEditorTest() override = default;

 protected:
  void SetUp() override {
    UserDataAuthClient::InitializeFake();
    InitializeUserManager();
    AddUserToUserManager();
    SystemSaltGetter::Initialize();
    CryptohomeMiscClient::InitializeFake();
    auth_parts_ = AuthParts::Create(&local_state_);
    TestingBrowserProcess::GetGlobal()->SetLocalState(&local_state_);
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

  void AddUserToCryptohome(const AccountId& user) {
    auto account_identifier =
        cryptohome::CreateAccountIdentifierFromAccountId(user);

    FakeUserDataAuthClient::TestApi::Get()->SetServiceIsAvailable(true);
    FakeUserDataAuthClient::TestApi::Get()
        ->set_supports_low_entropy_credentials(true);
    FakeUserDataAuthClient::TestApi::Get()->AddExistingUser(account_identifier);
  }

  void TearDown() override {
    SystemSaltGetter::Shutdown();
    CryptohomeMiscClient::Shutdown();
    auth_parts_.reset();
    user_manager_->Destroy();
    user_manager_.reset();
    UserDataAuthClient::Shutdown();
    TestingBrowserProcess::GetGlobal()->SetLocalState(nullptr);
    quick_unlock::PinBackend::ResetForTesting();
  }

  std::string HashPin(const std::string& unhashed_pin) {
    Key key(std::move(unhashed_pin));
    user_manager::KnownUser known_user(&local_state_);
    std::string salt;
    known_user.GetStringPrefForTest(account_id_,
                                    ash::prefs::kQuickUnlockPinSalt, &salt);
    key.Transform(Key::KEY_TYPE_SALTED_PBKDF2_AES256_1234, salt);
    return key.GetSecret();
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

  std::unique_ptr<ash::UserContext> MakeEmptyUserContext() {
    std::unique_ptr<ash::UserContext> auth_token_user_context_ =
        std::make_unique<ash::UserContext>();
    // Generate an auth token.
    auth_token_user_context_->SetAccountId(account_id_);
    auth_token_user_context_->SetSessionLifetime(
        base::Time::Now() + ash::quick_unlock::AuthToken::kTokenExpiration);

    auto* fake_userdataauth_client_testapi =
        ash::FakeUserDataAuthClient::TestApi::Get();

    auto session_ids = fake_userdataauth_client_testapi->AddSession(
        cryptohome::CreateAccountIdentifierFromAccountId(account_id_),
        /*authenticated=*/true);
    auth_token_user_context_->SetAuthSessionIds(session_ids.first,
                                                session_ids.second);

    return auth_token_user_context_;
  }

  std::unique_ptr<ash::UserContext> MakeUserContextWithPassword() {
    std::unique_ptr<ash::UserContext> auth_token_user_context_ =
        MakeEmptyUserContext();

    cryptohome::AuthFactorRef ref(
        cryptohome::AuthFactorType::kPassword,
        cryptohome::KeyLabel{kCryptohomeGaiaKeyLabel});
    cryptohome::AuthFactor password(ref,
                                    cryptohome::AuthFactorCommonMetadata());

    cryptohome::AuthFactorsSet factors;
    factors.Put(cryptohome::AuthFactorType::kPassword);
    factors.Put(cryptohome::AuthFactorType::kRecovery);

    auth_token_user_context_->SetAuthFactorsConfiguration(
        ash::AuthFactorsConfiguration({password}, factors));

    return auth_token_user_context_;
  }

  std::unique_ptr<ash::UserContext> MakeUserContextWithPin() {
    std::unique_ptr<ash::UserContext> auth_token_user_context_ =
        MakeEmptyUserContext();

    cryptohome::AuthFactorsSet factors;
    factors.Put(cryptohome::AuthFactorType::kPin);
    factors.Put(cryptohome::AuthFactorType::kRecovery);

    cryptohome::AuthFactorRef ref(cryptohome::AuthFactorType::kPin,
                                  cryptohome::KeyLabel{kCryptohomePinLabel});
    cryptohome::AuthFactor pin(ref, cryptohome::AuthFactorCommonMetadata());

    auth_token_user_context_->SetAuthFactorsConfiguration(
        ash::AuthFactorsConfiguration({pin}, factors));

    return auth_token_user_context_;
  }

  void VerifyPinUpdateSuccess(
      base::test::TestFuture<auth::mojom::ConfigureResult>& future) {
    EXPECT_EQ(future.Get(), auth::mojom::ConfigureResult::kSuccess);
    EXPECT_TRUE(
        FakeUserDataAuthClient::Get()
            ->WasCalled<
                FakeUserDataAuthClient::Operation::kUpdateAuthFactor>());
    auto update_auth_factor_request =
        FakeUserDataAuthClient::Get()
            ->GetLastRequest<
                FakeUserDataAuthClient::Operation::kUpdateAuthFactor>();
    EXPECT_EQ(update_auth_factor_request.auth_factor().type(),
              user_data_auth::AUTH_FACTOR_TYPE_PIN);
    EXPECT_EQ(update_auth_factor_request.auth_input().pin_input().secret(),
              HashPin(kNewPin));
  }

  std::unique_ptr<auth::PinFactorEditor> GetPinFactorEditor() {
    auto* pin_backend = quick_unlock::PinBackend::GetInstance();

    auth_factor_config_ = std::make_unique<auth::AuthFactorConfig>(
        &quick_unlock::QuickUnlockFactory::GetDelegate(), &local_state_);
    auto pin_factor_editor = std::make_unique<auth::PinFactorEditor>(
        auth_factor_config_.get(), pin_backend);

    return pin_factor_editor;
  }

  AccountId account_id_;
  std::unique_ptr<auth::AuthFactorConfig> auth_factor_config_;
  base::test::TaskEnvironment env_;
  TestingPrefServiceSimple local_state_;
  std::unique_ptr<user_manager::FakeUserManager> user_manager_;
  std::unique_ptr<AuthParts> auth_parts_;
};

// Assert that we get an error if we try to update the pin via PinFactorEditor
// when there's no crypthome pin already set for the user.
TEST_F(PinFactorEditorTest, CannotUpdatePinIfNotAlreadyPresent) {
  AddUserToCryptohome(account_id_);
  const std::string token =
      ash::AuthSessionStorage::Get()->Store(MakeUserContextWithPassword());

  std::unique_ptr<auth::PinFactorEditor> pin_factor_editor =
      GetPinFactorEditor();

  base::test::TestFuture<auth::mojom::ConfigureResult> future;

  pin_factor_editor->UpdatePin(token, kNewPin, future.GetCallback());

  EXPECT_EQ(future.Get(), auth::mojom::ConfigureResult::kFatalError);
}

// Assert that we can update the user's cryptohome pin when one is already
// present.
TEST_F(PinFactorEditorTest, CanUpdatePinIfAlreadyPresent) {
  AddUserToCryptohome(account_id_);
  AddCryptohomePin(account_id_, kOldPin);

  const std::string token =
      ash::AuthSessionStorage::Get()->Store(MakeUserContextWithPin());

  std::unique_ptr<auth::PinFactorEditor> pin_factor_editor =
      GetPinFactorEditor();

  base::test::TestFuture<auth::mojom::ConfigureResult> future;

  pin_factor_editor->UpdatePin(token, kNewPin, future.GetCallback());

  VerifyPinUpdateSuccess(future);
}

}  // namespace ash
