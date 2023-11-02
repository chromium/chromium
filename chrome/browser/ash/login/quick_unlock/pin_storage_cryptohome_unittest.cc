// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/quick_unlock/pin_storage_cryptohome.h"

#include <vector>

#include "ash/constants/ash_features.h"
#include "base/bind.h"
#include "base/logging.h"
#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ash/login/quick_unlock/fake_pin_salt_storage.h"
#include "chrome/browser/ash/login/quick_unlock/pin_backend.h"
#include "chrome/browser/ash/login/quick_unlock/quick_unlock_utils.h"
#include "chromeos/ash/components/cryptohome/common_types.h"
#include "chromeos/ash/components/cryptohome/cryptohome_parameters.h"
#include "chromeos/ash/components/cryptohome/cryptohome_util.h"
#include "chromeos/ash/components/cryptohome/system_salt_getter.h"
#include "chromeos/ash/components/dbus/cryptohome/rpc.pb.h"
#include "chromeos/ash/components/dbus/userdataauth/fake_cryptohome_misc_client.h"
#include "chromeos/ash/components/dbus/userdataauth/fake_userdataauth_client.h"
#include "chromeos/ash/components/login/auth/public/cryptohome_key_constants.h"
#include "chromeos/ash/components/login/auth/public/user_context.h"
#include "components/account_id/account_id.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::quick_unlock {

namespace {

using ::cryptohome::KeyLabel;

constexpr char kDummyPin[] = "123456";

struct Param {
  const bool use_auth_factors_feature;
};

class PinStorageCryptohomeUnitTest : public testing::TestWithParam<Param> {
 protected:
  PinStorageCryptohomeUnitTest() {
    if (GetParam().use_auth_factors_feature) {
      feature_list_.InitAndEnableFeature(features::kUseAuthFactors);
    } else {
      feature_list_.InitAndDisableFeature(features::kUseAuthFactors);
    }
  }
  ~PinStorageCryptohomeUnitTest() override = default;

  // testing::Test:
  void SetUp() override {
    user_context_ = std::make_unique<UserContext>(
        user_manager::USER_TYPE_REGULAR, test_account_id_);

    test_api_ = std::make_unique<TestApi>(/*override_quick_unlock=*/true);
    test_api_->EnablePinByPolicy(Purpose::kAny);
    SystemSaltGetter::Initialize();
    CryptohomeMiscClient::InitializeFake();
    UserDataAuthClient::InitializeFake();
    FakeUserDataAuthClient::TestApi::Get()
        ->set_supports_low_entropy_credentials(true);
    FakeUserDataAuthClient::TestApi::Get()->set_enable_auth_check(true);

    const auto cryptohome_user_id =
        cryptohome::CreateAccountIdentifierFromAccountId(test_account_id_);
    FakeUserDataAuthClient::TestApi::Get()->AddExistingUser(cryptohome_user_id);
    if (features::IsUseAuthFactorsEnabled()) {
      std::string session = FakeUserDataAuthClient::TestApi::Get()->AddSession(
          cryptohome_user_id, true /*authenticated*/);
      user_context_->SetIsUsingPin(true);
      user_context_->SetAuthSessionId(std::move(session));
      user_context_->SetAuthFactorsConfiguration(AuthFactorsConfiguration());
    }

    storage_ = std::make_unique<PinStorageCryptohome>();
    storage_->SetPinSaltStorageForTesting(
        std::make_unique<FakePinSaltStorage>());
  }

  void TearDown() override {
    UserDataAuthClient::Shutdown();
    CryptohomeMiscClient::Shutdown();
    SystemSaltGetter::Shutdown();
  }

  bool IsPinSet() {
    bool res;
    base::RunLoop loop;
    storage_->IsPinSetInCryptohome(
        std::make_unique<UserContext>(*user_context_),
        base::BindOnce(
            [](base::OnceClosure closure, bool* res, bool is_set) {
              *res = is_set;
              std::move(closure).Run();
            },
            loop.QuitClosure(), &res));
    loop.Run();
    return res;
  }

  bool CanAuthenticate() {
    bool res;
    base::RunLoop loop;
    storage_->CanAuthenticate(
        std::make_unique<UserContext>(*user_context_), Purpose::kAny,
        base::BindOnce(
            [](base::OnceClosure closure, bool* res, bool can_auth) {
              *res = can_auth;
              std::move(closure).Run();
            },
            loop.QuitClosure(), &res));
    loop.Run();
    return res;
  }

