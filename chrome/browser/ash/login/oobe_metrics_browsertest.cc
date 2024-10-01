// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/login_screen_test_api.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/ash/login/login_pref_names.h"
#include "chrome/browser/ash/login/screens/consolidated_consent_screen.h"
#include "chrome/browser/ash/login/test/cryptohome_mixin.h"
#include "chrome/browser/ash/login/test/js_checker.h"
#include "chrome/browser/ash/login/test/login_manager_mixin.h"
#include "chrome/browser/ash/login/test/oobe_base_test.h"
#include "chrome/browser/ash/login/test/oobe_screen_exit_waiter.h"
#include "chrome/browser/ash/login/test/oobe_screen_waiter.h"
#include "chrome/browser/ash/login/test/oobe_screens_utils.h"
#include "chrome/browser/ash/login/test/user_auth_config.h"
#include "chrome/browser/ash/login/wizard_controller.h"
#include "chrome/browser/ash/settings/stats_reporting_controller.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/metrics/structured/test/structured_metrics_mixin.h"
#include "chrome/browser/ui/webui/ash/login/choobe_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/consolidated_consent_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/gaia_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/marketing_opt_in_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/theme_selection_screen_handler.h"
#include "chrome/test/base/fake_gaia_mixin.h"
#include "components/metrics/metrics_state_manager.h"
#include "components/metrics/structured/structured_events.h"
#include "components/metrics/structured/test/test_structured_metrics_recorder.h"
#include "components/prefs/pref_test_utils.h"
#include "components/version_info/version_info.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/browser_test.h"

namespace ash {
namespace {

namespace cros_events = metrics::structured::events::v2::cr_os_events;
using ::testing::ElementsAre;

constexpr const char kConsolidatedConsnetAcceptedRegular[] = "AcceptedRegular";

}  // namespace

// TODO(b/305929689): Add tests for Pre-login OOBE resume metrics.
// TODO(b/305929689): Introduce tests for the different values of
// `metric_values`.
class OobeMetricsTest : public OobeBaseTest {
 public:
  OobeMetricsTest() {
    feature_list_.InitAndEnableFeature(ash::features::kOobeCrosEvents);
  }

  void SetUpOnMainThread() override {
    fake_gaia_.SetupFakeGaiaForLoginWithDefaults();
    structured_metrics_recorder_ =
        std::make_unique<metrics::structured::TestStructuredMetricsRecorder>();
    structured_metrics_recorder_->Initialize();
    LoginDisplayHost::default_host()->GetWizardContext()->is_branded_build =
        true;
    cryptohome_mixin_.ApplyAuthConfigIfUserExists(
        user_, test::UserAuthConfig::Create(test::kDefaultAuthSetup));

    // Set a fake touchpad device to ensure that CHOOBE screen is shown.
    test::SetFakeTouchpadDevice();
    OobeBaseTest::SetUpOnMainThread();
  }

  OobeMetricsTest(const OobeMetricsTest&) = delete;
  OobeMetricsTest& operator=(const OobeMetricsTest&) = delete;
  ~OobeMetricsTest() override = default;

  void ValidateEventRecorded(const metrics::structured::Event& event) {
    bool event_found = false;
    const std::vector<metrics::structured::Event>& events =
        structured_metrics_recorder_->GetEvents();
    for (const auto& ev : events) {
      if (ev.event_name() == event.event_name() &&
          ev.metric_values() == event.metric_values()) {
        event_found = true;
        break;
      }
    }
    CHECK(event_found)
        << "Event " << event.event_name()
        << " was not found or found with unexpected metrics values.";
  }

  base::HistogramTester histogram_tester_;
  LoginManagerMixin login_manager_mixin_{&mixin_host_, {}, &fake_gaia_};
  std::unique_ptr<metrics::structured::TestStructuredMetricsRecorder>
      structured_metrics_recorder_;
  AccountId user_{
      AccountId::FromUserEmailGaiaId(test::kTestEmail, test::kTestGaiaId)};

