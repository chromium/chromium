// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/resource_coordinator/tab_activity_watcher.h"

#include <memory>

#include "base/macros.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/simple_test_tick_clock.h"
#include "chrome/browser/engagement/site_engagement_service.h"
#include "chrome/browser/resource_coordinator/lifecycle_unit.h"
#include "chrome/browser/resource_coordinator/tab_activity_watcher.h"
#include "chrome/browser/resource_coordinator/tab_lifecycle_unit.h"
#include "chrome/browser/resource_coordinator/tab_lifecycle_unit_source.h"
#include "chrome/browser/resource_coordinator/tab_manager_features.h"
#include "chrome/browser/resource_coordinator/time.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/recently_audible_helper.h"
#include "chrome/browser/ui/tabs/tab_activity_simulator.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tabs/tab_ukm_test_helper.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/test_browser_window.h"
#include "chrome/test/base/testing_profile.h"
#include "components/ukm/content/source_url_recorder.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/web_contents_tester.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_source.h"
#include "services/metrics/public/mojom/ukm_interface.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/web_input_event.h"
#include "third_party/blink/public/platform/web_mouse_event.h"

using blink::WebInputEvent;
using content::WebContentsTester;
using ukm::builders::TabManager_TabMetrics;
using ForegroundedOrClosed =
    ukm::builders::TabManager_Background_ForegroundedOrClosed;

namespace resource_coordinator {
namespace {

const char* kTabMetricsEntryName = TabManager_TabMetrics::kEntryName;

const int64_t kIdShift = 1 << 13;

// Test URLs need to be from different origins to test site engagement score.
const GURL kTestUrls[] = {
    GURL("https://test1.example.com"), GURL("https://test3.example.com"),
    GURL("https://test2.example.com"), GURL("https://test4.example.com"),
};

// The default metric values for a tab.
const UkmMetricMap kBasicMetricValues({
    {TabManager_TabMetrics::kHasBeforeUnloadHandlerName, 0},
    {TabManager_TabMetrics::kHasFormEntryName, 0},
    {TabManager_TabMetrics::kIsPinnedName, 0},
    {TabManager_TabMetrics::kKeyEventCountName, 0},
    {TabManager_TabMetrics::kMouseEventCountName, 0},
    {TabManager_TabMetrics::kSiteEngagementScoreName, 0},
    {TabManager_TabMetrics::kTouchEventCountName, 0},
    {TabManager_TabMetrics::kWasRecentlyAudibleName, 0},
});

blink::WebMouseEvent CreateMouseEvent(WebInputEvent::Type event_type) {
  return blink::WebMouseEvent(event_type, WebInputEvent::kNoModifiers,
                              WebInputEvent::GetStaticTimeStampForTests());
}

}  // namespace

// Base class for testing tab UKM (URL-Keyed Metrics) entries logged by
// TabMetricsLogger via TabActivityWatcher.
class TabActivityWatcherTest : public ChromeRenderViewHostTestHarness {
 public:
  TabActivityWatcherTest() = default;
  // Reset TabActivityWatcher with given |params|.
  void SetParams(const base::FieldTrialParams& params) {
    feature_list_.InitAndEnableFeatureWithParameters(features::kTabRanker,
                                                     params);
    TabActivityWatcher::GetInstance()->ResetForTesting();
  }
  ~TabActivityWatcherTest() override = default;

  LifecycleUnit* AddNewTab(TabStripModel* tab_strip_model, int i) {
    LifecycleUnit* result = TabLifecycleUnitSource::GetTabLifecycleUnit(
        tab_activity_simulator_.AddWebContentsAndNavigate(tab_strip_model,
                                                          GURL(kTestUrls[i])));
    if (i == 0)
      tab_strip_model->ActivateTabAt(i);
    else
      tab_activity_simulator_.SwitchToTabAt(tab_strip_model, i);

    return result;
  }

  // Calculate reactivation score of the |lifecycle_unit| using tab_ranker.
  base::Optional<float> GetReactivationScore(
      LifecycleUnit* const lifecycle_unit) {
    return TabActivityWatcher::GetInstance()->CalculateReactivationScore(
        lifecycle_unit->AsTabLifecycleUnitExternal()->GetWebContents());
  }

 protected:
  UkmEntryChecker ukm_entry_checker_;
  TabActivitySimulator tab_activity_simulator_;
  base::test::ScopedFeatureList feature_list_;

