// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/service/glic_instance_helper.h"

#include "base/memory/weak_ptr.h"
#include "chrome/browser/glic/public/context/glic_sharing_manager.h"
#include "chrome/browser/glic/service/glic_instance_impl.h"
#include "chrome/browser/glic/test_support/glic_browser_test.h"
#include "chrome/browser/performance_manager/policies/discard_eligibility_policy.h"
#include "components/performance_manager/public/graph/page_node.h"
#include "components/performance_manager/public/performance_manager.h"
#include "components/ukm/test_ukm_recorder.h"
#include "content/public/test/browser_test.h"
#include "services/metrics/public/cpp/metrics_utils.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/flags/android/chrome_feature_list.h"
#include "content/public/browser/android/child_process_importance.h"
#endif

namespace glic {

class GlicInstanceHelperBrowserTest : public GlicBrowserTest {
 protected:
  void SetUp() override {
#if BUILDFLAG(IS_ANDROID)
    // Disable protection for recently visible tabs so that we can test the
    // protection when the Glic instance is open.
    feature_list_.InitAndEnableFeatureWithParameters(
        chrome::android::kProtectRecentlyVisibleTab,
        {{"duration_in_seconds", "0"}});
#endif  // BUILDFLAG(IS_ANDROID)
    GlicBrowserTest::SetUp();
  }