 private:
  FakeGaiaMixin fake_gaia_{&mixin_host_};
  CryptohomeMixin cryptohome_mixin_{&mixin_host_};
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(OobeMetricsTest, OnboardingBoundaryMilestonesMetrics) {
  login_manager_mixin_.LoginAsNewRegularUser();
  OobeScreenExitWaiter(GetFirstSigninScreen()).Wait();

  cros_events::OOBE_OnboardingStarted onboarding_started_event;
  onboarding_started_event.SetIsFlexFlow(false)
      .SetIsDemoModeFlow(false)
      .SetIsEphemeralOrMGS(false)
      .SetIsFirstOnboarding(false)
      .SetIsOwnerUser(true)
      .SetChromeMilestone(version_info::GetMajorVersionNumberAsInt());
  ValidateEventRecorded(onboarding_started_event);

  WizardController::default_controller()->AdvanceToScreen(
      MarketingOptInScreenView::kScreenId);
  test::TapOnPathAndWaitForOobeToBeDestroyed(
      {"marketing-opt-in", "marketing-opt-in-next-button"});

  cros_events::OOBE_OnboardingCompleted onboarding_completed_event;
  onboarding_completed_event.SetIsFlexFlow(false)
      .SetIsDemoModeFlow(false)
      .SetIsEphemeralOrMGS(false)
      .SetIsFirstOnboarding(false)
      .SetIsOwnerUser(true)
      .SetChromeMilestone(version_info::GetMajorVersionNumberAsInt());
  ValidateEventRecorded(onboarding_completed_event);
}

IN_PROC_BROWSER_TEST_F(OobeMetricsTest, PageEnteredPageLeftEvents) {
  login_manager_mixin_.LoginAsNewRegularUser();
  OobeScreenExitWaiter(GetFirstSigninScreen()).Wait();
  test::WaitForConsolidatedConsentScreen();

  cros_events::OOBE_PageEntered page_entered_event;
  page_entered_event.SetPageId(ConsolidatedConsentScreenView::kScreenId.name)
      .SetIsFlexFlow(false)
      .SetIsDemoModeFlow(false)
      .SetIsEphemeralOrMGS(false)
      .SetIsFirstOnboarding(false)
      .SetIsOwnerUser(true)
      .SetChromeMilestone(version_info::GetMajorVersionNumberAsInt());
  ValidateEventRecorded(page_entered_event);

  test::TapConsolidatedConsentAccept();
  OobeScreenExitWaiter(ConsolidatedConsentScreenView::kScreenId).Wait();

  cros_events::OOBE_PageLeft page_left_event;
  page_left_event.SetPageId(ConsolidatedConsentScreenView::kScreenId.name)
      .SetExitReason(kConsolidatedConsnetAcceptedRegular)
      .SetIsFlexFlow(false)
      .SetIsDemoModeFlow(false)
      .SetIsEphemeralOrMGS(false)
      .SetIsFirstOnboarding(false)
      .SetIsOwnerUser(true)
      .SetChromeMilestone(version_info::GetMajorVersionNumberAsInt());
  ValidateEventRecorded(page_left_event);
}

IN_PROC_BROWSER_TEST_F(OobeMetricsTest, PageSkipped) {
  // Set `is_branded_build` to `false` to get the consolidated consent screen
  // skipped.
  LoginDisplayHost::default_host()->GetWizardContext()->is_branded_build =
      false;

  login_manager_mixin_.LoginAsNewRegularUser();
  OobeScreenExitWaiter(GetFirstSigninScreen()).Wait();
  OobeScreenExitWaiter(ConsolidatedConsentScreenView::kScreenId).Wait();

  cros_events::OOBE_PageSkippedBySystem page_skipped_event;
  page_skipped_event.SetPageId(ConsolidatedConsentScreenView::kScreenId.name)
      .SetIsFlexFlow(false)
      .SetIsDemoModeFlow(false)
      .SetIsEphemeralOrMGS(false)
      .SetIsFirstOnboarding(false)
      .SetIsOwnerUser(true)
      .SetChromeMilestone(version_info::GetMajorVersionNumberAsInt());
  ValidateEventRecorded(page_skipped_event);
}

IN_PROC_BROWSER_TEST_F(OobeMetricsTest, SignInEvents) {
  // `login_manager_mixin_.LoginAsNewRegularUser()` can not be used in this test
  // since a simulation of login steps are required to get Sign-in events
  // recorded.
  WizardController::default_controller()->AdvanceToScreen(GaiaView::kScreenId);
  OobeScreenWaiter(GaiaView::kScreenId).Wait();
  test::OobeJS()
      .CreateVisibilityWaiter(true, {"gaia-signin", "signin-frame-dialog"})
      ->Wait();
  LoginDisplayHost::default_host()
      ->GetOobeUI()
      ->GetView<GaiaScreenHandler>()
      ->ShowSigninScreenForTest(FakeGaiaMixin::kFakeUserEmail,
                                FakeGaiaMixin::kFakeUserPassword,
                                FakeGaiaMixin::kEmptyUserServices);
  test::WaitForConsolidatedConsentScreen();

  cros_events::OOBE_GaiaSigninRequested signin_requested_event;
  signin_requested_event.SetIsReauthentication(false)
      .SetIsFlexFlow(false)
      .SetIsDemoModeFlow(false)
      .SetIsEphemeralOrMGS(false)
      .SetIsFirstOnboarding(false)
      .SetIsOwnerUser(false)
      .SetChromeMilestone(version_info::GetMajorVersionNumberAsInt());
  ValidateEventRecorded(signin_requested_event);

  cros_events::OOBE_GaiaSigninCompleted signin_completed_event;
  signin_completed_event.SetIsReauthentication(false)
      .SetIsFlexFlow(false)
      .SetIsDemoModeFlow(false)
      .SetIsEphemeralOrMGS(false)
      .SetIsFirstOnboarding(false)
      .SetIsOwnerUser(false)
      .SetChromeMilestone(version_info::GetMajorVersionNumberAsInt());
  ValidateEventRecorded(signin_completed_event);
}

IN_PROC_BROWSER_TEST_F(OobeMetricsTest, PRE_ResumeOnboarding) {
  OobeScreenWaiter(UserCreationView::kScreenId).Wait();
  LoginManagerMixin::TestUserInfo test_user(user_);
  login_manager_mixin_.LoginWithDefaultContext(test_user);
  OobeScreenExitWaiter(GetFirstSigninScreen()).Wait();

  WizardController::default_controller()->AdvanceToScreen(
      ChoobeScreenView::kScreenId);
  test::OobeJS().TapOnPath(
      {"choobe", "screensList", "cr-button-theme-selection"});
  test::OobeJS().TapOnPath({"choobe", "nextButton"});
}

IN_PROC_BROWSER_TEST_F(OobeMetricsTest, ResumeOnboarding) {
  login_manager_mixin_.LoginAsNewRegularUser();
  OobeScreenWaiter(ThemeSelectionScreenView::kScreenId).Wait();

  cros_events::OOBE_OnboardingResumed onboarding_resumed_event;
  onboarding_resumed_event
      .SetPendingPageId(ThemeSelectionScreenView::kScreenId.name)
      .SetIsFlexFlow(false)
      .SetIsDemoModeFlow(false)
      .SetIsEphemeralOrMGS(false)
      .SetIsFirstOnboarding(false)
      .SetIsOwnerUser(true)
      .SetChromeMilestone(version_info::GetMajorVersionNumberAsInt());
  ValidateEventRecorded(onboarding_resumed_event);

  cros_events::OOBE_ChoobeResumed choobe_resumed_event;
  choobe_resumed_event.SetIsFlexFlow(false)
      .SetIsDemoModeFlow(false)
      .SetIsEphemeralOrMGS(false)
      .SetIsFirstOnboarding(false)
      .SetIsOwnerUser(true)
      .SetChromeMilestone(version_info::GetMajorVersionNumberAsInt());
  ValidateEventRecorded(choobe_resumed_event);
}

class FirstUserOobeMetricsTest : public OobeMetricsTest {
 public:
  FirstUserOobeMetricsTest() = default;
  FirstUserOobeMetricsTest(const FirstUserOobeMetricsTest&) = delete;
  FirstUserOobeMetricsTest& operator=(const FirstUserOobeMetricsTest&) = delete;
  ~FirstUserOobeMetricsTest() override = default;

