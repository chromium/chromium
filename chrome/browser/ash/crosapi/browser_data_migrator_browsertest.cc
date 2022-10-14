// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/browser_data_migrator.h"

#include <string>

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_switches.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ash/app_mode/kiosk_app_manager.h"
#include "chrome/browser/ash/crosapi/browser_util.h"
#include "chrome/browser/ash/crosapi/move_migrator.h"
#include "chrome/browser/ash/login/app_mode/test/kiosk_base_test.h"
#include "chrome/browser/ash/login/existing_user_controller.h"
#include "chrome/browser/ash/login/login_manager_test.h"
#include "chrome/browser/ash/login/test/local_state_mixin.h"
#include "chrome/browser/ash/login/test/login_manager_mixin.h"
#include "chrome/browser/ash/login/test/profile_prepared_waiter.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/browser_process.h"
#include "chromeos/ash/components/dbus/session_manager/fake_session_manager_client.h"
#include "chromeos/ash/components/dbus/session_manager/session_manager_client.h"
#include "chromeos/ash/components/login/auth/public/user_context.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/policy_constants.h"
#include "components/user_manager/fake_user_manager.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_launcher.h"

namespace ash {

namespace {

const char kUserEmail[] = "test_user@gmail.com";
const char kGaiaID[] = "22222";

constexpr char kUserIdHash[] = "abcdefg";

// This creates <profile directory>/Preferences file for the account so that
// when `Profile` instance is created, it is considered a profile for an
// existing user. This is to avoid profile migration being marked as completed
// for a new user.
bool CreatePreferenceFileForProfile(const AccountId& account_id) {
  base::FilePath user_data_dir;
  base::PathService::Get(chrome::DIR_USER_DATA, &user_data_dir);
  const base::FilePath profile_data_dir =
      ProfileHelper::GetProfilePathByUserIdHash(
          user_manager::FakeUserManager::GetFakeUsernameHash(account_id));
  {
    base::ScopedAllowBlockingForTesting scoped_allow_blocking;
    if (!(base::CreateDirectory(user_data_dir) &&
          base::CreateDirectory(profile_data_dir) &&
          base::WriteFile(profile_data_dir.Append("Preferences"), "{}"))) {
      LOG(ERROR) << "Creating `Preferences` file failed.";
      return false;
    }
  }

  return true;
}

void SetLacrosAvailability(
    crosapi::browser_util::LacrosAvailability lacros_availability) {
  policy::PolicyMap policy;
  policy.Set(policy::key::kLacrosAvailability, policy::POLICY_LEVEL_MANDATORY,
             policy::POLICY_SCOPE_USER, policy::POLICY_SOURCE_CLOUD,
             base::Value(GetLacrosAvailabilityPolicyName(lacros_availability)),
             /*external_data_fetcher=*/nullptr);
  crosapi::browser_util::CacheLacrosAvailability(policy);
}

}  // namespace

// Used to test whether migration gets triggered during the signin flow.
// Concretely it tests `MaybeRestartToMigrate()` called from
// `UserSessionManager::DoBrowserLaunchInternal()` and
// `MaybeForceResumeMoveMigration()` called from
// `ExistingUserController::ContinueAuthSuccessAfterResumeAttempt()`.
class BrowserDataMigratorOnSignIn : public ash::LoginManagerTest {
 public:
  BrowserDataMigratorOnSignIn() = default;
  BrowserDataMigratorOnSignIn(BrowserDataMigratorOnSignIn&) = delete;
  BrowserDataMigratorOnSignIn& operator=(BrowserDataMigratorOnSignIn&) = delete;
  ~BrowserDataMigratorOnSignIn() override = default;

  const LoginManagerMixin::TestUserInfo regular_user_{
      AccountId::FromUserEmailGaiaId(kUserEmail, kGaiaID)};

  bool LoginAsExistingRegularUser() {
    return CreatePreferenceFileForProfile(regular_user_.account_id) &&
           LoginAsRegularUser();
  }

  bool LoginAsRegularUser() {
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
    SessionManagerClient::InitializeFakeInMemory();
  }