  bool TryAuthenticate(const std::string& pin, Purpose purpose) {
    auto user_context = std::make_unique<UserContext>(*user_context_);
    user_context->ResetAuthSessionId();
    bool res;
    base::RunLoop loop;
    storage_->TryAuthenticate(
        std::move(user_context), Key(pin), purpose,
        base::BindOnce(
            [](PinStorageCryptohomeUnitTest* self, bool* res,
               base::OnceClosure closure,
               std::unique_ptr<UserContext> user_context,
               absl::optional<AuthenticationError> error) {
              *res = !error.has_value();
              std::move(closure).Run();
            },
            base::Unretained(this), &res, loop.QuitClosure()));
    loop.Run();
    return res;
  }

  bool SetPin(const std::string& pin) {
    UserContext user_context;
    user_context.SetAccountId(test_account_id_);
    user_context.SetKey(Key(pin));

    bool res;
    base::RunLoop loop;
    storage_->SetPin(std::move(user_context_), pin, absl::nullopt,
                     base::BindOnce(
                         [](PinStorageCryptohomeUnitTest* self, bool* res,
                            base::OnceClosure closure,
                            std::unique_ptr<UserContext> user_context,
                            absl::optional<AuthenticationError> error) {
                           self->user_context_ = std::move(user_context);
                           *res = !error.has_value();
                           std::move(closure).Run();
                         },
                         base::Unretained(this), &res, loop.QuitClosure()));

    loop.Run();
    return res;
  }

  void SetPassword(const std::string& password) const {
    ::user_data_auth::AddKeyRequest request;

    const cryptohome::KeyDefinition key_def =
        cryptohome::KeyDefinition::CreateForPassword(
            password, KeyLabel(kCryptohomeGaiaKeyLabel),
            cryptohome::PRIV_MIGRATE);
    cryptohome::KeyDefinitionToKey(key_def, request.mutable_key());
    *request.mutable_account_id() =
        cryptohome::CreateAccountIdentifierFromAccountId(test_account_id_);
    // Ensure that has_authorization_request() would return true.
    request.mutable_authorization_request();
    base::RunLoop run_loop;
    UserDataAuthClient::Get()->AddKey(
        request, base::BindOnce(
                     [](base::OnceClosure closure,
                        absl::optional<::user_data_auth::AddKeyReply> reply) {
                       std::move(closure).Run();
                     },
                     run_loop.QuitClosure()));
    run_loop.Run();
  }

  // Setup a pin which has policy `auth_locked` true. That means the pin can't
  // be used for authentication because of the TPM protection.
  void SetAuthLockedPin(const std::string& pin) const {
    ::user_data_auth::AddKeyRequest request;

    const cryptohome::KeyDefinition key_def =
        cryptohome::KeyDefinition::CreateForPassword(
            pin, KeyLabel(kCryptohomePinLabel), cryptohome::PRIV_MIGRATE);
    cryptohome::KeyDefinitionToKey(key_def, request.mutable_key());
    request.mutable_key()
        ->mutable_data()
        ->mutable_policy()
        ->set_low_entropy_credential(true);
    *request.mutable_account_id() =
        cryptohome::CreateAccountIdentifierFromAccountId(test_account_id_);
    // Ensure that has_authorization_request() would return true.
    request.mutable_authorization_request();
    base::RunLoop run_loop;
    UserDataAuthClient::Get()->AddKey(
        request, base::BindOnce(
                     [](base::OnceClosure closure,
                        absl::optional<::user_data_auth::AddKeyReply> reply) {
                       std::move(closure).Run();
                     },
                     run_loop.QuitClosure()));

    run_loop.Run();
    FakeUserDataAuthClient::TestApi::Get()->SetPinLocked(
        cryptohome::CreateAccountIdentifierFromAccountId(test_account_id_),
        kCryptohomePinLabel, true);
  }

  bool RemovePin(const std::string& pin) {
    UserContext user_context;
    user_context.SetAccountId(test_account_id_);
    user_context.SetKey(Key(pin));

    bool res;
    base::RunLoop loop;
    storage_->RemovePin(std::move(user_context_),
                        base::BindOnce(
                            [](PinStorageCryptohomeUnitTest* self, bool* res,
                               base::OnceClosure closure,
                               std::unique_ptr<UserContext> user_context,
                               absl::optional<AuthenticationError> error) {
                              self->user_context_ = std::move(user_context);
                              *res = !error.has_value();
                              std::move(closure).Run();
                            },
                            base::Unretained(this), &res, loop.QuitClosure()));

    loop.Run();

    return res;
  }