 private:
  DISALLOW_COPY_AND_ASSIGN(TabActivityWatcherTest);
};

// Test that lifecycleunits are sorted with high activation score first order.
TEST_F(TabActivityWatcherTest, LogAndMaybeSortLifecycleUnitWithTabRanker) {
  SetParams({{"scorer_type", "0"}});
  Browser::CreateParams params(profile(), true);
  std::unique_ptr<Browser> browser =
      CreateBrowserWithTestWindowForParams(&params);
  TabStripModel* tab_strip_model = browser->tab_strip_model();

  // Create lifecycleunits.
  LifecycleUnit* tab0 = AddNewTab(tab_strip_model, 0);
  LifecycleUnit* tab1 = AddNewTab(tab_strip_model, 1);
  LifecycleUnit* tab2 = AddNewTab(tab_strip_model, 2);
  LifecycleUnit* tab3 = AddNewTab(tab_strip_model, 3);
  std::vector<LifecycleUnit*> lifecycleunits = {tab0, tab2, tab3, tab1};

  // Sort and check the new order.
  TabActivityWatcher::GetInstance()->LogAndMaybeSortLifecycleUnitWithTabRanker(
      &lifecycleunits);
  EXPECT_EQ(lifecycleunits[0], tab3);
  EXPECT_EQ(lifecycleunits[1], tab2);
  EXPECT_EQ(lifecycleunits[2], tab1);
  EXPECT_EQ(lifecycleunits[3], tab0);

  tab_strip_model->CloseAllTabs();
}

// Test that lifecycleunits are sorted with high frecency score first order.
TEST_F(TabActivityWatcherTest, SortLifecycleUnitWithFrecencyScorer) {
  SetParams({{"scorer_type", "3"}});
  Browser::CreateParams params(profile(), true);
  std::unique_ptr<Browser> browser =
      CreateBrowserWithTestWindowForParams(&params);
  TabStripModel* tab_strip_model = browser->tab_strip_model();

  // Create lifecycleunits.
  LifecycleUnit* tab0 = AddNewTab(tab_strip_model, 0);
  LifecycleUnit* tab1 = AddNewTab(tab_strip_model, 1);
  LifecycleUnit* tab2 = AddNewTab(tab_strip_model, 2);
  LifecycleUnit* tab3 = AddNewTab(tab_strip_model, 3);

  for (int i = 0; i < 10; ++i) {
    tab_activity_simulator_.SwitchToTabAt(tab_strip_model, 0);
    tab_activity_simulator_.SwitchToTabAt(tab_strip_model, 1);
  }
  tab_activity_simulator_.SwitchToTabAt(tab_strip_model, 3);
  tab_activity_simulator_.SwitchToTabAt(tab_strip_model, 2);
  std::vector<LifecycleUnit*> lifecycleunits = {tab0, tab1, tab2, tab3};
  // Sort and check the new order.
  TabActivityWatcher::GetInstance()->LogAndMaybeSortLifecycleUnitWithTabRanker(
      &lifecycleunits);
  // tab2 is the first one because it is foregrounded.
  EXPECT_EQ(lifecycleunits[0], tab2);
  // tab1 and tab0 are in front of tab3 because they are reactived many times.
  EXPECT_EQ(lifecycleunits[1], tab1);
  EXPECT_EQ(lifecycleunits[2], tab0);
  // tab3 comes last.
  EXPECT_EQ(lifecycleunits[3], tab3);

  tab_strip_model->CloseAllTabs();
}

// Test that frecency scores are calculated correctly.
TEST_F(TabActivityWatcherTest, GetFrecencyScore) {
  SetParams({{"scorer_type", "3"}});
  Browser::CreateParams params(profile(), true);
  std::unique_ptr<Browser> browser =
      CreateBrowserWithTestWindowForParams(&params);
  TabStripModel* tab_strip_model = browser->tab_strip_model();

  LifecycleUnit* tab0 = AddNewTab(tab_strip_model, 0);
  // Foregrounded tab doesn't have a reactivation score.
  EXPECT_FALSE(GetReactivationScore(tab0).has_value());

  LifecycleUnit* tab1 = AddNewTab(tab_strip_model, 1);
  // Foregrounded tab doesn't have a reactivation score.
  EXPECT_FALSE(GetReactivationScore(tab1).has_value());
  EXPECT_FLOAT_EQ(GetReactivationScore(tab0).value(), 0.16f);

  LifecycleUnit* tab2 = AddNewTab(tab_strip_model, 2);
  // Foregrounded tab doesn't have a reactivation score.
  EXPECT_FALSE(GetReactivationScore(tab2).has_value());
  EXPECT_FLOAT_EQ(GetReactivationScore(tab1).value(), 0.16f);
  EXPECT_FLOAT_EQ(GetReactivationScore(tab0).value(), 0.128f);

  tab_activity_simulator_.SwitchToTabAt(tab_strip_model, 1);
  // Foregrounded tab doesn't have a reactivation score.
  EXPECT_FALSE(GetReactivationScore(tab1).has_value());
  EXPECT_FLOAT_EQ(GetReactivationScore(tab2).value(), 0.16f);
  EXPECT_FLOAT_EQ(GetReactivationScore(tab0).value(), 0.1024f);

  tab_activity_simulator_.SwitchToTabAt(tab_strip_model, 0);
  // Foregrounded tab doesn't have a reactivation score.
  EXPECT_FALSE(GetReactivationScore(tab0).has_value());
  EXPECT_FLOAT_EQ(GetReactivationScore(tab1).value(), 0.2624f);
  EXPECT_FLOAT_EQ(GetReactivationScore(tab2).value(), 0.128f);

  tab_strip_model->CloseAllTabs();
}

// Test that lifecycleunits are correctly logged inside
// LogAndMaybeSortLifecycleUnitWithTabRanker.
TEST_F(TabActivityWatcherTest,
       LogInsideLogAndMaybeSortLifecycleUnitWithTabRanker) {
  SetParams({{"disable_background_log_with_TabRanker", "true"}});
  Browser::CreateParams params(profile(), true);
  std::unique_ptr<Browser> browser =
      CreateBrowserWithTestWindowForParams(&params);
  TabStripModel* tab_strip_model = browser->tab_strip_model();

  // Create lifecycleunits.
  LifecycleUnit* tab0 = AddNewTab(tab_strip_model, 0);
  LifecycleUnit* tab1 = AddNewTab(tab_strip_model, 1);
  LifecycleUnit* tab2 = AddNewTab(tab_strip_model, 2);
  std::vector<LifecycleUnit*> lifecycleunits = {tab0};

  // Call LogAndMaybeSortLifecycleUnitWithTabRanker on tab0 should log the
  // TabMetrics for tab0.
  TabActivityWatcher::GetInstance()->LogAndMaybeSortLifecycleUnitWithTabRanker(
      &lifecycleunits);
  {
    SCOPED_TRACE("");
    ukm_entry_checker_.ExpectNewEntry(
        kTabMetricsEntryName, kTestUrls[0],
        {
            {TabManager_TabMetrics::kQueryIdName, 1 * kIdShift},
            {TabManager_TabMetrics::kLabelIdName, 2 * kIdShift},
        });
  }

  // Call LogAndMaybeSortLifecycleUnitWithTabRanker on tab0 should log the
  // TabMetrics for tab0.
  TabActivityWatcher::GetInstance()->LogAndMaybeSortLifecycleUnitWithTabRanker(
      &lifecycleunits);
  {
    SCOPED_TRACE("");
    ukm_entry_checker_.ExpectNewEntry(
        kTabMetricsEntryName, kTestUrls[0],
        {
            {TabManager_TabMetrics::kQueryIdName, 3 * kIdShift},
            {TabManager_TabMetrics::kLabelIdName, 2 * kIdShift + 1},
        });
  }

  // Call LogAndMaybeSortLifecycleUnitWithTabRanker on tab2 should not log the
  // TabMetrics for tab2 because it is foregrounded.
  lifecycleunits = {tab2};
  TabActivityWatcher::GetInstance()->LogAndMaybeSortLifecycleUnitWithTabRanker(
      &lifecycleunits);
  {
    SCOPED_TRACE("");
    EXPECT_EQ(ukm_entry_checker_.NumNewEntriesRecorded(kTabMetricsEntryName),
              0);
  }

  // Call LogAndMaybeSortLifecycleUnitWithTabRanker on all three tabs should log
  // two TabMetrics events for tab0 and tab1.
  lifecycleunits = {tab0, tab1, tab2};
  TabActivityWatcher::GetInstance()->LogAndMaybeSortLifecycleUnitWithTabRanker(
      &lifecycleunits);
  {
    SCOPED_TRACE("");
    EXPECT_EQ(ukm_entry_checker_.NumNewEntriesRecorded(kTabMetricsEntryName),
              2);

    ukm_entry_checker_.ExpectNewEntry(
        kTabMetricsEntryName, kTestUrls[0],
        {
            {TabManager_TabMetrics::kQueryIdName, 5 * kIdShift},
            {TabManager_TabMetrics::kLabelIdName, 2 * kIdShift + 2},
        });

    ukm_entry_checker_.ExpectNewEntry(
        kTabMetricsEntryName, kTestUrls[1],
        {
            {TabManager_TabMetrics::kQueryIdName, 5 * kIdShift},
            {TabManager_TabMetrics::kLabelIdName, 6 * kIdShift},
        });
  }

  tab_strip_model->CloseAllTabs();
}

// Tests TabManager.TabMetrics UKM entries generated when tabs are backgrounded.
class TabMetricsTest : public TabActivityWatcherTest {
 public:
  TabMetricsTest() {
    SetParams({{"scorer_type", "0"},
               {"disable_background_log_with_TabRanker", "false"}});
  }
  ~TabMetricsTest() override = default;

