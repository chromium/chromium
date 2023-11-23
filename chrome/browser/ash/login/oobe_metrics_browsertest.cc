// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/login_screen_test_api.h"
#include "chrome/browser/ash/login/login_pref_names.h"
#include "chrome/browser/ash/login/test/js_checker.h"
#include "chrome/browser/ash/login/test/login_manager_mixin.h"
#include "chrome/browser/ash/login/test/oobe_base_test.h"
#include "chrome/browser/ash/login/test/oobe_screen_exit_waiter.h"
#include "chrome/browser/ash/login/test/oobe_screens_utils.h"
#include "chrome/browser/ash/login/wizard_controller.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/metrics/structured/test/structured_metrics_mixin.h"
#include "chrome/browser/metrics/structured/test/test_structured_metrics_recorder.h"
#include "chrome/browser/ui/webui/ash/login/marketing_opt_in_screen_handler.h"
#include "chrome/test/base/fake_gaia_mixin.h"
#include "components/metrics/structured/structured_events.h"
#include "components/version_info/version_info.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/browser_test.h"

namespace ash {
namespace {

namespace cros_events = metrics::structured::events::v2::cr_os_events;

}  // namespace

// TODO(b/305929689): Introduce tests for the different values of
// `metric_values`.
class OobeMetricsTest : public OobeBaseTest {
 public:
  OobeMetricsTest() {
    feature_list_.InitAndEnableFeature(ash::features::kOobeCrosEvents);
  }

  void SetUpOnMainThread() override {
    structured_metrics_recorder_ =
        std::make_unique<metrics::structured::TestStructuredMetricsRecorder>();
    structured_metrics_recorder_->Initialize();
    LoginDisplayHost::default_host()->GetWizardContext()->is_branded_build =
        true;
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

  LoginManagerMixin login_manager_mixin_{&mixin_host_, {}, &fake_gaia_};
  std::unique_ptr<metrics::structured::TestStructuredMetricsRecorder>
      structured_metrics_recorder_;

 private:
  FakeGaiaMixin fake_gaia_{&mixin_host_};
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

}  // namespace ash
