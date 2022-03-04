// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/browser_data_migrator.h"

#include <string>

#include "ash/components/login/auth/user_context.h"
#include "ash/constants/ash_switches.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ash/crosapi/move_migrator.h"
#include "chrome/browser/ash/login/existing_user_controller.h"
#include "chrome/browser/ash/login/login_manager_test.h"
#include "chrome/browser/ash/login/test/local_state_mixin.h"
#include "chrome/browser/ash/login/test/login_manager_mixin.h"
#include "chrome/browser/browser_process.h"
#include "chromeos/dbus/session_manager/fake_session_manager_client.h"
#include "chromeos/dbus/session_manager/session_manager_client.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_launcher.h"

namespace ash {

namespace {

constexpr char kUserIdHash[] = "abcdefg";

// As defined in /ash/components/login/auth/stub_authenticator.cc
static const char kUserIdHashSuffix[] = "-hash";

}  // namespace

class BrowserDataMigratorResumeOnSignInTest : public ash::LoginManagerTest,
                                              public LocalStateMixin::Delegate {
 public:
  BrowserDataMigratorResumeOnSignInTest() = default;
  BrowserDataMigratorResumeOnSignInTest(
      BrowserDataMigratorResumeOnSignInTest&) = delete;
  BrowserDataMigratorResumeOnSignInTest& operator=(
      BrowserDataMigratorResumeOnSignInTest&) = delete;
  ~BrowserDataMigratorResumeOnSignInTest() override = default;

  // ash::LoginManagerTest:
  void SetUp() override {
    login_manager_mixin_.AppendRegularUsers(1);

    ash::LoginManagerTest::SetUp();
  }

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

 private:
  LoginManagerMixin login_manager_mixin_{&mixin_host_};
  LocalStateMixin local_state_mixin_{&mixin_host_, this};
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(BrowserDataMigratorResumeOnSignInTest,
                       ForceResumeOnLogin) {
  // Test `MaybeForceResumeMoveMigration()` in
  // `ExistingUserController::ContinueAuthSuccessAfterResumeAttempt()`.
  base::RunLoop run_loop;
  ScopedRestartAttemptForTesting scoped_restart_attempt(
      base::BindLambdaForTesting([&]() { run_loop.Quit(); }));
  LoginAsRegularUser();
  run_loop.Run();
  EXPECT_TRUE(chromeos::FakeSessionManagerClient::Get()
                  ->request_browser_data_migration_called());
}

class BrowserDataMigratorResumeRestartInSession
    : public MixinBasedInProcessBrowserTest,
      public LocalStateMixin::Delegate {
 public:
  BrowserDataMigratorResumeRestartInSession()
      : scoped_attempt_restart_(
            std::make_unique<ScopedRestartAttemptForTesting>(
                base::DoNothing())) {}

  BrowserDataMigratorResumeRestartInSession(
      BrowserDataMigratorResumeRestartInSession&) = delete;
  BrowserDataMigratorResumeRestartInSession& operator=(
      BrowserDataMigratorResumeRestartInSession&) = delete;
  ~BrowserDataMigratorResumeRestartInSession() override = default;

  void SetUp() override { MixinBasedInProcessBrowserTest::SetUp(); }

  // LocalStateMixin::Delegate
  void SetUpLocalState() override {
    // Setting this pref triggers a restart to resume move migration. Check
    // `BrowserDataMigratorImpl::MaybeForceResumeMoveMigration()`.
    MoveMigrator::SetResumeStep(g_browser_process->local_state(), kUserIdHash,
                                MoveMigrator::ResumeStep::kMoveLacrosItems);
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

 private:
  std::unique_ptr<ScopedRestartAttemptForTesting> scoped_attempt_restart_;
  LocalStateMixin local_state_mixin_{&mixin_host_, this};
};

IN_PROC_BROWSER_TEST_F(BrowserDataMigratorResumeRestartInSession,
                       ResumeMigration) {
  // Test `MaybeForceResumeMoveMigration()` in
  // `ChromeBrowserMainPartsAsh::PreProfileInit()`.

  // Note that by the time the body of the test is called,
  // `ChromeBrowserMainPartsAsh::PreProfileInit()` would have been called. Thus
  // there is no need for a waiter.
  EXPECT_TRUE(chromeos::FakeSessionManagerClient::Get()
                  ->request_browser_data_migration_called());
}

}  // namespace ash