 protected:
  // Expects that a new TabMetrics event has been logged for |source_url|
  // with the expected metrics and the next available SequenceId.
  void ExpectNewEntry(const GURL& source_url,
                      const UkmMetricMap& expected_metrics) {
    ukm_entry_checker_.ExpectNewEntry(kEntryName, source_url, expected_metrics);

    const size_t num_entries = ukm_entry_checker_.NumEntries(kEntryName);
    EXPECT_EQ(num_entries, ++num_previous_entries);
  }

 protected:
  const char* kEntryName = TabManager_TabMetrics::kEntryName;
  size_t num_previous_entries = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(TabMetricsTest);
};

TEST_F(TabMetricsTest, Basic) {
  Browser::CreateParams params(profile(), true);
  std::unique_ptr<Browser> browser =
      CreateBrowserWithTestWindowForParams(&params);

  TabStripModel* tab_strip_model = browser->tab_strip_model();
  content::WebContents* fg_contents =
      tab_activity_simulator_.AddWebContentsAndNavigate(tab_strip_model,
                                                        GURL(kTestUrls[0]));
  tab_strip_model->ActivateTabAt(0);
  WebContentsTester::For(fg_contents)->TestSetIsLoading(false);

  // Adding, loading and activating a foreground tab doesn't trigger logging.
  EXPECT_EQ(0, ukm_entry_checker_.NumNewEntriesRecorded(kEntryName));

  // The second web contents is added as a background tab, so it logs an entry
  // when it stops loading.
  content::WebContents* bg_contents =
      tab_activity_simulator_.AddWebContentsAndNavigate(tab_strip_model,
                                                        GURL(kTestUrls[1]));
  WebContentsTester::For(bg_contents)->TestSetIsLoading(false);
  ExpectNewEntry(kTestUrls[1], kBasicMetricValues);

  // Activating a tab logs the deactivated tab.
  tab_activity_simulator_.SwitchToTabAt(tab_strip_model, 1);
  {
    SCOPED_TRACE("");
    ExpectNewEntry(kTestUrls[0], kBasicMetricValues);
  }

  tab_activity_simulator_.SwitchToTabAt(tab_strip_model, 0);
  {
    SCOPED_TRACE("");
    ExpectNewEntry(kTestUrls[1], kBasicMetricValues);
  }

  // Closing the tabs destroys the WebContentses but should not trigger logging.
  // The TestWebContentsObserver simulates hiding these tabs as they are closed;
  // we verify in TearDown() that no logging occurred.
  tab_strip_model->CloseAllTabs();
}

// Tests when tab events like pinning and navigating trigger logging.
TEST_F(TabMetricsTest, TabEvents) {
  Browser::CreateParams params(profile(), true);
  std::unique_ptr<Browser> browser =
      CreateBrowserWithTestWindowForParams(&params);

  TabStripModel* tab_strip_model = browser->tab_strip_model();
  content::WebContents* test_contents_1 =
      tab_activity_simulator_.AddWebContentsAndNavigate(tab_strip_model,
                                                        GURL(kTestUrls[0]));
  tab_strip_model->ActivateTabAt(0);

  // Opening the background tab triggers logging once the page finishes loading.
  content::WebContents* test_contents_2 =
      tab_activity_simulator_.AddWebContentsAndNavigate(tab_strip_model,
                                                        GURL(kTestUrls[1]));
  EXPECT_EQ(0, ukm_entry_checker_.NumNewEntriesRecorded(kEntryName));
  WebContentsTester::For(test_contents_2)->TestSetIsLoading(false);
  {
    SCOPED_TRACE("");
    ExpectNewEntry(GURL(kTestUrls[1]), kBasicMetricValues);
  }

  // Navigating the active tab doesn't trigger logging.
  WebContentsTester::For(test_contents_1)->NavigateAndCommit(kTestUrls[2]);
  EXPECT_EQ(0, ukm_entry_checker_.NumNewEntriesRecorded(kEntryName));

  // Pinning the active tab doesn't trigger logging.
  tab_strip_model->SetTabPinned(0, true);
  EXPECT_EQ(0, ukm_entry_checker_.NumNewEntriesRecorded(kEntryName));

  // Pinning and unpinning the background tab triggers logging.
  tab_strip_model->SetTabPinned(1, true);
  UkmMetricMap expected_metrics(kBasicMetricValues);
  expected_metrics[TabManager_TabMetrics::kIsPinnedName] = 1;
  {
    SCOPED_TRACE("");
    ExpectNewEntry(GURL(kTestUrls[1]), expected_metrics);
  }
  tab_strip_model->SetTabPinned(1, false);
  expected_metrics[TabManager_TabMetrics::kIsPinnedName] = 0;
  {
    SCOPED_TRACE("");
    ExpectNewEntry(GURL(kTestUrls[1]), kBasicMetricValues);
  }

  // Navigating the background tab triggers logging once the page finishes
  // loading.
  auto navigation = content::NavigationSimulator::CreateBrowserInitiated(
      kTestUrls[0], test_contents_2);
  navigation->SetKeepLoading(true);
  navigation->Commit();
  EXPECT_EQ(0, ukm_entry_checker_.NumNewEntriesRecorded(kEntryName));
  navigation->StopLoading();
  {
    SCOPED_TRACE("");
    ExpectNewEntry(GURL(kTestUrls[0]), kBasicMetricValues);
  }

  tab_strip_model->CloseAllTabs();
}

// Tests setting and changing tab metrics.
TEST_F(TabMetricsTest, TabMetrics) {
  Browser::CreateParams params(profile(), true);
  std::unique_ptr<Browser> browser =
      CreateBrowserWithTestWindowForParams(&params);

  TabStripModel* tab_strip_model = browser->tab_strip_model();
  content::WebContents* test_contents_1 =
      tab_activity_simulator_.AddWebContentsAndNavigate(tab_strip_model,
                                                        GURL(kTestUrls[0]));
  tab_strip_model->ActivateTabAt(0);

  // Expected metrics for tab event.
  UkmMetricMap expected_metrics(kBasicMetricValues);

  // Load background contents and verify UKM entry.
  content::WebContents* test_contents_2 =
      tab_activity_simulator_.AddWebContentsAndNavigate(tab_strip_model,
                                                        GURL(kTestUrls[1]));
  WebContentsTester::For(test_contents_2)->TestSetIsLoading(false);
  {
    SCOPED_TRACE("");
    ExpectNewEntry(kTestUrls[1], expected_metrics);
  }

  // Site engagement score should round down to the nearest 10.
  SiteEngagementService::Get(profile())->ResetBaseScoreForURL(kTestUrls[1], 45);
  expected_metrics[TabManager_TabMetrics::kSiteEngagementScoreName] = 40;

  auto* audible_helper_2 =
      RecentlyAudibleHelper::FromWebContents(test_contents_2);
  audible_helper_2->SetRecentlyAudibleForTesting();
  expected_metrics[TabManager_TabMetrics::kWasRecentlyAudibleName] = 1;

  // Pin the background tab to log an event. (This moves it to index 0.)
  tab_strip_model->SetTabPinned(1, true);
  expected_metrics[TabManager_TabMetrics::kIsPinnedName] = 1;
  {
    SCOPED_TRACE("");
    ExpectNewEntry(kTestUrls[1], expected_metrics);
  }

  // Unset WasRecentlyAudible and navigate the background tab to a new domain.
  // Site engagement score for the new domain is 0.
  audible_helper_2->SetNotRecentlyAudibleForTesting();
  expected_metrics[TabManager_TabMetrics::kWasRecentlyAudibleName] = 0;
  WebContentsTester::For(test_contents_2)->NavigateAndCommit(kTestUrls[2]);
  expected_metrics[TabManager_TabMetrics::kSiteEngagementScoreName] = 0;

  WebContentsTester::For(test_contents_2)->TestSetIsLoading(false);
  {
    SCOPED_TRACE("");
    ExpectNewEntry(kTestUrls[2], expected_metrics);
  }

  // Navigate the active tab and switch away from it. The entry should reflect
  // the new URL (even when the page hasn't finished loading).
  WebContentsTester::For(test_contents_1)->NavigateAndCommit(kTestUrls[2]);
  tab_activity_simulator_.SwitchToTabAt(tab_strip_model, 0);
  {
    SCOPED_TRACE("");
    // This tab still has the default metrics.
    ExpectNewEntry(kTestUrls[2], kBasicMetricValues);
  }

  tab_strip_model->CloseAllTabs();
}

// Tests counting input events. TODO(michaelpg): Currently only tests mouse
// events.
TEST_F(TabMetricsTest, InputEvents) {
  Browser::CreateParams params(profile(), true);
  std::unique_ptr<Browser> browser =
      CreateBrowserWithTestWindowForParams(&params);

  TabStripModel* tab_strip_model = browser->tab_strip_model();
  content::WebContents* test_contents_1 =
      tab_activity_simulator_.AddWebContentsAndNavigate(tab_strip_model,
                                                        GURL(kTestUrls[0]));
  content::WebContents* test_contents_2 =
      tab_activity_simulator_.AddWebContentsAndNavigate(tab_strip_model,
                                                        GURL(kTestUrls[1]));

  // RunUntilIdle is needed because the widget input handler is initialized
  // asynchronously via mojo (see SetupWidgetInputHandler).
  base::RunLoop().RunUntilIdle();
  tab_strip_model->ActivateTabAt(0);

  UkmMetricMap expected_metrics_1(kBasicMetricValues);
  UkmMetricMap expected_metrics_2(kBasicMetricValues);

  // Fake some input events.
  content::RenderWidgetHost* widget_1 =
      test_contents_1->GetRenderViewHost()->GetWidget();
  widget_1->ForwardMouseEvent(CreateMouseEvent(WebInputEvent::kMouseDown));
  widget_1->ForwardMouseEvent(CreateMouseEvent(WebInputEvent::kMouseUp));
  widget_1->ForwardMouseEvent(CreateMouseEvent(WebInputEvent::kMouseMove));
  expected_metrics_1[TabManager_TabMetrics::kMouseEventCountName] = 3;

  // Switch to the background tab. The current tab is deactivated and logged.
  tab_activity_simulator_.SwitchToTabAt(tab_strip_model, 1);
  {
    SCOPED_TRACE("");
    ExpectNewEntry(kTestUrls[0], expected_metrics_1);
  }

  // The second tab's counts are independent of the other's.
  content::RenderWidgetHost* widget_2 =
      test_contents_2->GetRenderViewHost()->GetWidget();
  widget_2->ForwardMouseEvent(CreateMouseEvent(WebInputEvent::kMouseMove));
  expected_metrics_2[TabManager_TabMetrics::kMouseEventCountName] = 1;

  // Switch back to the first tab to log the second tab.
  tab_activity_simulator_.SwitchToTabAt(tab_strip_model, 0);
  {
    SCOPED_TRACE("");
    ExpectNewEntry(kTestUrls[1], expected_metrics_2);
  }

  // New events are added to the first tab's existing counts.
  widget_1->ForwardMouseEvent(CreateMouseEvent(WebInputEvent::kMouseMove));
  widget_1->ForwardMouseEvent(CreateMouseEvent(WebInputEvent::kMouseMove));
  expected_metrics_1[TabManager_TabMetrics::kMouseEventCountName] = 5;
  tab_activity_simulator_.SwitchToTabAt(tab_strip_model, 1);
  {
    SCOPED_TRACE("");
    ExpectNewEntry(kTestUrls[0], expected_metrics_1);
  }
  tab_activity_simulator_.SwitchToTabAt(tab_strip_model, 0);
  {
    SCOPED_TRACE("");
    ExpectNewEntry(kTestUrls[1], expected_metrics_2);
  }

  // After a navigation, test that the counts are reset.
  WebContentsTester::For(test_contents_1)->NavigateAndCommit(kTestUrls[2]);
  // The widget may have been invalidated by the navigation.
  widget_1 = test_contents_1->GetRenderViewHost()->GetWidget();
  widget_1->ForwardMouseEvent(CreateMouseEvent(WebInputEvent::kMouseMove));
  expected_metrics_1[TabManager_TabMetrics::kMouseEventCountName] = 1;
  tab_activity_simulator_.SwitchToTabAt(tab_strip_model, 1);
  {
    SCOPED_TRACE("");
    ExpectNewEntry(kTestUrls[2], expected_metrics_1);
  }

  tab_strip_model->CloseAllTabs();
}

// Tests that logging doesn't occur when the WebContents is hidden while still
// the active tab, e.g. when the browser window hides before closing.
// Flaky on chromeos: https://crbug.com/923147
TEST_F(TabMetricsTest, DISABLED_HideWebContents) {
  Browser::CreateParams params(profile(), true);
  std::unique_ptr<Browser> browser =
      CreateBrowserWithTestWindowForParams(&params);

  TabStripModel* tab_strip_model = browser->tab_strip_model();
  content::WebContents* test_contents =
      tab_activity_simulator_.AddWebContentsAndNavigate(tab_strip_model,
                                                        GURL(kTestUrls[0]));
  tab_strip_model->ActivateTabAt(0);

  // Hiding the window doesn't trigger a log entry, unless the window was
  // minimized.
  // TODO(michaelpg): Test again with the window minimized using the
  // FakeBrowserWindow from window_activity_watcher_unittest.cc.
  test_contents->WasHidden();
  EXPECT_EQ(0, ukm_entry_checker_.NumNewEntriesRecorded(kEntryName));
  test_contents->WasShown();
  EXPECT_EQ(0, ukm_entry_checker_.NumNewEntriesRecorded(kEntryName));

  tab_strip_model->CloseAllTabs();
}

// Tests navigation-related metrics.
TEST_F(TabMetricsTest, Navigations) {
  Browser::CreateParams params(profile(), true);
  auto browser = CreateBrowserWithTestWindowForParams(&params);
  TabStripModel* tab_strip_model = browser->tab_strip_model();

  // Set up first tab.
  tab_activity_simulator_.AddWebContentsAndNavigate(tab_strip_model,
                                                    GURL(kTestUrls[0]));
  tab_strip_model->ActivateTabAt(0);

  // Expected metrics for tab event.
  UkmMetricMap expected_metrics(kBasicMetricValues);

  // Load background contents and verify UKM entry.
  content::WebContents* test_contents =
      tab_activity_simulator_.AddWebContentsAndNavigate(
          tab_strip_model, GURL(kTestUrls[1]),
          ui::PageTransitionFromInt(ui::PAGE_TRANSITION_TYPED |
                                    ui::PAGE_TRANSITION_FROM_ADDRESS_BAR));
  WebContentsTester::For(test_contents)->TestSetIsLoading(false);
  expected_metrics[TabManager_TabMetrics::kPageTransitionCoreTypeName] =
      base::nullopt;
  expected_metrics[TabManager_TabMetrics::kPageTransitionFromAddressBarName] =
      true;
  expected_metrics[TabManager_TabMetrics::kPageTransitionIsRedirectName] =
      false;
  expected_metrics[TabManager_TabMetrics::kNavigationEntryCountName] = 1;
  {
    SCOPED_TRACE("");
    ExpectNewEntry(kTestUrls[1], expected_metrics);
  }

  // Navigate background tab (not all transition types make sense in the
  // background, but this is simpler than juggling two tabs to trigger logging).
  tab_activity_simulator_.Navigate(test_contents, kTestUrls[2],
                                   ui::PAGE_TRANSITION_LINK);
  WebContentsTester::For(test_contents)->TestSetIsLoading(false);
  expected_metrics[TabManager_TabMetrics::kPageTransitionCoreTypeName] =
      ui::PAGE_TRANSITION_LINK;
  expected_metrics[TabManager_TabMetrics::kPageTransitionFromAddressBarName] =
      false;
  expected_metrics[TabManager_TabMetrics::kPageTransitionIsRedirectName] =
      false;
  expected_metrics[TabManager_TabMetrics::kNavigationEntryCountName].value()++;
  {
    SCOPED_TRACE("");
    ExpectNewEntry(kTestUrls[2], expected_metrics);
  }

  tab_activity_simulator_.Navigate(
      test_contents, kTestUrls[0],
      ui::PageTransitionFromInt(ui::PAGE_TRANSITION_LINK |
                                ui::PAGE_TRANSITION_SERVER_REDIRECT));
  WebContentsTester::For(test_contents)->TestSetIsLoading(false);
  expected_metrics[TabManager_TabMetrics::kPageTransitionCoreTypeName] =
      ui::PAGE_TRANSITION_LINK;
  expected_metrics[TabManager_TabMetrics::kPageTransitionFromAddressBarName] =
      false;
  expected_metrics[TabManager_TabMetrics::kPageTransitionIsRedirectName] = true;
  expected_metrics[TabManager_TabMetrics::kNavigationEntryCountName].value()++;
  {
    SCOPED_TRACE("");
    ExpectNewEntry(kTestUrls[0], expected_metrics);
  }

  tab_activity_simulator_.Navigate(test_contents, kTestUrls[0],
                                   ui::PAGE_TRANSITION_RELOAD);
  WebContentsTester::For(test_contents)->TestSetIsLoading(false);
  expected_metrics[TabManager_TabMetrics::kPageTransitionCoreTypeName] =
      ui::PAGE_TRANSITION_RELOAD;
  expected_metrics[TabManager_TabMetrics::kNavigationEntryCountName].value()++;
  expected_metrics[TabManager_TabMetrics::kPageTransitionFromAddressBarName] =
      false;
  expected_metrics[TabManager_TabMetrics::kPageTransitionIsRedirectName] =
      false;
  {
    SCOPED_TRACE("");
    ExpectNewEntry(kTestUrls[0], expected_metrics);
  }

  tab_activity_simulator_.Navigate(test_contents, kTestUrls[1],
                                   ui::PAGE_TRANSITION_AUTO_BOOKMARK);
  WebContentsTester::For(test_contents)->TestSetIsLoading(false);
  expected_metrics[TabManager_TabMetrics::kPageTransitionCoreTypeName] =
      ui::PAGE_TRANSITION_AUTO_BOOKMARK;
  // FromAddressBar and IsRedirect should still be false, no need to update
  // their values in |expected_metrics|.
  expected_metrics[TabManager_TabMetrics::kNavigationEntryCountName].value()++;
  {
    SCOPED_TRACE("");
    ExpectNewEntry(kTestUrls[1], expected_metrics);
  }

  tab_activity_simulator_.Navigate(test_contents, kTestUrls[1],
                                   ui::PAGE_TRANSITION_FORM_SUBMIT);
  WebContentsTester::For(test_contents)->TestSetIsLoading(false);
  expected_metrics[TabManager_TabMetrics::kPageTransitionCoreTypeName] =
      ui::PAGE_TRANSITION_FORM_SUBMIT;
  expected_metrics[TabManager_TabMetrics::kNavigationEntryCountName].value()++;
  {
    SCOPED_TRACE("");
    ExpectNewEntry(kTestUrls[1], expected_metrics);
  }

  // Test non-reportable core type.
  tab_activity_simulator_.Navigate(
      test_contents, kTestUrls[0],
      ui::PageTransitionFromInt(ui::PAGE_TRANSITION_KEYWORD |
                                ui::PAGE_TRANSITION_FROM_ADDRESS_BAR));
  WebContentsTester::For(test_contents)->TestSetIsLoading(false);
  expected_metrics[TabManager_TabMetrics::kPageTransitionCoreTypeName] =
      base::nullopt;
  expected_metrics[TabManager_TabMetrics::kPageTransitionFromAddressBarName] =
      true;
  expected_metrics[TabManager_TabMetrics::kNavigationEntryCountName].value()++;
  {
    SCOPED_TRACE("");
    ExpectNewEntry(kTestUrls[0], expected_metrics);
  }

  tab_strip_model->CloseAllTabs();
}

// Tests that replacing a foreground tab doesn't log new tab metrics until the
// new tab is backgrounded.
TEST_F(TabMetricsTest, ReplaceForegroundTab) {
  Browser::CreateParams params(profile(), true);
  std::unique_ptr<Browser> browser =
      CreateBrowserWithTestWindowForParams(&params);

  TabStripModel* tab_strip_model = browser->tab_strip_model();
  content::WebContents* orig_contents =
      tab_activity_simulator_.AddWebContentsAndNavigate(tab_strip_model,
                                                        GURL(kTestUrls[0]));
  tab_strip_model->ActivateTabAt(0);
  WebContentsTester::For(orig_contents)->TestSetIsLoading(false);

  // Build the replacement contents.
  std::unique_ptr<content::WebContents> new_contents =
      tab_activity_simulator_.CreateWebContents(profile());

  // Ensure the test URL gets a UKM source ID upon navigating.
  // Normally this happens when the browser or prerenderer attaches tab helpers.
  ukm::InitializeSourceUrlRecorderForWebContents(new_contents.get());

  tab_activity_simulator_.Navigate(new_contents.get(), GURL(kTestUrls[1]));
  WebContentsTester::For(new_contents.get())->TestSetIsLoading(false);

  // Replace and delete the old contents.
  std::unique_ptr<content::WebContents> old_contents =
      tab_strip_model->ReplaceWebContentsAt(0, std::move(new_contents));
  ASSERT_EQ(old_contents.get(), orig_contents);
  old_contents.reset();
  tab_strip_model->GetWebContentsAt(0)->WasShown();

  EXPECT_EQ(0, ukm_entry_checker_.NumNewEntriesRecorded(kEntryName));

  // Add a new tab so the first tab is backgrounded.
  tab_activity_simulator_.AddWebContentsAndNavigate(tab_strip_model,
                                                    GURL(kTestUrls[2]));
  tab_activity_simulator_.SwitchToTabAt(tab_strip_model, 1);
  {
    SCOPED_TRACE("");
    // Replaced tab uses the orig source_id; so the metrics is logged to
    // kTestUrls[0].
    ExpectNewEntry(kTestUrls[0], kBasicMetricValues);
  }

  tab_strip_model->CloseAllTabs();
}

// Tests TabManager.Background.ForegroundedOrClosed UKMs logged by
// TabActivityWatcher.
class ForegroundedOrClosedTest : public TabActivityWatcherTest {
 public:
  ForegroundedOrClosedTest()
      : scoped_set_tick_clock_for_testing_(&test_clock_) {
    // Start at a nonzero time.
    AdvanceClock();
    SetParams({{"scorer_type", "0"},
               {"disable_background_log_with_TabRanker", "false"}});
  }
  ~ForegroundedOrClosedTest() override = default;

