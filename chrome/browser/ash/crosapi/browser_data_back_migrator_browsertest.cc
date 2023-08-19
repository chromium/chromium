// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/browser_data_back_migrator.h"

#include "ash/constants/ash_features.h"
#include "base/files/file_util.h"
#include "base/test/bind.h"
#include "base/test/test_future.h"
#include "chrome/browser/ash/app_mode/kiosk_app_manager.h"
#include "chrome/browser/ash/crosapi/browser_data_migrator.h"
#include "chrome/browser/ash/crosapi/browser_util.h"
#include "chrome/browser/ash/login/app_mode/test/kiosk_base_test.h"
#include "chrome/browser/ash/login/existing_user_controller.h"
#include "chrome/browser/ash/login/login_manager_test.h"
#include "chrome/browser/ash/login/test/login_manager_mixin.h"
#include "chrome/browser/ash/login/test/profile_prepared_waiter.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/browser_process.h"
#include "chromeos/ash/components/dbus/session_manager/fake_session_manager_client.h"
#include "chromeos/ash/components/login/auth/public/user_context.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/policy_constants.h"
#include "components/user_manager/fake_user_manager.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_launcher.h"

namespace ash {

namespace {

const char kUserEmail[] = "test_user@gmail.com";
const char kGaiaID[] = "22222";

bool CreateLacrosDirectoryForProfile(const AccountId& account_id) {
  base::FilePath user_data_dir;
  base::PathService::Get(chrome::DIR_USER_DATA, &user_data_dir);
  const base::FilePath profile_data_dir =
      ProfileHelper::GetProfilePathByUserIdHash(
          user_manager::FakeUserManager::GetFakeUsernameHash(account_id));
  const base::FilePath lacros_dir =
      profile_data_dir.Append(browser_data_migrator_util::kLacrosDir);
  {
    base::ScopedAllowBlockingForTesting scoped_allow_blocking;
    if (!(base::CreateDirectory(user_data_dir) &&
          base::CreateDirectory(profile_data_dir) &&
          base::CreateDirectory(lacros_dir))) {
      LOG(ERROR) << "Creating Lacros directory file failed.";
      return false;
    }
  }

  return true;
}

}  // namespace

// Used to test whether back migration gets triggered during the signin flow.
// Concretely it tests `MaybeRestartToMigrateBack()` called from
// `UserSessionManager::DoBrowserLaunchInternal()`.
class BrowserDataBackMigratorOnSignIn : public ash::LoginManagerTest {
 public:
  BrowserDataBackMigratorOnSignIn() = default;
  BrowserDataBackMigratorOnSignIn(BrowserDataBackMigratorOnSignIn&) = delete;
  BrowserDataBackMigratorOnSignIn& operator=(BrowserDataBackMigratorOnSignIn&) =
      delete;
  ~BrowserDataBackMigratorOnSignIn() override = default;

  const LoginManagerMixin::TestUserInfo regular_user_{
      AccountId::FromUserEmailGaiaId(kUserEmail, kGaiaID)};

  bool Login() {
    ExistingUserController* controller =
        ExistingUserController::current_controller();
    if (!controller) {
      return false;
    }

    const UserContext user_context =
        CreateUserContext(regular_user_.account_id, kPassword);
    SetExpectedCredentials(user_context);

    controller->Login(user_context, SigninSpecifics());
    return true;
  }

  void SetUpInProcessBrowserTestFixture() override {
    feature_list_.InitWithFeatures(
        {ash::features::kLacrosProfileBackwardMigration},
        {ash::features::kLacrosOnly});

    SessionManagerClient::InitializeFakeInMemory();
  }

 protected:
  LoginManagerMixin login_manager_mixin_{&mixin_host_, {regular_user_}};

 private:
  base::test::ScopedFeatureList feature_list_;
};

// Check that back migration is triggered from signin flow when the Lacros
// directory exists.
IN_PROC_BROWSER_TEST_F(BrowserDataBackMigratorOnSignIn, BackMigrateOnSignIn) {
  CreateLacrosDirectoryForProfile(regular_user_.account_id);

  base::test::TestFuture<void> waiter;
  ScopedBackMigratorRestartAttemptForTesting
      scoped_back_migrator_restart_attempt(
          base::BindLambdaForTesting([&]() { waiter.SetValue(); }));
  ASSERT_TRUE(Login());
  EXPECT_TRUE(waiter.Wait());
  EXPECT_TRUE(FakeSessionManagerClient::Get()
                  ->request_browser_data_backward_migration_called());
}

// Check that back migration is not triggered from signin flow when the Lacros
// directory does not exist.
IN_PROC_BROWSER_TEST_F(BrowserDataBackMigratorOnSignIn,
                       BackMigrateNoLacrosDir) {
  ash::test::ProfilePreparedWaiter profile_prepared(regular_user_.account_id);
  ASSERT_TRUE(Login());
  // When there is no Lacros dir we cannot wait for the run loop to quit because
  // BrowserDataBackMigrator::AttemptRestart is never called.
  // Note that `ProfilePreparedWaiter` waits for
  // `ExistingUserController::OnProfilePrepared()` to be called and this is
  // called after `UserSessionManager::InitializeUserSession()` is called, which
  // leads to `BrowserDataBackMigrator::MaybeRestartToMigrateBack()`. Therefore
  // by the time the wait ends, back migration check would have happened.
  profile_prepared.Wait();
  EXPECT_FALSE(FakeSessionManagerClient::Get()
                   ->request_browser_data_backward_migration_called());
}

class BrowserDataBackMigratorForKiosk : public KioskBaseTest {
 public:
  BrowserDataBackMigratorForKiosk() = default;
  BrowserDataBackMigratorForKiosk(BrowserDataBackMigratorForKiosk&) = delete;
  BrowserDataBackMigratorForKiosk& operator=(BrowserDataBackMigratorForKiosk&) =
      delete;
  ~BrowserDataBackMigratorForKiosk() override = default;

  void SetUp() override {
    feature_list_.InitWithFeatures(
        {ash::features::kLacrosProfileBackwardMigration}, {});

    KioskBaseTest::SetUp();
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(BrowserDataBackMigratorForKiosk, MigrateOnKioskLaunch) {
  // Register the test app with `KioskAppManager` so that the `AccountId` can be
  // retrieved.
  PrepareAppLaunch();
  KioskAppManager::App app;
  CHECK(KioskAppManager::Get());
  CHECK(KioskAppManager::Get()->GetApp(test_app_id(), &app));
  CreateLacrosDirectoryForProfile(app.account_id);

  base::test::TestFuture<void> waiter;
  ScopedBackMigratorRestartAttemptForTesting
      scoped_back_migrator_restart_attempt(
          base::BindLambdaForTesting([&]() { waiter.SetValue(); }));
  StartAppLaunchFromLoginScreen(
      NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_ONLINE);
  EXPECT_TRUE(waiter.Wait());
  EXPECT_TRUE(FakeSessionManagerClient::Get()
                  ->request_browser_data_backward_migration_called());
}

}  // namespace ash