 private:
#if BUILDFLAG(IS_ANDROID)
  base::test::ScopedFeatureList feature_list_;
#endif  // BUILDFLAG(IS_ANDROID)
};

// Test that pinned tab protection depends on Glic instance visibility.
IN_PROC_BROWSER_TEST_F(GlicInstanceHelperBrowserTest, PinnedTabProtection) {
  // Create a tab and load a page.
  tabs::TabInterface* tab = CreateAndActivateTab(GetSimpleTestUrl());
  ASSERT_TRUE(tab);

  // Create a second tab to make the first tab hidden.
  tabs::TabInterface* tab2 = CreateAndActivateTab(GetSimpleTestUrl());
  ASSERT_TRUE(tab2);

  base::WeakPtr<performance_manager::PageNode> page_node =
      performance_manager::PerformanceManager::GetPrimaryPageNodeForWebContents(
          tab->GetContents());
  ASSERT_TRUE(page_node);

  auto* policy =
      performance_manager::policies::DiscardEligibilityPolicy::GetFromGraph(
          performance_manager::PerformanceManager::GetGraph());
  ASSERT_TRUE(policy);

  // Initially eligible.
  EXPECT_EQ(
      performance_manager::policies::CanDiscardResult::kEligible,
      policy->CanDiscard(page_node.get(),
                         performance_manager::policies::
                             DiscardEligibilityPolicy::DiscardReason::PROACTIVE,
                         /*ignore_recent_visibility=*/true));

  // Open a Glic instance on the active tab (tab2).
  ASSERT_OK_AND_ASSIGN(GlicInstanceImpl * instance, OpenGlicForActiveTab());
  ASSERT_TRUE(instance->IsShowing());

  // Pin background tab to the visible Glic instance.
  EXPECT_TRUE(instance->GetSharingManagerInternal().PinTabs(
      {tab->GetHandle()}, GlicPinTrigger::kUnknown));

  // Now protected.
  EXPECT_EQ(
      performance_manager::policies::CanDiscardResult::kProtected,
      policy->CanDiscard(page_node.get(),
                         performance_manager::policies::
                             DiscardEligibilityPolicy::DiscardReason::PROACTIVE,
                         /*ignore_recent_visibility=*/true));

  // Hide Glic without destroying the instance.
  PreventDeletionOnClose(instance);
  EXPECT_OK(CloseGlicForTabAndWait(tab2));

  // Eligible again while Glic is hidden.
  EXPECT_EQ(
      performance_manager::policies::CanDiscardResult::kEligible,
      policy->CanDiscard(page_node.get(),
                         performance_manager::policies::
                             DiscardEligibilityPolicy::DiscardReason::PROACTIVE,
                         /*ignore_recent_visibility=*/true));

  // Show Glic again.
  ASSERT_OK(OpenGlicForActiveTab());

  // Protected again.
  EXPECT_EQ(
      performance_manager::policies::CanDiscardResult::kProtected,
      policy->CanDiscard(page_node.get(),
                         performance_manager::policies::
                             DiscardEligibilityPolicy::DiscardReason::PROACTIVE,
                         /*ignore_recent_visibility=*/true));

  // Unpin the tab.
  EXPECT_TRUE(
      instance->GetSharingManagerInternal().UnpinTabs({tab->GetHandle()}));

  // Eligible again.
  EXPECT_EQ(
      performance_manager::policies::CanDiscardResult::kEligible,
      policy->CanDiscard(page_node.get(),
                         performance_manager::policies::
                             DiscardEligibilityPolicy::DiscardReason::PROACTIVE,
                         /*ignore_recent_visibility=*/true));
}

#if BUILDFLAG(IS_ANDROID)
// Android-specific test for process importance.
IN_PROC_BROWSER_TEST_F(GlicInstanceHelperBrowserTest,
                       PinnedTabAndroidImportance) {
  // Android Q+ required for NOT_PERCEPTIBLE importance.
  if (!content::IsNotPerceptibleImportanceSupported()) {
    GTEST_SKIP() << "NOT_PERCEPTIBLE importance not supported";
  }

  tabs::TabInterface* tab = CreateAndActivateTab(GetSimpleTestUrl());
  ASSERT_TRUE(tab);

  content::RenderProcessHost* rph =
      tab->GetContents()->GetPrimaryMainFrame()->GetProcess();
  ASSERT_TRUE(rph);

  GlicInstanceHelper* helper = GlicInstanceHelper::From(tab);
  ASSERT_TRUE(helper);

  auto run_until_importance_is =
      [&](content::ChildProcessImportance expected_importance) {
        return RunUntilEqual([&]() { return rph->GetEffectiveImportance(); },
                             expected_importance,
                             "Timeout waiting for importance");
      };

  // Active tab is IMPORTANT.
  EXPECT_OK(
      run_until_importance_is(content::ChildProcessImportance::IMPORTANT));

  // Background the tab by creating and activating another one.
  tabs::TabInterface* tab2 = CreateAndActivateTab(GetSimpleTestUrl());
  ASSERT_TRUE(tab2);

  EXPECT_OK(run_until_importance_is(content::ChildProcessImportance::NORMAL));

  // Open a Glic instance on the active tab (tab2).
  ASSERT_OK_AND_ASSIGN(GlicInstanceImpl * instance, OpenGlicForActiveTab());
  ASSERT_TRUE(instance->IsShowing());

  // Pin background tab to the visible Glic instance. It should be protected.
  EXPECT_TRUE(instance->GetSharingManagerInternal().PinTabs(
      {tab->GetHandle()}, GlicPinTrigger::kUnknown));
  EXPECT_OK(run_until_importance_is(
      content::ChildProcessImportance::NOT_PERCEPTIBLE));

  // Hide Glic without destroying the instance.
  EXPECT_OK(CloseGlicForTabAndWait(tab2));

  // Eligible again while Glic is hidden.
  EXPECT_OK(run_until_importance_is(content::ChildProcessImportance::NORMAL));

  // Show Glic again.
  ASSERT_OK(OpenGlicForActiveTab());

  // Protected, so boosted again.
  EXPECT_OK(run_until_importance_is(
      content::ChildProcessImportance::NOT_PERCEPTIBLE));

  // Unpin the tab.
  EXPECT_TRUE(
      instance->GetSharingManagerInternal().UnpinTabs({tab->GetHandle()}));

  // Eligible again.
  EXPECT_OK(run_until_importance_is(content::ChildProcessImportance::NORMAL));
}
#endif  // BUILDFLAG(IS_ANDROID)

IN_PROC_BROWSER_TEST_F(GlicInstanceHelperBrowserTest,
                       AutoOpenPdfLogsUkmOnClose) {
  ukm::TestAutoSetUkmRecorder ukm_tester;
  tabs::TabInterface* tab = CreateAndActivateTab(GetSimpleTestUrl());
  ASSERT_TRUE(tab);
  ukm::SourceId active_tab_source_id =
      tab->GetContents()->GetPrimaryMainFrame()->GetPageUkmSourceId();

  GlicKeyedService::Get(GetProfile())
      ->ToggleUI(tab->GetBrowserWindowInterface(), /*prevent_close=*/false,
                 mojom::InvocationSource::kAutoOpenedForPdf);
  ASSERT_OK(WaitForGlicOpen(tab));
  EXPECT_OK(CloseGlicForTabAndWait(tab));

  // Wait for the 5-second debounce timer to flush UKM.
  EXPECT_TRUE(base::test::RunUntil([&]() {
    return !ukm_tester
                .GetEntriesByName(
                    ukm::builders::Glic_AutoOpen_Closed::kEntryName)
                .empty();
  }));

  auto entries = ukm_tester.GetEntriesByName(
      ukm::builders::Glic_AutoOpen_Closed::kEntryName);
  ASSERT_EQ(entries.size(), 1u);
  EXPECT_TRUE(ukm_tester.EntryHasMetric(
      entries[0], ukm::builders::Glic_AutoOpen_Closed::kSessionDurationMsName));
  EXPECT_TRUE(ukm_tester.EntryHasMetric(
      entries[0],
      ukm::builders::Glic_AutoOpen_Closed::kTimeToFirstActionMsName));
  EXPECT_EQ(entries[0]->source_id, active_tab_source_id);
  ukm_tester.ExpectEntryMetric(
      entries[0], ukm::builders::Glic_AutoOpen_Closed::kCloseReasonName,
      static_cast<int64_t>(AutoOpenCloseReason::kExplicitlyClosed));
  ukm_tester.ExpectEntryMetric(
      entries[0], ukm::builders::Glic_AutoOpen_Closed::kFirstActionName,
      static_cast<int64_t>(DaisyChainFirstAction::kSidePanelClosed));
  ukm_tester.ExpectEntryMetric(
      entries[0], ukm::builders::Glic_AutoOpen_Closed::kPromptCountName,
      ukm::GetExponentialBucketMinForCounts1000(0));
}

IN_PROC_BROWSER_TEST_F(GlicInstanceHelperBrowserTest,
                       AutoOpenPdfLogsUkmOnTabSwitch) {
  ukm::TestAutoSetUkmRecorder ukm_tester;
  tabs::TabInterface* tab1 = CreateAndActivateTab(GetSimpleTestUrl());
  ASSERT_TRUE(tab1);
  ukm::SourceId first_tab_source_id =
      tab1->GetContents()->GetPrimaryMainFrame()->GetPageUkmSourceId();

  GlicKeyedService::Get(GetProfile())
      ->ToggleUI(tab1->GetBrowserWindowInterface(), /*prevent_close=*/false,
                 mojom::InvocationSource::kAutoOpenedForPdf);
  ASSERT_OK(WaitForGlicOpen(tab1));

  // Create and switch to tab2.
  tabs::TabInterface* tab2 = CreateAndActivateTab(GetSimpleTestUrl());
  ASSERT_TRUE(tab2);

  // Wait for the 5-second debounce timer to flush UKM.
  EXPECT_TRUE(base::test::RunUntil([&]() {
    return !ukm_tester
                .GetEntriesByName(
                    ukm::builders::Glic_AutoOpen_Closed::kEntryName)
                .empty();
  }));

  auto entries = ukm_tester.GetEntriesByName(
      ukm::builders::Glic_AutoOpen_Closed::kEntryName);
  ASSERT_EQ(entries.size(), 1u);
  EXPECT_TRUE(ukm_tester.EntryHasMetric(
      entries[0], ukm::builders::Glic_AutoOpen_Closed::kSessionDurationMsName));
  EXPECT_TRUE(ukm_tester.EntryHasMetric(
      entries[0],
      ukm::builders::Glic_AutoOpen_Closed::kTimeToFirstActionMsName));
  EXPECT_EQ(entries[0]->source_id, first_tab_source_id);
  ukm_tester.ExpectEntryMetric(
      entries[0], ukm::builders::Glic_AutoOpen_Closed::kCloseReasonName,
      static_cast<int64_t>(AutoOpenCloseReason::kTabSwitched));
  ukm_tester.ExpectEntryMetric(
      entries[0], ukm::builders::Glic_AutoOpen_Closed::kFirstActionName,
      static_cast<int64_t>(DaisyChainFirstAction::kTabSwitched));
  ukm_tester.ExpectEntryMetric(
      entries[0], ukm::builders::Glic_AutoOpen_Closed::kPromptCountName,
      ukm::GetExponentialBucketMinForCounts1000(0));
}

IN_PROC_BROWSER_TEST_F(GlicInstanceHelperBrowserTest,
                       AutoOpenPdfLogsUkmWithUserInput) {
  ukm::TestAutoSetUkmRecorder ukm_tester;
  tabs::TabInterface* tab = CreateAndActivateTab(GetSimpleTestUrl());
  ASSERT_TRUE(tab);
  ukm::SourceId active_tab_source_id =
      tab->GetContents()->GetPrimaryMainFrame()->GetPageUkmSourceId();

  GlicKeyedService::Get(GetProfile())
      ->ToggleUI(tab->GetBrowserWindowInterface(), /*prevent_close=*/false,
                 mojom::InvocationSource::kAutoOpenedForPdf);
  ASSERT_OK_AND_ASSIGN(GlicInstanceImpl * instance, WaitForGlicOpen(tab));

  instance->instance_metrics().OnUserInputSubmitted(
      mojom::WebClientMode::kAudio);
  instance->instance_metrics().OnUserInputSubmitted(
      mojom::WebClientMode::kText);

  EXPECT_OK(CloseGlicForTabAndWait(tab));

  // Wait for the 5-second debounce timer to flush UKM.
  EXPECT_TRUE(base::test::RunUntil([&]() {
    return !ukm_tester
                .GetEntriesByName(
                    ukm::builders::Glic_AutoOpen_Closed::kEntryName)
                .empty();
  }));

  auto entries = ukm_tester.GetEntriesByName(
      ukm::builders::Glic_AutoOpen_Closed::kEntryName);
  ASSERT_EQ(entries.size(), 1u);
  EXPECT_TRUE(ukm_tester.EntryHasMetric(
      entries[0], ukm::builders::Glic_AutoOpen_Closed::kSessionDurationMsName));
  EXPECT_TRUE(ukm_tester.EntryHasMetric(
      entries[0],
      ukm::builders::Glic_AutoOpen_Closed::kTimeToFirstActionMsName));
  EXPECT_EQ(entries[0]->source_id, active_tab_source_id);
  ukm_tester.ExpectEntryMetric(
      entries[0], ukm::builders::Glic_AutoOpen_Closed::kCloseReasonName,
      static_cast<int64_t>(AutoOpenCloseReason::kExplicitlyClosed));
  ukm_tester.ExpectEntryMetric(
      entries[0], ukm::builders::Glic_AutoOpen_Closed::kFirstActionName,
      static_cast<int64_t>(DaisyChainFirstAction::kInputSubmitted));
  ukm_tester.ExpectEntryMetric(
      entries[0], ukm::builders::Glic_AutoOpen_Closed::kPromptCountName,
      ukm::GetExponentialBucketMinForCounts1000(2));
}

}  // namespace glic
