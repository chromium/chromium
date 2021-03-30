// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>

#include "ash/constants/ash_switches.h"
#include "ash/public/cpp/login_screen_test_api.h"
#include "base/run_loop.h"
#include "base/timer/timer.h"
#include "chrome/browser/ash/arc/arc_util.h"
#include "chrome/browser/ash/arc/session/arc_service_launcher.h"
#include "chrome/browser/ash/arc/session/arc_session_manager.h"
#include "chrome/browser/ash/arc/test/test_arc_session_manager.h"
#include "chrome/browser/ash/login/test/js_checker.h"
#include "chrome/browser/ash/login/test/logged_in_user_mixin.h"
#include "chrome/browser/ash/login/test/oobe_screen_waiter.h"
#include "chrome/browser/ash/login/ui/login_display_host.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/webui/chromeos/login/oobe_ui.h"
#include "chrome/browser/ui/webui/chromeos/login/supervision_transition_screen_handler.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"
#include "components/arc/arc_prefs.h"
#include "components/arc/session/arc_session_runner.h"
#include "components/arc/session/arc_supervision_transition.h"
#include "components/arc/test/fake_arc_session.h"
#include "components/prefs/pref_service.h"
#include "components/user_manager/user_type.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_launcher.h"

namespace chromeos {

namespace {

constexpr char kSupervisionTransitionId[] = "supervision-transition";

const test::UIPath kSupervisionDialog = {kSupervisionTransitionId,
                                         "supervisionTransitionDialog"};
const test::UIPath kErrorDialog = {kSupervisionTransitionId,
                                   "supervisionTransitionErrorDialog"};
const test::UIPath kAcceptButton = {kSupervisionTransitionId, "accept-button"};

}  // namespace

// Param returns the original user type.
class SupervisionTransitionScreenTest
    : public MixinBasedInProcessBrowserTest,
      public testing::WithParamInterface<LoggedInUserMixin::LogInType> {
 public:
  SupervisionTransitionScreenTest() = default;
  ~SupervisionTransitionScreenTest() override = default;

  // MixinBasedInProcessBrowserTest:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitchASCII(switches::kArcAvailability,
                                    "officially-supported");
    MixinBasedInProcessBrowserTest::SetUpCommandLine(command_line);
  }

  void SetUpOnMainThread() override {
    arc::ArcServiceLauncher::Get()->ResetForTesting();
    arc::ArcSessionManager::Get()->SetArcSessionRunnerForTesting(
        std::make_unique<arc::ArcSessionRunner>(
            base::BindRepeating(arc::FakeArcSession::Create)));
    arc::ExpandPropertyFilesForTesting(arc::ArcSessionManager::Get());

    MixinBasedInProcessBrowserTest::SetUpOnMainThread();
    // For this test class, the PRE tests just happen to always wait for active
    // session immediately after logging in, while the main tests do some checks
    // and then postpone WaitForActiveSession() until later. So wait for active
    // session immediately if IsPreTest() and postpone the call to
    // WaitForActiveSession() otherwise.
    logged_in_user_mixin_.LogInUser(
        false /*issue_any_scope_token*/,
        content::IsPreTest() /*wait_for_active_session*/);
  }

  // The tests simulate user type changes between regular and child user.
  // This returns the intended user type after transition. GetParam() returns
  // the initial user type.
  LoggedInUserMixin::LogInType GetTargetUserType() const {
    return GetParam() == LoggedInUserMixin::LogInType::kRegular
               ? LoggedInUserMixin::LogInType::kChild
               : LoggedInUserMixin::LogInType::kRegular;
  }

 protected:
  LoggedInUserMixin& logged_in_user_mixin() { return logged_in_user_mixin_; }

 private:
  LoggedInUserMixin logged_in_user_mixin_{
      &mixin_host_, content::IsPreTest() ? GetParam() : GetTargetUserType(),
      embedded_test_server(), this, false /*should_launch_browser*/};
};