 protected:
  LoginManagerMixin login_manager_mixin_{&mixin_host_, {regular_user_}};
};

class BrowserDataMigratorCopyMigrateOnSignIn
    : public BrowserDataMigratorOnSignIn {
 public:
  BrowserDataMigratorCopyMigrateOnSignIn() = default;
  BrowserDataMigratorCopyMigrateOnSignIn(
      BrowserDataMigratorCopyMigrateOnSignIn&) = delete;
  BrowserDataMigratorCopyMigrateOnSignIn& operator=(
      BrowserDataMigratorCopyMigrateOnSignIn&) = delete;
  ~BrowserDataMigratorCopyMigrateOnSignIn() override = default;

  void SetUp() override {
    feature_list_.InitWithFeatures({ash::features::kLacrosSupport}, {});

    BrowserDataMigratorOnSignIn::SetUp();
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// Check that migration is triggered from signin flow if Lacros is enabled.
IN_PROC_BROWSER_TEST_F(BrowserDataMigratorCopyMigrateOnSignIn,
                       MigrateOnSignIn) {
  base::RunLoop run_loop;
  ScopedRestartAttemptForTesting scoped_restart_attempt(
      base::BindLambdaForTesting([&]() { run_loop.Quit(); }));
  ASSERT_TRUE(LoginAsExistingRegularUser());
  run_loop.Run();
  EXPECT_TRUE(
      FakeSessionManagerClient::Get()->request_browser_data_migration_called());
  // Migration should be triggered in copy mode and not move mode.
  EXPECT_TRUE(FakeSessionManagerClient::Get()
                  ->request_browser_data_migration_mode_called());
  EXPECT_EQ(FakeSessionManagerClient::Get()
                ->request_browser_data_migration_mode_value(),
            "copy");
}

// Check that migration marked as completed for a new user and thus migration is
// not triggered from signin flow.
IN_PROC_BROWSER_TEST_F(BrowserDataMigratorCopyMigrateOnSignIn,
                       SkipMigrateOnSignInForNewUser) {
  ash::test::ProfilePreparedWaiter profile_prepared(regular_user_.account_id);
  ASSERT_TRUE(LoginAsRegularUser());
  // Note that `ProfilePreparedWaiter` waits for
  // `ExistingUserController::OnProfilePrepared()` to be called and this is
  // called after `UserSessionManager::InitializeUserSession()` is called, which
  // leads to `BrowserDataMigratorImpl::MaybeRestartToMigrate()`. Therefore by
  // the time the wait ends, migration check would have happened.
  profile_prepared.Wait();
  EXPECT_FALSE(
      FakeSessionManagerClient::Get()->request_browser_data_migration_called());
  const std::string user_id_hash =
      user_manager::FakeUserManager::GetFakeUsernameHash(
          regular_user_.account_id);
  EXPECT_TRUE(
      crosapi::browser_util::IsCopyOrMoveProfileMigrationCompletedForUser(
          g_browser_process->local_state(), user_id_hash));
}

class BrowserDataMigratorMoveMigrateOnSignInByPolicy
    : public BrowserDataMigratorOnSignIn {
 public:
  BrowserDataMigratorMoveMigrateOnSignInByPolicy() = default;
  BrowserDataMigratorMoveMigrateOnSignInByPolicy(
      BrowserDataMigratorMoveMigrateOnSignInByPolicy&) = delete;
  BrowserDataMigratorMoveMigrateOnSignInByPolicy& operator=(
      BrowserDataMigratorMoveMigrateOnSignInByPolicy&) = delete;
  ~BrowserDataMigratorMoveMigrateOnSignInByPolicy() override = default;
};

// Enabling LacrosOnly by policy should trigger move migration during signin.
IN_PROC_BROWSER_TEST_F(BrowserDataMigratorMoveMigrateOnSignInByPolicy,
                       MigrateOnSignIn) {
  base::RunLoop run_loop;
  ScopedRestartAttemptForTesting scoped_restart_attempt(
      base::BindLambdaForTesting([&]() { run_loop.Quit(); }));
  SetLacrosAvailability(crosapi::browser_util::LacrosAvailability::kLacrosOnly);
  ASSERT_TRUE(LoginAsExistingRegularUser());
  run_loop.Run();
  EXPECT_TRUE(
      FakeSessionManagerClient::Get()->request_browser_data_migration_called());
  EXPECT_TRUE(FakeSessionManagerClient::Get()
                  ->request_browser_data_migration_mode_called());
  EXPECT_EQ(FakeSessionManagerClient::Get()
                ->request_browser_data_migration_mode_value(),
            "move");
}

class BrowserDataMigratorMoveMigrateOnSignInByFeature
    : public BrowserDataMigratorOnSignIn {
 public:
  BrowserDataMigratorMoveMigrateOnSignInByFeature() = default;
  BrowserDataMigratorMoveMigrateOnSignInByFeature(
      BrowserDataMigratorMoveMigrateOnSignInByFeature&) = delete;
  BrowserDataMigratorMoveMigrateOnSignInByFeature& operator=(
      BrowserDataMigratorMoveMigrateOnSignInByFeature&) = delete;
  ~BrowserDataMigratorMoveMigrateOnSignInByFeature() override = default;

  void SetUp() override {
    feature_list_.InitWithFeatures(
        {ash::features::kLacrosSupport, ash::features::kLacrosPrimary,
         ash::features::kLacrosOnly},
        {});

    BrowserDataMigratorOnSignIn::SetUp();
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// Enabling LacrosOnly with feature flags should trigger move migration during
// signin.
IN_PROC_BROWSER_TEST_F(BrowserDataMigratorMoveMigrateOnSignInByFeature,
                       MigrateOnSignIn) {
  base::RunLoop run_loop;
  ScopedRestartAttemptForTesting scoped_restart_attempt(
      base::BindLambdaForTesting([&]() { run_loop.Quit(); }));
  ASSERT_TRUE(LoginAsExistingRegularUser());
  run_loop.Run();
  EXPECT_TRUE(
      FakeSessionManagerClient::Get()->request_browser_data_migration_called());
  EXPECT_TRUE(FakeSessionManagerClient::Get()
                  ->request_browser_data_migration_mode_called());
  EXPECT_EQ(FakeSessionManagerClient::Get()
                ->request_browser_data_migration_mode_value(),
            "move");
}

class BrowserDataMigratorResumeOnSignIn : public BrowserDataMigratorOnSignIn,
                                          public LocalStateMixin::Delegate {
 public:
  BrowserDataMigratorResumeOnSignIn() = default;
  BrowserDataMigratorResumeOnSignIn(BrowserDataMigratorResumeOnSignIn&) =
      delete;
  BrowserDataMigratorResumeOnSignIn& operator=(
      BrowserDataMigratorResumeOnSignIn&) = delete;
  ~BrowserDataMigratorResumeOnSignIn() override = default;

  // LocalStateMixin::Delegate:
  void SetUpLocalState() override {
    const auto& user = login_manager_mixin_.users()[0];

    const std::string user_id_hash =
        user_manager::FakeUserManager::GetFakeUsernameHash(user.account_id);

    // Setting this pref triggers a restart to resume move migration. Check
    // `BrowserDataMigratorImpl::MaybeForceResumeMoveMigration()`.
    MoveMigrator::SetResumeStep(g_browser_process->local_state(), user_id_hash,
                                MoveMigrator::ResumeStep::kMoveLacrosItems);
  }

 private:
  LocalStateMixin local_state_mixin_{&mixin_host_, this};
};

IN_PROC_BROWSER_TEST_F(BrowserDataMigratorResumeOnSignIn, ForceResumeOnLogin) {
  // Test `MaybeForceResumeMoveMigration()` in
  // `ExistingUserController::ContinueAuthSuccessAfterResumeAttempt()`.
  base::RunLoop run_loop;
  ScopedRestartAttemptForTesting scoped_restart_attempt(
      base::BindLambdaForTesting([&]() { run_loop.Quit(); }));
  ASSERT_TRUE(LoginAsExistingRegularUser());
  run_loop.Run();
  EXPECT_TRUE(
      FakeSessionManagerClient::Get()->request_browser_data_migration_called());
  EXPECT_TRUE(FakeSessionManagerClient::Get()
                  ->request_browser_data_migration_mode_called());
  EXPECT_EQ(FakeSessionManagerClient::Get()
                ->request_browser_data_migration_mode_value(),
            "move");
}

// Used to test whether migration gets triggered upon restart in session.
// Concretely it tests `MaybeRestartToMigrate()` or
// `MaybeForceResumeMoveMigration()` called from
// `ChromeBrowserMainPartsAsh::PreProfileInit()`. Since `PreProfileInit()` gets
// called before the body of tests are run, all the setups have to happen in
// early setup stages like `SetUp()` or `SetUpCommandLine()`.
class BrowserDataMigratorRestartInSession
    : public MixinBasedInProcessBrowserTest,
      public LocalStateMixin::Delegate {
 public:
  BrowserDataMigratorRestartInSession()
      : scoped_attempt_restart_(
            std::make_unique<ScopedRestartAttemptForTesting>(
                base::DoNothing())) {}
  BrowserDataMigratorRestartInSession(BrowserDataMigratorRestartInSession&) =
      delete;
  BrowserDataMigratorRestartInSession& operator=(
      BrowserDataMigratorRestartInSession&) = delete;
  ~BrowserDataMigratorRestartInSession() override = default;

  // LocalStateMixin::Delegate
  void SetUpLocalState() override {
    // Add `kUserIdHash`@gmail.com to kRegularUsersPref so that
    // `UserManager::FindUser()` is able to find this user in
    // `BrowserDataMigrator::MaybeRestartToMigrate()` and
    // `BrowserDataMigrator::RestartToMigrate()`.
    base::Value::List users;
    users.Append(base::Value(std::string(kUserIdHash) + "@gmail.com"));
    g_browser_process->local_state()->SetList(user_manager::kRegularUsersPref,
                                              std::move(users));
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    // By setting these flags, Ash is launched as if it's restarted inside a
    // user session.
    command_line->AppendSwitchASCII(switches::kLoginUser, kUserIdHash);
    command_line->AppendSwitchASCII(switches::kLoginProfile, kUserIdHash);

    MixinBasedInProcessBrowserTest::SetUpCommandLine(command_line);
  }

  void SetUpInProcessBrowserTestFixture() override {
    SessionManagerClient::InitializeFakeInMemory();
  }

 protected:
  // Since triggering migration means calling `chrome::AttemptRestart()`, it has
  // to be substituted.
  std::unique_ptr<ScopedRestartAttemptForTesting> scoped_attempt_restart_;
  LocalStateMixin local_state_mixin_{&mixin_host_, this};
  base::test::ScopedFeatureList feature_list_;
};

class BrowserDataMigratorMoveMigrateOnRestartInSessionByFeature
    : public BrowserDataMigratorRestartInSession {
 public:
  BrowserDataMigratorMoveMigrateOnRestartInSessionByFeature() = default;
  BrowserDataMigratorMoveMigrateOnRestartInSessionByFeature(
      BrowserDataMigratorMoveMigrateOnRestartInSessionByFeature&) = delete;
  BrowserDataMigratorMoveMigrateOnRestartInSessionByFeature& operator=(
      BrowserDataMigratorMoveMigrateOnRestartInSessionByFeature&) = delete;
  ~BrowserDataMigratorMoveMigrateOnRestartInSessionByFeature() override =
      default;

  void SetUp() override {
    feature_list_.InitWithFeatures(
        {ash::features::kLacrosSupport, ash::features::kLacrosPrimary,
         ash::features::kLacrosOnly},
        {});

    BrowserDataMigratorRestartInSession::SetUp();
  }
};

// Test that enabling LacrosOnly by feature flags triggers move migration during
// restart.
IN_PROC_BROWSER_TEST_F(
    BrowserDataMigratorMoveMigrateOnRestartInSessionByFeature,
    RunMoveMigration) {
  EXPECT_TRUE(
      FakeSessionManagerClient::Get()->request_browser_data_migration_called());
  EXPECT_TRUE(FakeSessionManagerClient::Get()
                  ->request_browser_data_migration_mode_called());
  EXPECT_EQ(FakeSessionManagerClient::Get()
                ->request_browser_data_migration_mode_value(),
            "move");
}

class BrowserDataMigratorMoveMigrateOnRestartInSessionByPolicy
    : public BrowserDataMigratorRestartInSession {
 public:
  BrowserDataMigratorMoveMigrateOnRestartInSessionByPolicy() = default;
  BrowserDataMigratorMoveMigrateOnRestartInSessionByPolicy(
      BrowserDataMigratorMoveMigrateOnRestartInSessionByPolicy&) = delete;
  BrowserDataMigratorMoveMigrateOnRestartInSessionByPolicy& operator=(
      BrowserDataMigratorMoveMigrateOnRestartInSessionByPolicy&) = delete;
  ~BrowserDataMigratorMoveMigrateOnRestartInSessionByPolicy() override =
      default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitchASCII(
        crosapi::browser_util::kLacrosAvailabilityPolicySwitch,
        crosapi::browser_util::kLacrosAvailabilityPolicyLacrosOnly);
    BrowserDataMigratorRestartInSession::SetUpCommandLine(command_line);
  }
};

// Test that enabling LacrosOnly by policy triggers move migration during
// restart.
IN_PROC_BROWSER_TEST_F(BrowserDataMigratorMoveMigrateOnRestartInSessionByPolicy,
                       RunMoveMigration) {
  EXPECT_TRUE(
      FakeSessionManagerClient::Get()->request_browser_data_migration_called());
  EXPECT_TRUE(FakeSessionManagerClient::Get()
                  ->request_browser_data_migration_mode_called());
  EXPECT_EQ(FakeSessionManagerClient::Get()
                ->request_browser_data_migration_mode_value(),
            "move");
}

class BrowserDataMigratorResumeRestartInSession
    : public BrowserDataMigratorRestartInSession {
 public:
  BrowserDataMigratorResumeRestartInSession() = default;
  BrowserDataMigratorResumeRestartInSession(
      BrowserDataMigratorResumeRestartInSession&) = delete;
  BrowserDataMigratorResumeRestartInSession& operator=(
      BrowserDataMigratorResumeRestartInSession&) = delete;
  ~BrowserDataMigratorResumeRestartInSession() override = default;

  // LocalStateMixin::Delegate
  void SetUpLocalState() override {
    // Setting this pref triggers a restart to resume move migration. Check
    // `BrowserDataMigratorImpl::MaybeForceResumeMoveMigration()`.
    MoveMigrator::SetResumeStep(g_browser_process->local_state(), kUserIdHash,
                                MoveMigrator::ResumeStep::kMoveLacrosItems);

    BrowserDataMigratorRestartInSession::SetUpLocalState();
  }
};

IN_PROC_BROWSER_TEST_F(BrowserDataMigratorResumeRestartInSession,
                       ResumeMigration) {
  // Test `MaybeForceResumeMoveMigration()` in
  // `ChromeBrowserMainPartsAsh::PreProfileInit()`.
  EXPECT_TRUE(
      FakeSessionManagerClient::Get()->request_browser_data_migration_called());
  EXPECT_TRUE(FakeSessionManagerClient::Get()
                  ->request_browser_data_migration_mode_called());
  EXPECT_EQ(FakeSessionManagerClient::Get()
                ->request_browser_data_migration_mode_value(),
            "move");
}

class BrowserDataMigratorForKiosk : public KioskBaseTest {
 public:
  BrowserDataMigratorForKiosk() = default;
  BrowserDataMigratorForKiosk(BrowserDataMigratorForKiosk&) = delete;
  BrowserDataMigratorForKiosk& operator=(BrowserDataMigratorForKiosk&) = delete;
  ~BrowserDataMigratorForKiosk() override = default;

  void SetUp() override {
    feature_list_.InitWithFeatures({ash::features::kLacrosSupport}, {});

    KioskBaseTest::SetUp();
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(BrowserDataMigratorForKiosk, MigrateOnKioskLaunch) {
  SetLacrosAvailability(crosapi::browser_util::LacrosAvailability::kUserChoice);

  // Call this so that the test app is registered with `KioskAppManager` and
  // thus the `AccountId` can be retrieved.
  PrepareAppLaunch();
  KioskAppManager::App app;
  CHECK(KioskAppManager::Get());
  CHECK(KioskAppManager::Get()->GetApp(test_app_id(), &app));
  CreatePreferenceFileForProfile(app.account_id);

  base::RunLoop run_loop;
  ScopedRestartAttemptForTesting scoped_restart_attempt(
      base::BindLambdaForTesting([&]() { run_loop.Quit(); }));
  StartAppLaunchFromLoginScreen(
      NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_ONLINE);
  run_loop.Run();
  EXPECT_TRUE(
      FakeSessionManagerClient::Get()->request_browser_data_migration_called());
}

}  // namespace ash
