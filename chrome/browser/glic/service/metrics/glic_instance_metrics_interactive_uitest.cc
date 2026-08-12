// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/service/metrics/glic_instance_metrics.h"

#include "base/strings/string_number_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/metrics/user_action_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/glic/host/glic_features.mojom.h"
#include "chrome/browser/glic/public/features.h"
#include "chrome/browser/glic/public/glic_keyed_service.h"
#include "chrome/browser/glic/public/glic_side_panel_coordinator.h"
#include "chrome/browser/glic/service/glic_instance_impl.h"
#include "chrome/browser/glic/service/metrics/glic_metrics_session_manager.h"
#include "chrome/browser/glic/test_support/glic_test_util.h"
#include "chrome/browser/glic/test_support/interactive_glic_test.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_test_util.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/test/browser_test.h"

namespace glic {

namespace {
// Use 45 ms for testing.
constexpr base::TimeDelta kInactivityTimeoutMs = base::Milliseconds(45);
// Use 30 ms for testing.
constexpr base::TimeDelta kHiddenTimeoutMs = base::Milliseconds(30);
// Use 5 ms for testing.
constexpr base::TimeDelta kStartTimerMs = base::Milliseconds(5);
// Use 1 ms for testing.
constexpr base::TimeDelta kDebounceTimeoutMs = base::Milliseconds(1);
}  // namespace

// TODO(crbug.com/537846973): Migrate this test suite to GlicBrowserTest.
class GlicInstanceMetricsTest : public test::InteractiveGlicTest {
 public:
  GlicInstanceMetricsTest() {
    scoped_feature_list_.InitWithFeaturesAndParameters(
        {
            {features::kGlicMetricsSession,
             {{features::kGlicMetricsSessionInactivityTimeout.name,
               base::NumberToString(kInactivityTimeoutMs.InMilliseconds()) +
                   "ms"},
              {features::kGlicMetricsSessionHiddenTimeout.name,
               base::NumberToString(kHiddenTimeoutMs.InMilliseconds()) + "ms"},
              {features::kGlicMetricsSessionStartTimeout.name,
               base::NumberToString(kStartTimerMs.InMilliseconds()) + "ms"},
              {features::kGlicMetricsSessionRestartDebounceTimer.name,
               base::NumberToString(kDebounceTimeoutMs.InMilliseconds()) +
                   "ms"}}},
        },
        {});
  }
  ~GlicInstanceMetricsTest() override = default;

  void SetUp() override { test::InteractiveGlicTest::SetUp(); }

 protected:
  base::HistogramTester histogram_tester_;
  base::UserActionTester user_action_tester_;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(GlicInstanceMetricsTest,
                       InstanceCreatedRecordsUserAction) {
  RunTestSequence(
      ToggleGlicWindow(GlicWindowMode::kAttached),
      WaitForAndInstrumentGlic(GlicInstrumentMode::kHostAndContents));
  {
    base::RunLoop run_loop;
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE, run_loop.QuitClosure(),
        kStartTimerMs + base::Milliseconds(10));
    run_loop.Run();
  }
  EXPECT_EQ(user_action_tester_.GetActionCount("Glic.Instance.Created"), 1);
}

IN_PROC_BROWSER_TEST_F(GlicInstanceMetricsTest,
                       InstanceCreatedAndHiddenRecordsOpenDuration) {
  RunTestSequence(
      ToggleGlicWindow(GlicWindowMode::kAttached),
      WaitForAndInstrumentGlic(GlicInstrumentMode::kHostAndContents));
  {
    base::RunLoop run_loop;
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE, run_loop.QuitClosure(),
        kStartTimerMs + base::Milliseconds(10));
    run_loop.Run();
  }
  RunTestSequence(
      CloseGlic(),
      WaitUntil(
          [this]() {
            return base::NumberToString(
                histogram_tester_
                    .GetAllSamples("Glic.Instance.SidePanel.OpenDuration")
                    .size());
          },
          "1"));
}

IN_PROC_BROWSER_TEST_F(GlicInstanceMetricsTest, SessionEndsWhenHidden) {
  RunTestSequence(
      ToggleGlicWindow(GlicWindowMode::kAttached),
      WaitForAndInstrumentGlic(GlicInstrumentMode::kHostAndContents),
      Wait(kStartTimerMs + base::Milliseconds(10)), CloseGlic(),
      WaitForHide(kGlicHostElementId));
  {
    base::RunLoop run_loop;
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE, run_loop.QuitClosure(), kHiddenTimeoutMs);
    run_loop.Run();
  }
  EXPECT_EQ(
      histogram_tester_.GetAllSamples("Glic.Instance.SidePanel.OpenDuration")
          .size(),
      1u);
  // Session should end after hidden timeout.
  EXPECT_EQ(user_action_tester_.GetActionCount("Glic.Instance.Session.End"), 1);
  EXPECT_EQ(histogram_tester_.GetBucketCount(
                "Glic.Instance.Session.EndReason",
                GlicMultiInstanceSessionEndReason::kHidden),
            1);
}

class GlicFreMetricsTest : public test::InteractiveGlicTest {
 public:
  GlicFreMetricsTest() {}
  ~GlicFreMetricsTest() override = default;

  void SetUpOnMainThread() override {
    test::InteractiveGlicTest::SetUpOnMainThread();
    glic::GlicKeyedService::Get(browser()->GetProfile())
        ->enabling()
        .SetCompletedFre(glic::prefs::FreStatus::kNotStarted);
  }

 protected:
  base::UserActionTester user_action_tester_;
};

IN_PROC_BROWSER_TEST_F(GlicFreMetricsTest, FreShownAndDismissed) {
  RunTestSequence(
      ToggleGlicWindow(GlicWindowMode::kAttached),
      WaitForAndInstrumentGlic(GlicInstrumentMode::kHostAndContents),
      Wait(kStartTimerMs + base::Milliseconds(10)),
      ToggleGlicWindow(GlicWindowMode::kAttached),
      WaitForHide(kGlicHostElementId));

  EXPECT_EQ(user_action_tester_.GetActionCount("Glic.Fre.Shown"), 1);
  EXPECT_EQ(user_action_tester_.GetActionCount("Glic.Onboarding.OptInAccept"),
            0);
  EXPECT_EQ(user_action_tester_.GetActionCount("Glic.Fre.Dismissed"), 1);
}

}  // namespace glic