IN_PROC_BROWSER_TEST_P(SupervisionTransitionScreenTest,
                       PRE_SuccessfulTransition) {
  Profile* profile = ProfileManager::GetPrimaryUserProfile();
  profile->GetPrefs()->SetBoolean(arc::prefs::kArcSignedIn, true);
  arc::SetArcPlayStoreEnabledForProfile(profile, true);
}

IN_PROC_BROWSER_TEST_P(SupervisionTransitionScreenTest, SuccessfulTransition) {
  OobeScreenWaiter(SupervisionTransitionScreenView::kScreenId).Wait();

  test::OobeJS().ExpectVisiblePath(kSupervisionDialog);
  test::OobeJS().ExpectHiddenPath(kErrorDialog);

  EXPECT_FALSE(ash::LoginScreenTestApi::IsGuestButtonShown());
  EXPECT_FALSE(ash::LoginScreenTestApi::IsAddUserButtonShown());

  ProfileManager::GetPrimaryUserProfile()->GetPrefs()->SetInteger(
      arc::prefs::kArcSupervisionTransition,
      static_cast<int>(arc::ArcSupervisionTransition::NO_TRANSITION));

  EXPECT_FALSE(ProfileManager::GetPrimaryUserProfile()->GetPrefs()->GetBoolean(
      arc::prefs::kArcDataRemoveRequested));

  logged_in_user_mixin().GetLoginManagerMixin()->WaitForActiveSession();
}

IN_PROC_BROWSER_TEST_P(SupervisionTransitionScreenTest, PRE_TransitionTimeout) {
  Profile* profile = ProfileManager::GetPrimaryUserProfile();
  profile->GetPrefs()->SetBoolean(arc::prefs::kArcSignedIn, true);
  arc::SetArcPlayStoreEnabledForProfile(profile, true);
}

// Flaky on linux-chromeos-rel (see https://crbug.com/1032997)
IN_PROC_BROWSER_TEST_P(SupervisionTransitionScreenTest,
                       DISABLED_TransitionTimeout) {
  OobeScreenWaiter(SupervisionTransitionScreenView::kScreenId).Wait();

  test::OobeJS().ExpectVisiblePath(kSupervisionDialog);
  test::OobeJS().ExpectHiddenPath(kErrorDialog);

  EXPECT_FALSE(ash::LoginScreenTestApi::IsGuestButtonShown());
  EXPECT_FALSE(ash::LoginScreenTestApi::IsAddUserButtonShown());

  base::OneShotTimer* timer =
      LoginDisplayHost::default_host()
          ->GetOobeUI()
          ->GetHandler<SupervisionTransitionScreenHandler>()
          ->GetTimerForTesting();
  ASSERT_TRUE(timer->IsRunning());
  timer->FireNow();

  EXPECT_TRUE(ProfileManager::GetPrimaryUserProfile()->GetPrefs()->GetBoolean(
      arc::prefs::kArcDataRemoveRequested));

  test::OobeJS().CreateVisibilityWaiter(true, kErrorDialog)->Wait();
  test::OobeJS().ExpectHiddenPath(kSupervisionDialog);

  EXPECT_FALSE(ash::LoginScreenTestApi::IsGuestButtonShown());
  EXPECT_FALSE(ash::LoginScreenTestApi::IsAddUserButtonShown());

  test::OobeJS().TapOnPath(kAcceptButton);

  logged_in_user_mixin().GetLoginManagerMixin()->WaitForActiveSession();
}

IN_PROC_BROWSER_TEST_P(SupervisionTransitionScreenTest,
                       PRE_SkipTransitionIfArcNeverStarted) {
}

IN_PROC_BROWSER_TEST_P(SupervisionTransitionScreenTest,
                       SkipTransitionIfArcNeverStarted) {
  // Login should go through without being interrupted.
  logged_in_user_mixin().GetLoginManagerMixin()->WaitForActiveSession();
}

INSTANTIATE_TEST_SUITE_P(All,
                         SupervisionTransitionScreenTest,
                         testing::Values(LoggedInUserMixin::LogInType::kRegular,
                                         LoggedInUserMixin::LogInType::kChild));

}  // namespace chromeos
