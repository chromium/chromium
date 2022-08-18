// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/quick_unlock/pin_storage_cryptohome.h"

#include <vector>

#include "ash/components/login/auth/public/cryptohome_key_constants.h"
#include "ash/components/login/auth/public/user_context.h"
#include "base/bind.h"
#include "base/logging.h"
#include "base/run_loop.h"
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
#include "components/account_id/account_id.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::quick_unlock {

namespace {

using ::cryptohome::KeyLabel;

constexpr char kDummyPin[] = "123456";

class PinStorageCryptohomeUnitTest : public testing::Test {
 protected:
  PinStorageCryptohomeUnitTest() = default;
  ~PinStorageCryptohomeUnitTest() override = default;

  // testing::Test:
  void SetUp() override {
    test_api_ = std::make_unique<TestApi>(/*override_quick_unlock=*/true);
    test_api_->EnablePinByPolicy(Purpose::kAny);
    SystemSaltGetter::Initialize();
    CryptohomeMiscClient::InitializeFake();
    UserDataAuthClient::InitializeFake();
    FakeUserDataAuthClient::TestApi::Get()
        ->set_supports_low_entropy_credentials(true);
    FakeUserDataAuthClient::TestApi::Get()->set_enable_auth_check(true);
    storage_ = std::make_unique<PinStorageCryptohome>();
    storage_->SetPinSaltStorageForTesting(
        std::make_unique<FakePinSaltStorage>());
  }

  void TearDown() override {
    UserDataAuthClient::Shutdown();
    CryptohomeMiscClient::Shutdown();
    SystemSaltGetter::Shutdown();
  }

  bool IsPinSet() const {
    bool res;
    base::RunLoop loop;
    storage_->IsPinSetInCryptohome(
        test_account_id_,
        base::BindOnce(
            [](base::OnceClosure closure, bool* res, bool is_set) {
              *res = is_set;
              std::move(closure).Run();
            },
            loop.QuitClosure(), &res));
    loop.Run();
    return res;
  }

  bool CanAuthenticate() const {
    bool res;
    base::RunLoop loop;
    storage_->CanAuthenticate(
        test_account_id_, Purpose::kAny,
        base::BindOnce(
            [](base::OnceClosure closure, bool* res, bool can_auth) {
              *res = can_auth;
              std::move(closure).Run();
            },
            loop.QuitClosure(), &res));
    loop.Run();
    return res;
  }

  bool TryAuthenticate(const std::string& pin, Purpose purpose) const {
    bool res;
    base::RunLoop loop;
    storage_->TryAuthenticate(
        test_account_id_, Key(pin), purpose,
        base::BindOnce(
            [](base::OnceClosure closure, bool* res, bool auth_success) {
              *res = auth_success;
              std::move(closure).Run();
            },
            loop.QuitClosure(), &res));
    loop.Run();
    return res;
  }

  bool SetPin(const std::string& pin) const {
    UserContext user_context;
    user_context.SetAccountId(test_account_id_);
    user_context.SetKey(Key(pin));

    bool res;
    base::RunLoop loop;
    storage_->SetPin(
        user_context, pin, absl::nullopt,
        base::BindOnce(
            [](base::OnceClosure closure, bool* res, bool did_set) {
              *res = did_set;
              std::move(closure).Run();
            },
            loop.QuitClosure(), &res));

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

  bool RemovePin(const std::string& pin) const {
    UserContext user_context;
    user_context.SetAccountId(test_account_id_);
    user_context.SetKey(Key(pin));

    bool res;
    base::RunLoop loop;
    storage_->RemovePin(user_context, base::BindOnce(
                                          [](base::OnceClosure closure,
                                             bool* res, bool did_remove) {
                                            *res = did_remove;
                                            std::move(closure).Run();
                                          },
                                          loop.QuitClosure(), &res));

    loop.Run();

    return res;
  }

  content::BrowserTaskEnvironment task_environment_;

  std::unique_ptr<PinStorageCryptohome> storage_;
  AccountId test_account_id_{
      AccountId::FromUserEmailGaiaId("user@example.com", "11111")};
  std::unique_ptr<TestApi> test_api_;
};

}  // namespace

// Verifies that cryptohome pin is supported.
TEST_F(PinStorageCryptohomeUnitTest, IsSupported) {
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
TEST_F(PinStorageCryptohomeUnitTest, PinNotSet) {
  ASSERT_FALSE(IsPinSet());
  ASSERT_FALSE(CanAuthenticate());
}

// Password key is set. Pin auth should be disallowed.
TEST_F(PinStorageCryptohomeUnitTest, PasswordSet) {
  SetPassword(kDummyPin);
  ASSERT_FALSE(IsPinSet());
  ASSERT_FALSE(CanAuthenticate());
}

// Verifies Set/Remove pin flow.
TEST_F(PinStorageCryptohomeUnitTest, SetRemovePin) {
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
TEST_F(PinStorageCryptohomeUnitTest, AuthLockedTest) {
  SetAuthLockedPin(kDummyPin);

  ASSERT_FALSE(CanAuthenticate());
  ASSERT_TRUE(IsPinSet());
}

// Verifies the `unlock_webauthn_secret` parameter is set correctly when
// TryAuthenticate with different purposes.
TEST_F(PinStorageCryptohomeUnitTest, UnlockWebAuthnSecret) {
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

}  // namespace ash::quick_unlock
