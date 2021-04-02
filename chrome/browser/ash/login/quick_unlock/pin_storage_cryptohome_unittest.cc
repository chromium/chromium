// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/quick_unlock/pin_storage_cryptohome.h"

#include <vector>

#include "base/bind.h"
#include "base/callback_forward.h"
#include "base/logging.h"
#include "base/run_loop.h"
#include "chrome/browser/ash/login/quick_unlock/pin_backend.h"
#include "chrome/browser/ash/login/quick_unlock/quick_unlock_utils.h"
#include "chromeos/cryptohome/cryptohome_parameters.h"
#include "chromeos/cryptohome/cryptohome_util.h"
#include "chromeos/cryptohome/system_salt_getter.h"
#include "chromeos/dbus/cryptohome/rpc.pb.h"
#include "chromeos/dbus/userdataauth/fake_cryptohome_misc_client.h"
#include "chromeos/dbus/userdataauth/fake_userdataauth_client.h"
#include "chromeos/login/auth/cryptohome_key_constants.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {
namespace quick_unlock {

namespace {

constexpr char kDummyPin[] = "123456";

class PinStorageCryptohomeUnitTest : public testing::Test {
 protected:
  PinStorageCryptohomeUnitTest() = default;
  ~PinStorageCryptohomeUnitTest() override = default;

  // testing::Test:
  void SetUp() override {
    quick_unlock::EnabledForTesting(true);
    SystemSaltGetter::Initialize();
    CryptohomeMiscClient::InitializeFake();
    UserDataAuthClient::InitializeFake();
    FakeUserDataAuthClient::Get()->set_supports_low_entropy_credentials(true);
    storage_ = std::make_unique<PinStorageCryptohome>();
  }

  void TearDown() override {
    UserDataAuthClient::Shutdown();
    CryptohomeMiscClient::Shutdown();
    SystemSaltGetter::Shutdown();
    quick_unlock::EnabledForTesting(false);
    quick_unlock::IsFingerprintEnabled(nullptr);
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
        test_account_id_,
        base::BindOnce(
            [](base::OnceClosure closure, bool* res, bool can_auth) {
              *res = can_auth;
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
        user_context, pin, base::nullopt,
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
            password, kCryptohomeGaiaKeyLabel, cryptohome::PRIV_MIGRATE);
    cryptohome::KeyDefinitionToKey(key_def, request.mutable_key());
    *request.mutable_account_id() =
        cryptohome::CreateAccountIdentifierFromAccountId(test_account_id_);
    // Ensure that has_authorization_request() would return true.
    request.mutable_authorization_request();
    base::RunLoop run_loop;
    chromeos::UserDataAuthClient::Get()->AddKey(
        request, base::BindOnce(
                     [](base::OnceClosure closure,
                        base::Optional<::user_data_auth::AddKeyReply> reply) {
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
        cryptohome::KeyDefinition::CreateForPassword(pin, kCryptohomePinLabel,
                                                     cryptohome::PRIV_MIGRATE);
    cryptohome::KeyDefinitionToKey(key_def, request.mutable_key());
    request.mutable_key()
        ->mutable_data()
        ->mutable_policy()
        ->set_low_entropy_credential(true);
    request.mutable_key()->mutable_data()->mutable_policy()->set_auth_locked(
        true);
    *request.mutable_account_id() =
        cryptohome::CreateAccountIdentifierFromAccountId(test_account_id_);
    // Ensure that has_authorization_request() would return true.
    request.mutable_authorization_request();
    base::RunLoop run_loop;
    chromeos::UserDataAuthClient::Get()->AddKey(
        request, base::BindOnce(
                     [](base::OnceClosure closure,
                        base::Optional<::user_data_auth::AddKeyReply> reply) {
                       std::move(closure).Run();
                     },
                     run_loop.QuitClosure()));
    run_loop.Run();
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

  ASSERT_TRUE(RemovePin(kDummyPin));
  ASSERT_FALSE(IsPinSet());
  ASSERT_FALSE(CanAuthenticate());
}

// Verifies case when pin can't be used to authenticate (`auth_locked` == True).
TEST_F(PinStorageCryptohomeUnitTest, AuthLockedTest) {
  SetAuthLockedPin(kDummyPin);

  ASSERT_FALSE(CanAuthenticate());
  ASSERT_TRUE(IsPinSet());
}

}  // namespace quick_unlock
}  // namespace chromeos
