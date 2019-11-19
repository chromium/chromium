// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/bind_test_util.h"
#include "chrome/browser/chromeos/login/login_manager_test.h"
#include "chrome/browser/chromeos/login/quick_unlock/pin_backend.h"
#include "chrome/browser/chromeos/login/quick_unlock/pin_storage_prefs.h"
#include "chrome/browser/chromeos/login/quick_unlock/quick_unlock_factory.h"
#include "chrome/browser/chromeos/login/quick_unlock/quick_unlock_storage.h"
#include "chrome/browser/chromeos/login/startup_utils.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chromeos/dbus/cryptohome/fake_cryptohome_client.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "components/account_id/account_id.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {
namespace quick_unlock {
namespace {

constexpr char kTestUserEmail[] = "test-user@gmail.com";
constexpr char kTestUserGaiaId[] = "1234567890";

class PinMigrationTest : public LoginManagerTest {
 public:
  PinMigrationTest()
      : LoginManagerTest(false /*should_launch_browser*/,
                         true /* should_initialize_webui */) {}
  ~PinMigrationTest() override = default;

  void SetUp() override {
    // Initialize CryptohomeClient and configure it for testing. It will be
    // destroyed in ChromeBrowserMain.
    CryptohomeClient::InitializeFake();
    FakeCryptohomeClient::Get()->set_supports_low_entropy_credentials(true);

    LoginManagerTest::SetUp();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(PinMigrationTest);
};

// Step 1/3: Register a new user.
IN_PROC_BROWSER_TEST_F(PinMigrationTest, PRE_PRE_Migrate) {
  RegisterUser(AccountId::FromUserEmailGaiaId(kTestUserEmail, kTestUserGaiaId));
  StartupUtils::MarkOobeCompleted();
}

// Step 2/3: Log the user in, add a prefs-based PIN.
IN_PROC_BROWSER_TEST_F(PinMigrationTest, PRE_Migrate) {
  AccountId test_account =
      AccountId::FromUserEmailGaiaId(kTestUserEmail, kTestUserGaiaId);
  LoginUser(test_account);

  // Register PIN.
  QuickUnlockStorage* storage =
      QuickUnlockFactory::GetForAccountId(test_account);
  ASSERT_TRUE(!!storage);
  storage->pin_storage_prefs()->SetPin("111111");

  // Validate PIN is set.
  base::RunLoop run_loop;
  base::Optional<bool> has_pin_result;
  PinBackend::GetInstance()->IsSet(
      test_account, base::BindLambdaForTesting([&](bool has_pin) {
        has_pin_result = has_pin;
        run_loop.Quit();
      }));
  run_loop.Run();
  ASSERT_TRUE(has_pin_result.has_value());
  EXPECT_TRUE(*has_pin_result);
}

// Step 3/3: Log in again, verify prefs-based PIN does not contain state, but
// PIN is still set.
IN_PROC_BROWSER_TEST_F(PinMigrationTest, Migrate) {
  AccountId test_account =
      AccountId::FromUserEmailGaiaId(kTestUserEmail, kTestUserGaiaId);
  LoginUser(test_account);

  // Prefs PIN is not set.
  EXPECT_FALSE(QuickUnlockFactory::GetForAccountId(test_account)
                   ->pin_storage_prefs()
                   ->IsPinSet());

  // Since prefs-based PIN is not set, calling IsSet on PinBackend will only
  // return true if the PIN is set in cryptohome.
  base::RunLoop run_loop;
  base::Optional<bool> has_pin_result;
  PinBackend::GetInstance()->IsSet(
      test_account, base::BindLambdaForTesting([&](bool has_pin) {
        has_pin_result = has_pin;
        run_loop.Quit();
      }));
  run_loop.Run();
  ASSERT_TRUE(has_pin_result.has_value());
  EXPECT_TRUE(*has_pin_result);
}

}  // namespace
}  // namespace quick_unlock
}  // namespace chromeos
