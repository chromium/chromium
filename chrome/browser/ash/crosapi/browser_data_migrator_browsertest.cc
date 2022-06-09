// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/browser_data_migrator.h"

#include <string>

#include "ash/components/login/auth/user_context.h"
#include "ash/constants/ash_features.h"
#include "ash/constants/ash_switches.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ash/crosapi/browser_util.h"
#include "chrome/browser/ash/crosapi/move_migrator.h"
#include "chrome/browser/ash/login/existing_user_controller.h"
#include "chrome/browser/ash/login/login_manager_test.h"
#include "chrome/browser/ash/login/test/local_state_mixin.h"
#include "chrome/browser/ash/login/test/login_manager_mixin.h"
#include "chrome/browser/browser_process.h"
#include "chromeos/dbus/session_manager/fake_session_manager_client.h"
#include "chromeos/dbus/session_manager/session_manager_client.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/policy_constants.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_launcher.h"

namespace ash {

namespace {

constexpr char kUserIdHash[] = "abcdefg";

// As defined in /ash/components/login/auth/stub_authenticator.cc
static const char kUserIdHashSuffix[] = "-hash";

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

  // ash::LoginManagerTest:
  void SetUp() override {
    login_manager_mixin_.AppendRegularUsers(1);

    ash::LoginManagerTest::SetUp();
  }

  bool LoginAsRegularUser() {
    ExistingUserController* controller =
        ExistingUserController::current_controller();
    if (!controller) {
      return false;
    }

    const auto& test_user_info = login_manager_mixin_.users()[0];

    const UserContext user_context =
        CreateUserContext(test_user_info.account_id, kPassword);
    SetExpectedCredentials(user_context);
    controller->Login(user_context, SigninSpecifics());
    return true;
  }

  void SetUpInProcessBrowserTestFixture() override {
    chromeos::SessionManagerClient::InitializeFakeInMemory();
  }

 protected:
  LoginManagerMixin login_manager_mixin_{&mixin_host_};
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
    feature_list_.InitWithFeatures(
        {ash::features::kLacrosSupport,
         ash::features::kLacrosProfileMigrationForAnyUser},
        {});

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
  LoginAsRegularUser();
  run_loop.Run();
  EXPECT_TRUE(chromeos::FakeSessionManagerClient::Get()
                  ->request_browser_data_migration_called());
  // Migration should be triggered in copy mode and not move mode.
  EXPECT_FALSE(chromeos::FakeSessionManagerClient::Get()
                   ->request_browser_data_migration_for_move_called());
};

class BrowserDataMigratorMoveMigrateOnSignInByPolicy
    : public BrowserDataMigratorOnSignIn {
 public:
  BrowserDataMigratorMoveMigrateOnSignInByPolicy() = default;
  BrowserDataMigratorMoveMigrateOnSignInByPolicy(
      BrowserDataMigratorMoveMigrateOnSignInByPolicy&) = delete;
  BrowserDataMigratorMoveMigrateOnSignInByPolicy& operator=(
      BrowserDataMigratorMoveMigrateOnSignInByPolicy&) = delete;
  ~BrowserDataMigratorMoveMigrateOnSignInByPolicy() override = default;

  void SetUp() override {
    feature_list_.InitWithFeatures(
        {ash::features::kLacrosProfileMigrationForAnyUser}, {});

    BrowserDataMigratorOnSignIn::SetUp();
  }

  void SetLacrosAvailability(
      crosapi::browser_util::LacrosAvailability lacros_availability) {
    policy::PolicyMap policy;
    policy.Set(
        policy::key::kLacrosAvailability, policy::POLICY_LEVEL_MANDATORY,
        policy::POLICY_SCOPE_USER, policy::POLICY_SOURCE_CLOUD,
        base::Value(GetLacrosAvailabilityPolicyName(lacros_availability)),
        /*external_data_fetcher=*/nullptr);
    crosapi::browser_util::CacheLacrosAvailability(policy);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// Enabling LacrosOnly by policy should trigger move migration during signin.
IN_PROC_BROWSER_TEST_F(BrowserDataMigratorMoveMigrateOnSignInByPolicy,
                       MigrateOnSignIn) {
  base::RunLoop run_loop;
  ScopedRestartAttemptForTesting scoped_restart_attempt(
      base::BindLambdaForTesting([&]() { run_loop.Quit(); }));
  SetLacrosAvailability(crosapi::browser_util::LacrosAvailability::kLacrosOnly);
  LoginAsRegularUser();
  run_loop.Run();
  EXPECT_TRUE(chromeos::FakeSessionManagerClient::Get()
                  ->request_browser_data_migration_called());
  EXPECT_TRUE(chromeos::FakeSessionManagerClient::Get()
                  ->request_browser_data_migration_for_move_called());
};

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
         ash::features::kLacrosOnly,
         ash::features::kLacrosProfileMigrationForAnyUser},
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
  LoginAsRegularUser();
  run_loop.Run();
  EXPECT_TRUE(chromeos::FakeSessionManagerClient::Get()
                  ->request_browser_data_migration_called());
  EXPECT_TRUE(chromeos::FakeSessionManagerClient::Get()
                  ->request_browser_data_migration_for_move_called());
};

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
        user.account_id.GetUserEmail() + kUserIdHashSuffix;

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
  LoginAsRegularUser();
  run_loop.Run();
  EXPECT_TRUE(chromeos::FakeSessionManagerClient::Get()
                  ->request_browser_data_migration_called());
  EXPECT_TRUE(chromeos::FakeSessionManagerClient::Get()
                  ->request_browser_data_migration_for_move_called());
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
    chromeos::SessionManagerClient::InitializeFakeInMemory();
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
         ash::features::kLacrosOnly,
         ash::features::kLacrosProfileMigrationForAnyUser},
        {});

    BrowserDataMigratorRestartInSession::SetUp();
  }
};

// Test that enabling LacrosOnly by feature flags triggers move migration during
// restart.
IN_PROC_BROWSER_TEST_F(
    BrowserDataMigratorMoveMigrateOnRestartInSessionByFeature,
    RunMoveMigration) {
  EXPECT_TRUE(chromeos::FakeSessionManagerClient::Get()
                  ->request_browser_data_migration_called());
  EXPECT_TRUE(chromeos::FakeSessionManagerClient::Get()
                  ->request_browser_data_migration_for_move_called());
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

  void SetUp() override {
    feature_list_.InitAndEnableFeature(
        ash::features::kLacrosProfileMigrationForAnyUser);

    BrowserDataMigratorRestartInSession::SetUp();
  }

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
  EXPECT_TRUE(chromeos::FakeSessionManagerClient::Get()
                  ->request_browser_data_migration_called());
  EXPECT_TRUE(chromeos::FakeSessionManagerClient::Get()
                  ->request_browser_data_migration_for_move_called());
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
  EXPECT_TRUE(chromeos::FakeSessionManagerClient::Get()
                  ->request_browser_data_migration_called());
  EXPECT_TRUE(chromeos::FakeSessionManagerClient::Get()
                  ->request_browser_data_migration_for_move_called());
}

}  // namespace ash
