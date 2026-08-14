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
#include "components/ukm/test_ukm_recorder.h"
#include "content/public/test/browser_test.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_source_id.h"

namespace glic {

DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kSecondTabElementId);

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

IN_PROC_BROWSER_TEST_F(GlicInstanceMetricsTest,
                       AutoOpenForPdfRecordsUkmOnClose) {
  ukm::TestAutoSetUkmRecorder ukm_tester;
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("/title1.html")));
  ukm::SourceId active_tab_source_id = browser()
                                           ->tab_strip_model()
                                           ->GetActiveWebContents()
                                           ->GetPrimaryMainFrame()
                                           ->GetPageUkmSourceId();

  RunTestSequence(
      Do([this] {
        instance_coordinator().Toggle(
            browser(), false, mojom::InvocationSource::kAutoOpenedForPdf);
      }),
      WaitForAndInstrumentGlic(GlicInstrumentMode::kHostAndContents),
      Wait(kStartTimerMs + base::Milliseconds(10)), CloseGlic(),
      WaitForHide(kGlicHostElementId));

  auto entries = ukm_tester.GetEntriesByName(
      ukm::builders::Glic_AutoOpen_Closed::kEntryName);
  ASSERT_EQ(entries.size(), 1u);
  EXPECT_TRUE(ukm_tester.EntryHasMetric(
      entries[0], ukm::builders::Glic_AutoOpen_Closed::kSessionDurationMsName));
  EXPECT_EQ(entries[0]->source_id, active_tab_source_id);
}

class GlicInstanceMetricsTestWithDaisyChaining
    : public GlicInstanceMetricsTest {
 public:
  GlicInstanceMetricsTestWithDaisyChaining() {
    daisy_chain_feature_list_.InitWithFeatures(
        /*enabled_features=*/{features::kGlicDefaultToLastActiveConversation},
        /*disabled_features=*/{features::kGlicDaisyChainNewTabs});
  }

 private:
  base::test::ScopedFeatureList daisy_chain_feature_list_;
};

IN_PROC_BROWSER_TEST_F(GlicInstanceMetricsTestWithDaisyChaining,
                       AutoOpenForPdfDaisyChainIgnoresTabSwitch) {
  ukm::TestAutoSetUkmRecorder ukm_tester;
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("/title1.html")));
  ukm::SourceId first_tab_source_id = browser()
                                          ->tab_strip_model()
                                          ->GetActiveWebContents()
                                          ->GetPrimaryMainFrame()
                                          ->GetPageUkmSourceId();

  RunTestSequence(
      // Open PDF side panel on Tab 1
      Do([this] {
        instance_coordinator().Toggle(
            browser(), false, mojom::InvocationSource::kAutoOpenedForPdf);
      }),
      WaitForAndInstrumentGlic(GlicInstrumentMode::kHostAndContents),
      Wait(kStartTimerMs + base::Milliseconds(10)),
      StopObservingState(glic::test::internal::kDelayState),
      // Open Tab 2 and switch to it
      AddInstrumentedTab(kSecondTabElementId,
                         embedded_test_server()->GetURL("/title2.html")),
      // Verify no UKM logged on tab switch
      Do([&ukm_tester] {
        EXPECT_TRUE(ukm_tester
                        .GetEntriesByName(
                            ukm::builders::Glic_AutoOpen_Closed::kEntryName)
                        .empty());
      }),
      // Daisy chain to Tab 2
      Do([this] {
        auto* tab1 = browser()->tab_strip_model()->GetTabAtIndex(0);
        auto* tab2 = browser()->tab_strip_model()->GetTabAtIndex(1);
        auto* instance = GetGlicInstanceImpl();
        ASSERT_TRUE(instance);
        instance->MaybeDaisyChainToTab(tab1, tab2,
                                       DaisyChainSource::kAutoOpenPdf);
      }),
      WaitForShow(kGlicViewElementId),
      Wait(kStartTimerMs + base::Milliseconds(10)),
      StopObservingState(glic::test::internal::kDelayState),
      // Explicitly close Glic side panel on Tab 2
      Do([this] {
        auto* tab2 = browser()->tab_strip_model()->GetActiveTab();
        if (auto* coordinator = GlicSidePanelCoordinator::GetForTab(tab2)) {
          coordinator->Close();
        }
      }),
      WaitForHide(kGlicViewElementId),
      // Verify no UKM logged yet because Tab 1 still has its side panel open
      Do([&ukm_tester] {
        EXPECT_TRUE(ukm_tester
                        .GetEntriesByName(
                            ukm::builders::Glic_AutoOpen_Closed::kEntryName)
                        .empty());
      }),
      // Switch back to Tab 1
      Do([this] { browser()->tab_strip_model()->ActivateTabAt(0); }),
      WaitForShow(kGlicViewElementId),
      // Explicitly close Glic side panel on Tab 1
      Do([this] {
        auto* tab1 = browser()->tab_strip_model()->GetActiveTab();
        if (auto* coordinator = GlicSidePanelCoordinator::GetForTab(tab1)) {
          coordinator->Close();
        }
      }),
      WaitForHide(kGlicViewElementId));

  auto entries = ukm_tester.GetEntriesByName(
      ukm::builders::Glic_AutoOpen_Closed::kEntryName);
  ASSERT_EQ(entries.size(), 1u);
  EXPECT_TRUE(ukm_tester.EntryHasMetric(
      entries[0], ukm::builders::Glic_AutoOpen_Closed::kSessionDurationMsName));
  EXPECT_EQ(entries[0]->source_id, first_tab_source_id);
}

}  // namespace glic
