// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>

#include "ash/components/arc/arc_features.h"
#include "ash/components/arc/arc_prefs.h"
#include "ash/components/arc/session/arc_management_transition.h"
#include "ash/components/arc/session/arc_session_runner.h"
#include "ash/components/arc/test/fake_arc_session.h"
#include "ash/constants/ash_switches.h"
#include "ash/public/cpp/login_screen_test_api.h"
#include "base/run_loop.h"
#include "base/timer/timer.h"
#include "chrome/browser/ash/arc/arc_util.h"
#include "chrome/browser/ash/arc/session/arc_service_launcher.h"
#include "chrome/browser/ash/arc/session/arc_session_manager.h"
#include "chrome/browser/ash/arc/test/test_arc_session_manager.h"
#include "chrome/browser/ash/login/screens/management_transition_screen.h"
#include "chrome/browser/ash/login/test/device_state_mixin.h"
#include "chrome/browser/ash/login/test/js_checker.h"
#include "chrome/browser/ash/login/test/logged_in_user_mixin.h"
#include "chrome/browser/ash/login/test/oobe_screen_waiter.h"
#include "chrome/browser/ash/login/ui/login_display_host.h"
#include "chrome/browser/ash/login/wizard_controller.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/webui/ash/login/management_transition_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/oobe_ui.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_service.h"
#include "components/user_manager/user_type.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_launcher.h"

namespace ash {
namespace {

// Renamed from "supervision-transition".
constexpr char kManagementTransitionId[] = "management-transition";

const test::UIPath kManagementDialog = {kManagementTransitionId,
                                        "managementTransitionDialog"};
const test::UIPath kErrorDialog = {kManagementTransitionId,
                                   "managementTransitionErrorDialog"};
const test::UIPath kAcceptButton = {kManagementTransitionId, "accept-button"};

struct TransitionScreenTestParams {
  TransitionScreenTestParams(LoggedInUserMixin::LogInType pre_test_user_type,
                             LoggedInUserMixin::LogInType test_user_type,
                             bool use_managed_account = false)
      : pre_test_user_type(pre_test_user_type),
        test_user_type(test_user_type),
        use_managed_account(use_managed_account) {}

  LoggedInUserMixin::LogInType pre_test_user_type;
  LoggedInUserMixin::LogInType test_user_type;
  bool use_managed_account;
};

// Param returns the original user type.
class ManagementTransitionScreenTest
    : public MixinBasedInProcessBrowserTest,
      public testing::WithParamInterface<TransitionScreenTestParams> {
 public:
  ManagementTransitionScreenTest() {
    feature_list_.InitAndEnableFeature(
        arc::kEnableUnmanagedToManagedTransitionFeature);
  }

  ManagementTransitionScreen* GetScreen() {
    return WizardController::default_controller()
        ->GetScreen<ManagementTransitionScreen>();
  }

  ~ManagementTransitionScreenTest() override = default;

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
    // Allow ARC by policy for managed users.
    if (use_managed_account()) {
      logged_in_user_mixin()
          .GetUserPolicyMixin()
          ->RequestPolicyUpdate()
          ->policy_payload()
          ->mutable_arcenabled()
          ->set_value(true);
    }

    // For this test class, the PRE tests just happen to always wait for active
    // session immediately after logging in, while the main tests do some checks
    // and then postpone WaitForActiveSession() until later. So wait for active
    // session immediately if IsPreTest() and postpone the call to
    // WaitForActiveSession() otherwise.
    logged_in_user_mixin_.LogInUser(
        false /*issue_any_scope_token*/,
        content::IsPreTest() /*wait_for_active_session*/);
  }

  LoggedInUserMixin::LogInType GetTargetUserType() const {
    return content::IsPreTest() ? GetParam().pre_test_user_type
                                : GetParam().test_user_type;
  }

  bool IsChild() const {
    return GetTargetUserType() == LoggedInUserMixin::LogInType::kChild;
  }

  bool use_managed_account() { return GetParam().use_managed_account; }

  absl::optional<AccountId> GetAccountId() {
    if (use_managed_account()) {
      return AccountId::FromUserEmailGaiaId(
          FakeGaiaMixin::kEnterpriseUser1,
          FakeGaiaMixin::kEnterpriseUser1GaiaId);
    }

    return absl::nullopt;
  }

 protected:
  LoggedInUserMixin& logged_in_user_mixin() { return logged_in_user_mixin_; }