  void SetUpOnMainThread() override {
    OobeMetricsTest::SetUpOnMainThread();
    g_browser_process->local_state()->ClearPref(prefs::kOobeComplete);
  }
};

IN_PROC_BROWSER_TEST_F(FirstUserOobeMetricsTest,
                       OobeBoundaryMilestonesMetrics) {
  login_manager_mixin_.LoginAsNewRegularUser();
  OobeScreenExitWaiter(GetFirstSigninScreen()).Wait();

  cros_events::OOBE_OobeStarted oobe_started_event;
  oobe_started_event.SetIsFlexFlow(false).SetChromeMilestone(
      version_info::GetMajorVersionNumberAsInt());
  ValidateEventRecorded(oobe_started_event);

  cros_events::OOBE_PreLoginOobeCompleted prelogin_oobe_completed_event;
  prelogin_oobe_completed_event
      .SetCompletedFlowType(static_cast<int>(
          OobeMetricsHelper::CompletedPreLoginOobeFlowType::kRegular))
      .SetIsFlexFlow(false)
      .SetIsDemoModeFlow(false)
      .SetChromeMilestone(version_info::GetMajorVersionNumberAsInt());

  ValidateEventRecorded(oobe_started_event);

  cros_events::OOBE_DeviceRegistered device_registered_event;
  device_registered_event.SetIsFlexFlow(false)
      .SetIsDemoModeFlow(false)
      .SetIsFirstOnboarding(true)
      .SetChromeMilestone(version_info::GetMajorVersionNumberAsInt());
  ValidateEventRecorded(device_registered_event);

  cros_events::OOBE_OnboardingStarted onboarding_started_event;
  onboarding_started_event.SetIsFlexFlow(false)
      .SetIsDemoModeFlow(false)
      .SetIsEphemeralOrMGS(false)
      .SetIsFirstOnboarding(true)
      .SetIsOwnerUser(true)
      .SetChromeMilestone(version_info::GetMajorVersionNumberAsInt());
  ValidateEventRecorded(onboarding_started_event);

  WizardController::default_controller()->AdvanceToScreen(
      MarketingOptInScreenView::kScreenId);
  test::TapOnPathAndWaitForOobeToBeDestroyed(
      {"marketing-opt-in", "marketing-opt-in-next-button"});

  cros_events::OOBE_OnboardingCompleted onboarding_completed_event;
  onboarding_completed_event.SetIsFlexFlow(false)
      .SetIsDemoModeFlow(false)
      .SetIsEphemeralOrMGS(false)
      .SetIsFirstOnboarding(true)
      .SetIsOwnerUser(true)
      .SetChromeMilestone(version_info::GetMajorVersionNumberAsInt());
  ValidateEventRecorded(onboarding_completed_event);

  cros_events::OOBE_OobeCompleted oobe_completed;
  oobe_completed.SetIsFlexFlow(false)
      .SetIsDemoModeFlow(false)
      .SetIsEphemeralOrMGS(false)
      .SetIsFirstOnboarding(true)
      .SetIsOwnerUser(true)
      .SetChromeMilestone(version_info::GetMajorVersionNumberAsInt());
  ValidateEventRecorded(oobe_completed);
}

// Metrics client ID is only created on Google Chrome branded builds.
// So, the following tests should only run on branded builds.
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
IN_PROC_BROWSER_TEST_F(FirstUserOobeMetricsTest, ClientIdNotReset) {
  login_manager_mixin_.LoginAsNewRegularUser();
  OobeScreenExitWaiter(GetFirstSigninScreen()).Wait();

  WizardController::default_controller()->AdvanceToScreen(
      MarketingOptInScreenView::kScreenId);
  test::TapOnPathAndWaitForOobeToBeDestroyed(
      {"marketing-opt-in", "marketing-opt-in-next-button"});

  EXPECT_THAT(histogram_tester_.GetAllSamples(
                  "OOBE.StatsReportingControllerReportedReset"),
              ElementsAre(base::Bucket(0, 1)));
  EXPECT_THAT(histogram_tester_.GetAllSamples("OOBE.MetricsClientIdReset"),
              ElementsAre(base::Bucket(0, 1)));
  EXPECT_THAT(histogram_tester_.GetAllSamples("OOBE.MetricsClientIdReset2"),
              ElementsAre(base::Bucket(0, 1)));

  // Verify that `kOobeMetricsClientIdAtOobeStart` preference was cleared.
  EXPECT_FALSE(g_browser_process->local_state()->HasPrefPath(
      prefs::kOobeMetricsClientIdAtOobeStart));
}

IN_PROC_BROWSER_TEST_F(FirstUserOobeMetricsTest, ClientIdReset) {
  login_manager_mixin_.LoginAsNewRegularUser();
  OobeScreenExitWaiter(GetFirstSigninScreen()).Wait();

  // Simulate the unexpected switching (during OOBE) of the enabled status of
  // the stats reporting setting from enabled to disabled to trigger the
  // unexpected switch causing the reset. Later enable the stats reporting
  // controller again to get the histograms to be recorded.
  StatsReportingController::Get()->SetEnabled(
      ProfileManager::GetActiveUserProfile(), true);
  WaitForPrefValue(g_browser_process->local_state(),
                   prefs::kOobeMetricsReportedAsEnabled, base::Value(true));

  StatsReportingController::Get()->SetEnabled(
      ProfileManager::GetActiveUserProfile(), false);
  WaitForPrefValue(g_browser_process->local_state(),
                   prefs::kOobeStatsReportingControllerReportedReset,
                   base::Value(true));

  StatsReportingController::Get()->SetEnabled(
      ProfileManager::GetActiveUserProfile(), true);

  WizardController::default_controller()->AdvanceToScreen(
      MarketingOptInScreenView::kScreenId);
  test::TapOnPathAndWaitForOobeToBeDestroyed(
      {"marketing-opt-in", "marketing-opt-in-next-button"});

  EXPECT_THAT(histogram_tester_.GetAllSamples(
                  "OOBE.StatsReportingControllerReportedReset"),
              ElementsAre(base::Bucket(1, 1)));
  EXPECT_THAT(histogram_tester_.GetAllSamples("OOBE.MetricsClientIdReset"),
              ElementsAre(base::Bucket(1, 1)));
  EXPECT_THAT(histogram_tester_.GetAllSamples("OOBE.MetricsClientIdReset2"),
              ElementsAre(base::Bucket(1, 1)));

  // Verify that `kOobeMetricsClientIdAtOobeStart` preference was cleared.
  EXPECT_FALSE(g_browser_process->local_state()->HasPrefPath(
      prefs::kOobeMetricsClientIdAtOobeStart));
}
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)

}  // namespace ash
