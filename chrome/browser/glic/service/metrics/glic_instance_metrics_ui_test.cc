// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/string_number_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/metrics/user_action_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/glic/host/glic_features.mojom.h"
#include "chrome/browser/glic/service/glic_instance_impl.h"
#include "chrome/browser/glic/service/metrics/glic_instance_metrics.h"
#include "chrome/browser/glic/service/metrics/glic_metrics_session_manager.h"
#include "chrome/browser/glic/test_support/interactive_glic_test.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_test_util.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/test/browser_test.h"

namespace glic {

// Use 45 ms for testing.
base::TimeDelta INACTIVITY_TIMEOUT_MS = base::Milliseconds(45);
// Use 30 ms for testing.
base::TimeDelta HIDDEN_TIMEOUT_MS = base::Milliseconds(30);
// Use 5 ms for testing.
base::TimeDelta START_TIMER_MS = base::Milliseconds(5);
// Use 1 ms for testing.
base::TimeDelta DEBOUNCE_TIMEOUT_MS = base::Milliseconds(1);

class GlicInstanceMetricsTest : public test::InteractiveGlicTest {
 public:
  GlicInstanceMetricsTest() {
    scoped_feature_list_.InitWithFeaturesAndParameters(
        {
            {features::kGlicMetricsSession,
             {{features::kGlicMetricsSessionInactivityTimeout.name,
               base::NumberToString(INACTIVITY_TIMEOUT_MS.InMilliseconds()) +
                   "ms"},
              {features::kGlicMetricsSessionHiddenTimeout.name,
               base::NumberToString(HIDDEN_TIMEOUT_MS.InMilliseconds()) + "ms"},
              {features::kGlicMetricsSessionStartTimeout.name,
               base::NumberToString(START_TIMER_MS.InMilliseconds()) + "ms"},
              {features::kGlicMetricsSessionRestartDebounceTimer.name,
               base::NumberToString(DEBOUNCE_TIMEOUT_MS.InMilliseconds()) +
                   "ms"}}},
            {features::kGlicMultiInstance, {}},
            {mojom::features::kGlicMultiTab, {}},
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
        START_TIMER_MS + base::Milliseconds(10));
    run_loop.Run();
  }
  EXPECT_EQ(user_action_tester_.GetActionCount("Glic.Instance.Created"), 2);
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
        START_TIMER_MS + base::Milliseconds(10));
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
      Wait(START_TIMER_MS + base::Milliseconds(10)), CloseGlic(),
      WaitForHide(test::kGlicHostElementId));
  {
    base::RunLoop run_loop;
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE, run_loop.QuitClosure(), HIDDEN_TIMEOUT_MS);
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

}  // namespace glic