 private:
  DeviceStateMixin device_state_{
      &mixin_host_,
      DeviceStateMixin::State::OOBE_COMPLETED_PERMANENTLY_UNOWNED};
  LoggedInUserMixin logged_in_user_mixin_{&mixin_host_,
                                          GetTargetUserType(),
                                          embedded_test_server(),
                                          this,
                                          false /*should_launch_browser*/,
                                          GetAccountId()};

  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_P(ManagementTransitionScreenTest,
                       PRE_SuccessfulTransition) {
  Profile* profile = ProfileManager::GetPrimaryUserProfile();
  profile->GetPrefs()->SetBoolean(arc::prefs::kArcSignedIn, true);
  profile->GetPrefs()->SetBoolean(arc::prefs::kArcIsManaged, IsChild());
  arc::SetArcPlayStoreEnabledForProfile(profile, true);
}

IN_PROC_BROWSER_TEST_P(ManagementTransitionScreenTest, SuccessfulTransition) {
  OobeScreenWaiter(ManagementTransitionScreenView::kScreenId).Wait();

  test::OobeJS().ExpectVisiblePath(kManagementDialog);
  test::OobeJS().ExpectHiddenPath(kErrorDialog);

  EXPECT_FALSE(LoginScreenTestApi::IsGuestButtonShown());
  EXPECT_FALSE(LoginScreenTestApi::IsAddUserButtonShown());

  ProfileManager::GetPrimaryUserProfile()->GetPrefs()->SetInteger(
      arc::prefs::kArcManagementTransition,
      static_cast<int>(arc::ArcManagementTransition::NO_TRANSITION));

  EXPECT_FALSE(ProfileManager::GetPrimaryUserProfile()->GetPrefs()->GetBoolean(
      arc::prefs::kArcDataRemoveRequested));

  logged_in_user_mixin().GetLoginManagerMixin()->WaitForActiveSession();
}

IN_PROC_BROWSER_TEST_P(ManagementTransitionScreenTest, PRE_TransitionTimeout) {
  Profile* profile = ProfileManager::GetPrimaryUserProfile();
  profile->GetPrefs()->SetBoolean(arc::prefs::kArcSignedIn, true);
  profile->GetPrefs()->SetBoolean(arc::prefs::kArcIsManaged, IsChild());
  arc::SetArcPlayStoreEnabledForProfile(profile, true);
}

// Flaky on linux-chromeos-rel (see https://crbug.com/1032997)
IN_PROC_BROWSER_TEST_P(ManagementTransitionScreenTest,
                       DISABLED_TransitionTimeout) {
  OobeScreenWaiter(ManagementTransitionScreenView::kScreenId).Wait();

  test::OobeJS().ExpectVisiblePath(kManagementDialog);
  test::OobeJS().ExpectHiddenPath(kErrorDialog);

  EXPECT_FALSE(LoginScreenTestApi::IsGuestButtonShown());
  EXPECT_FALSE(LoginScreenTestApi::IsAddUserButtonShown());

  base::OneShotTimer* timer = GetScreen()->GetTimerForTesting();
  ASSERT_TRUE(timer->IsRunning());
  timer->FireNow();

  EXPECT_TRUE(ProfileManager::GetPrimaryUserProfile()->GetPrefs()->GetBoolean(
      arc::prefs::kArcDataRemoveRequested));

  test::OobeJS().CreateVisibilityWaiter(true, kErrorDialog)->Wait();
  test::OobeJS().ExpectHiddenPath(kManagementDialog);

  EXPECT_FALSE(LoginScreenTestApi::IsGuestButtonShown());
  EXPECT_FALSE(LoginScreenTestApi::IsAddUserButtonShown());

  test::OobeJS().TapOnPath(kAcceptButton);

  logged_in_user_mixin().GetLoginManagerMixin()->WaitForActiveSession();
}

IN_PROC_BROWSER_TEST_P(ManagementTransitionScreenTest,
                       PRE_SkipTransitionIfArcNeverStarted) {}

IN_PROC_BROWSER_TEST_P(ManagementTransitionScreenTest,
                       SkipTransitionIfArcNeverStarted) {
  // Login should go through without being interrupted.
  logged_in_user_mixin().GetLoginManagerMixin()->WaitForActiveSession();
}

INSTANTIATE_TEST_SUITE_P(
    All,
    ManagementTransitionScreenTest,
    testing::Values(
        TransitionScreenTestParams(LoggedInUserMixin::LogInType::kChild,
                                   LoggedInUserMixin::LogInType::kRegular),
        TransitionScreenTestParams(LoggedInUserMixin::LogInType::kRegular,
                                   LoggedInUserMixin::LogInType::kChild),
        TransitionScreenTestParams(LoggedInUserMixin::LogInType::kRegular,
                                   LoggedInUserMixin::LogInType::kRegular,
                                   true /* use_managed_account */)));

}  // namespace
}  // namespace ash
