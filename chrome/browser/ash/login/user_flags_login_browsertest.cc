// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/bind.h"
#include "chrome/browser/about_flags.h"
#include "chrome/browser/ash/login/session/user_session_manager_test_api.h"
#include "chrome/browser/ash/login/test/login_manager_mixin.h"
#include "chrome/browser/ash/login/test/session_manager_state_waiter.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"
#include "chromeos/dbus/session_manager/fake_session_manager_client.h"
#include "chromeos/login/auth/user_context.h"
#include "components/flags_ui/feature_entry_macros.h"
#include "components/flags_ui/flags_state.h"
#include "components/flags_ui/pref_service_flags_storage.h"
#include "components/user_manager/user_manager.h"
#include "content/public/test/browser_test.h"

namespace chromeos {

// Checks whether per-user feature flags get correctly applied at session start
// by restarting the browser.
class UserFlagsLoginTest : public MixinBasedInProcessBrowserTest {
 public:
  UserFlagsLoginTest()
      : scoped_feature_entries_({
            {"feature-1", "name-1", "description-1", flags_ui::kOsCrOS,
             SINGLE_VALUE_TYPE("switch-1")},
            {"feature-2", "name-2", "description-2", flags_ui::kOsCrOS,
             ORIGIN_LIST_VALUE_TYPE("switch-2", "")},
        }) {
    login_manager_.set_session_restore_enabled();
    login_manager_.AppendRegularUsers(1);
  }
  ~UserFlagsLoginTest() override = default;

  void SetUpOnMainThread() override {
    FakeSessionManagerClient::Get()->set_supports_browser_restart(true);
    MixinBasedInProcessBrowserTest::SetUpOnMainThread();
  }

 protected:
  about_flags::testing::ScopedFeatureEntries scoped_feature_entries_;
  LoginManagerMixin login_manager_{&mixin_host_, {}};
};

// Start a session and set up flag configuration.
IN_PROC_BROWSER_TEST_F(UserFlagsLoginTest, PRE_PRE_RestartToApplyFlags) {
  auto context =
      LoginManagerMixin::CreateDefaultUserContext(login_manager_.users()[0]);
  ASSERT_TRUE(login_manager_.LoginAndWaitForActiveSession(context));

  flags_ui::PrefServiceFlagsStorage flags_storage(
      ProfileManager::GetActiveUserProfile()->GetPrefs());
  flags_storage.SetFlags({"feature-1", "feature-2"});
  flags_storage.SetOriginListFlag("feature-2", "http://example.com");
  flags_storage.CommitPendingWrites();

  // Mark the session as stopped so PRE_RestartToApplyFlags starts at the login
  // screen.
  FakeSessionManagerClient::Get()->StopSession(
      login_manager::SessionStopReason::BROWSER_SHUTDOWN);
}

// Triggers a login. A restart should be performed to apply flags.
IN_PROC_BROWSER_TEST_F(UserFlagsLoginTest, PRE_RestartToApplyFlags) {
  bool restart_requested = false;
  ash::SessionStateWaiter waiter;
  ash::test::UserSessionManagerTestApi session_manager_test_api(
      ash::UserSessionManager::GetInstance());
  session_manager_test_api.SetAttemptRestartClosureInTests(
      base::BindLambdaForTesting([&]() {
        // Signal |waiter| to exit its RunLoop.
        waiter.OnUserSessionStarted(true);
        restart_requested = true;
      }));

  login_manager_.LoginWithDefaultContext(login_manager_.users()[0]);
  waiter.Wait();
  EXPECT_TRUE(restart_requested);
}

// Verifies that the flag configuration gets applied after restart.
IN_PROC_BROWSER_TEST_F(UserFlagsLoginTest, RestartToApplyFlags) {
  login_manager_.WaitForActiveSession();

  EXPECT_TRUE(base::CommandLine::ForCurrentProcess()->HasSwitch("switch-1"));
  EXPECT_EQ(
      "http://example.com",
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII("switch-2"));
}

}  // namespace chromeos