 protected:
  const char* kEntryName = ForegroundedOrClosed::kEntryName;

  void AdvanceClock() { test_clock_.Advance(base::TimeDelta::FromSeconds(1)); }

 private:
  base::SimpleTestTickClock test_clock_;
  resource_coordinator::ScopedSetTickClockForTesting
      scoped_set_tick_clock_for_testing_;

  DISALLOW_COPY_AND_ASSIGN(ForegroundedOrClosedTest);
};

// Tests TabManager.Backgrounded.ForegroundedOrClosed UKM logging.
// Flaky on ChromeOS. http://crbug.com/924864
#if defined(OS_CHROMEOS)
#define MAYBE_SingleTab DISABLED_SingleTab
#else
#define MAYBE_SingleTab SingleTab
#endif
TEST_F(ForegroundedOrClosedTest, MAYBE_SingleTab) {
  Browser::CreateParams params(profile(), true);
  std::unique_ptr<Browser> browser =
      CreateBrowserWithTestWindowForParams(&params);

  TabStripModel* tab_strip_model = browser->tab_strip_model();
  tab_activity_simulator_.AddWebContentsAndNavigate(tab_strip_model,
                                                    GURL(kTestUrls[0]));

  // The tab is in the foreground, so it isn't logged as a background tab.
  tab_strip_model->CloseWebContentsAt(0, TabStripModel::CLOSE_USER_GESTURE);
  EXPECT_EQ(0, ukm_entry_checker_.NumNewEntriesRecorded(kEntryName));
}

// Tests TabManager.Backgrounded.ForegroundedOrClosed UKM logging.
TEST_F(ForegroundedOrClosedTest, MultipleTabs) {
  Browser::CreateParams params(profile(), true);
  std::unique_ptr<Browser> browser =
      CreateBrowserWithTestWindowForParams(&params);

  TabStripModel* tab_strip_model = browser->tab_strip_model();
  tab_activity_simulator_.AddWebContentsAndNavigate(tab_strip_model,
                                                    GURL(kTestUrls[0]));
  tab_strip_model->ActivateTabAt(0);
  tab_activity_simulator_.AddWebContentsAndNavigate(tab_strip_model,
                                                    GURL(kTestUrls[1]));
  AdvanceClock();
  tab_activity_simulator_.AddWebContentsAndNavigate(tab_strip_model,
                                                    GURL(kTestUrls[2]));
  AdvanceClock();
  // MRU ordering by tab indices:
  // 0 (foreground), 2 (created last), 1 (created first),

  // Foreground a tab to log an event.
  tab_activity_simulator_.SwitchToTabAt(tab_strip_model, 2);
  {
    SCOPED_TRACE("");
    ukm_entry_checker_.ExpectNewEntry(
        kEntryName, kTestUrls[2],
        {
            {ForegroundedOrClosed::kIsForegroundedName, 1},
        });
  }
  AdvanceClock();
  // MRU ordering by tab indices:
  // 2 (foreground), 0 (foregrounded earlier), 1 (never foregrounded)

  // Foreground the middle tab to log another event.
  tab_activity_simulator_.SwitchToTabAt(tab_strip_model, 1);
  {
    SCOPED_TRACE("");
    ukm_entry_checker_.ExpectNewEntry(
        kEntryName, kTestUrls[1],
        {
            {ForegroundedOrClosed::kIsForegroundedName, 1},
        });
  }
  AdvanceClock();
  // MRU ordering by tab indices:
  // 1 (foreground), 2 (foregrounded earlier), 0 (foregrounded even earlier)

  // Close all tabs. Background tabs are logged as closed.
  tab_strip_model->CloseAllTabs();
  {
    SCOPED_TRACE("");
    // The rightmost tab was in the background and was closed.
    ukm_entry_checker_.ExpectNewEntry(
        kEntryName, kTestUrls[2],
        {
            {ForegroundedOrClosed::kIsForegroundedName, 0},
        });

    // The leftmost tab was in the background and was closed.
    ukm_entry_checker_.ExpectNewEntry(
        kEntryName, kTestUrls[0],
        {
            {ForegroundedOrClosed::kIsForegroundedName, 0},
        });

    // No event is logged for the middle tab, which was in the foreground.
    EXPECT_EQ(0, ukm_entry_checker_.NumNewEntriesRecorded(kEntryName));
  }
}

}  // namespace resource_coordinator