  content::BrowserTaskEnvironment task_environment_;

  std::unique_ptr<PinStorageCryptohome> storage_;
  AccountId test_account_id_{
      AccountId::FromUserEmailGaiaId("user@example.com", "11111")};
  std::unique_ptr<UserContext> user_context_ =
      std::make_unique<UserContext>(user_manager::USER_TYPE_REGULAR,
                                    test_account_id_);
  std::unique_ptr<TestApi> test_api_;

  base::test::ScopedFeatureList feature_list_;
};

}  // namespace

// Verifies that cryptohome pin is supported.
TEST_P(PinStorageCryptohomeUnitTest, IsSupported) {
  base::RunLoop loop;
  PinStorageCryptohome::IsSupported(base::BindOnce(
      [](base::OnceClosure closure, bool supported) {
        EXPECT_TRUE(supported);
        std::move(closure).Run();
      },
      loop.QuitClosure()));
  loop.Run();
}

// No keys are set.
TEST_P(PinStorageCryptohomeUnitTest, PinNotSet) {
  ASSERT_FALSE(IsPinSet());
  ASSERT_FALSE(CanAuthenticate());
}

// Password key is set. Pin auth should be disallowed.
TEST_P(PinStorageCryptohomeUnitTest, PasswordSet) {
  SetPassword(kDummyPin);
  ASSERT_FALSE(IsPinSet());
  ASSERT_FALSE(CanAuthenticate());
}

// Verifies Set/Remove pin flow.
TEST_P(PinStorageCryptohomeUnitTest, SetRemovePin) {
  ASSERT_TRUE(SetPin(kDummyPin));
  ASSERT_TRUE(IsPinSet());
  ASSERT_TRUE(CanAuthenticate());
  EXPECT_TRUE(TryAuthenticate(kDummyPin, Purpose::kAny));

  ASSERT_TRUE(RemovePin(kDummyPin));
  ASSERT_FALSE(IsPinSet());
  ASSERT_FALSE(CanAuthenticate());
  EXPECT_FALSE(TryAuthenticate(kDummyPin, Purpose::kAny));
}

// Verifies case when pin can't be used to authenticate (`auth_locked` == True).
TEST_P(PinStorageCryptohomeUnitTest, AuthLockedTest) {
  SetAuthLockedPin(kDummyPin);

  ASSERT_FALSE(CanAuthenticate());
  ASSERT_TRUE(IsPinSet());
}

// Verifies the `unlock_webauthn_secret` parameter is set correctly when
// TryAuthenticate with different purposes.
TEST_P(PinStorageCryptohomeUnitTest, UnlockWebAuthnSecret) {
  ASSERT_TRUE(SetPin(kDummyPin));
  ASSERT_TRUE(IsPinSet());
  ASSERT_TRUE(CanAuthenticate());

  // Only calling TryAuthenticate with purpose Purpose::kWebAuthn should set the
  // `unlock_webauthn_secret` parameter to true.

  ASSERT_TRUE(TryAuthenticate(kDummyPin, Purpose::kAny));
  EXPECT_FALSE(
      FakeUserDataAuthClient::Get()->get_last_unlock_webauthn_secret());

  test_api_->EnablePinByPolicy(Purpose::kWebAuthn);
  ASSERT_TRUE(TryAuthenticate(kDummyPin, Purpose::kWebAuthn));
  EXPECT_TRUE(FakeUserDataAuthClient::Get()->get_last_unlock_webauthn_secret());

  test_api_->EnablePinByPolicy(Purpose::kUnlock);
  ASSERT_TRUE(TryAuthenticate(kDummyPin, Purpose::kUnlock));
  EXPECT_FALSE(
      FakeUserDataAuthClient::Get()->get_last_unlock_webauthn_secret());
}

INSTANTIATE_TEST_SUITE_P(
    All,
    PinStorageCryptohomeUnitTest,
    testing::Values(Param{.use_auth_factors_feature = false},
                    Param{.use_auth_factors_feature = true}));

}  // namespace ash::quick_unlock
