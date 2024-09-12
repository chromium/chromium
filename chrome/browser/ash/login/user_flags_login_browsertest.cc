// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/test/bind.h"
#include "chrome/browser/about_flags.h"
#include "chrome/browser/ash/login/session/user_session_manager_test_api.h"
#include "chrome/browser/ash/login/test/login_manager_mixin.h"
#include "chrome/browser/ash/login/test/session_manager_state_waiter.h"
#include "chrome/browser/ash/settings/about_flags.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/ash/login/user_adding_screen.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"
#include "chromeos/ash/components/cryptohome/cryptohome_parameters.h"
#include "chromeos/ash/components/dbus/session_manager/fake_session_manager_client.h"
#include "chromeos/ash/components/login/auth/public/user_context.h"
#include "components/flags_ui/feature_entry_macros.h"
#include "components/flags_ui/flags_state.h"
#include "components/flags_ui/pref_service_flags_storage.h"
#include "components/user_manager/user_manager.h"
#include "content/public/test/browser_test.h"

namespace ash {

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
    set_exit_when_last_browser_closes(false);
    login_manager_.set_session_restore_enabled();
    login_manager_.AppendRegularUsers(2);
  }
  ~UserFlagsLoginTest() override = default;

  // MixinBasedInProcessBrowserTest:
  void SetUpOnMainThread() override {
    FakeSessionManagerClient::Get()->set_supports_browser_restart(true);
    MixinBasedInProcessBrowserTest::SetUpOnMainThread();
  }

 protected:
  ::about_flags::testing::ScopedFeatureEntries scoped_feature_entries_;
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
  SessionStateWaiter waiter;
  test::UserSessionManagerTestApi session_manager_test_api(
      UserSessionManager::GetInstance());
  session_manager_test_api.SetAttemptRestartClosureInTests(
      base::BindLambdaForTesting([&]() {
        // Signal |waiter| to exit its RunLoop.
        waiter.OnUserSessionStarted(true);
        restart_requested = true;
      }));
  login_manager_.set_should_wait_for_profile(false);
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

// Verifies that flags of a secondary user is not applied.
IN_PROC_BROWSER_TEST_F(UserFlagsLoginTest, FlagsNotAppliedForSecondary) {
  // Signs in the primary user.
  auto context =
      LoginManagerMixin::CreateDefaultUserContext(login_manager_.users()[0]);
  ASSERT_TRUE(login_manager_.LoginAndWaitForActiveSession(context));
  user_manager::UserManager* user_manager = user_manager::UserManager::Get();
  const user_manager::User* primary_user = user_manager->GetActiveUser();

  // Adds a secondary user.
  UserAddingScreen::Get()->Start();
  context =
      LoginManagerMixin::CreateDefaultUserContext(login_manager_.users()[1]);
  ASSERT_TRUE(login_manager_.LoginAndWaitForActiveSession(context));

  // Current active user should be the secondary user, which is different from
  // the primary user.
  const user_manager::User* secondary_user = user_manager->GetActiveUser();
  ASSERT_NE(primary_user, secondary_user);

  // Sets the flags for the secondary user.
  PrefService* secondary_user_prefs =
      ProfileManager::GetActiveUserProfile()->GetPrefs();
  flags_ui::PrefServiceFlagsStorage flags_storage(secondary_user_prefs);
  flags_storage.SetFlags({"feature-1", "feature-2"});
  flags_storage.SetOriginListFlag("feature-2", "http://example.com");
  flags_storage.CommitPendingWrites();

  // Update flags.
  about_flags::FeatureFlagsUpdate(flags_storage, secondary_user_prefs)
      .UpdateSessionManager();

  // Session manager client should not get flags for secondary user.
  EXPECT_FALSE(FakeSessionManagerClient::Get()->GetFlagsForUser(
      cryptohome::CreateAccountIdentifierFromAccountId(
          secondary_user->GetAccountId()),
      nullptr));
}

}  // namespace ash
