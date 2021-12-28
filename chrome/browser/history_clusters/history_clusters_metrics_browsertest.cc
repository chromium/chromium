// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/base64.h"
#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/history/history_test_utils.h"
#include "chrome/browser/history_clusters/history_clusters_metrics_logger.h"
#include "chrome/browser/history_clusters/history_clusters_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/webui/history/history_ui.h"
#include "chrome/browser/ui/webui/history_clusters/history_clusters.mojom.h"
#include "chrome/browser/ui/webui/history_clusters/history_clusters_handler.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/history_clusters/core/features.h"
#include "components/ukm/test_ukm_recorder.h"
#include "components/variations/active_field_trials.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_source.h"

namespace history_clusters {

namespace {

void ValidateHistoryClustersUKMEntry(const ukm::mojom::UkmEntry* entry,
                                     HistoryClustersInitialState init_state,
                                     HistoryClustersFinalState final_state,
                                     int num_queries,
                                     int num_toggles_to_basic_history) {
  ukm::TestAutoSetUkmRecorder ukm_recorder;
  EXPECT_TRUE(ukm_recorder.EntryHasMetric(
      entry, ukm::builders::HistoryClusters::kInitialStateName));
  ukm_recorder.ExpectEntryMetric(
      entry, ukm::builders::HistoryClusters::kInitialStateName,
      static_cast<int>(init_state));
  EXPECT_TRUE(ukm_recorder.EntryHasMetric(
      entry, ukm::builders::HistoryClusters::kFinalStateName));
  ukm_recorder.ExpectEntryMetric(
      entry, ukm::builders::HistoryClusters::kFinalStateName,
      static_cast<int>(final_state));
  EXPECT_TRUE(ukm_recorder.EntryHasMetric(
      entry, ukm::builders::HistoryClusters::kNumQueriesName));
  ukm_recorder.ExpectEntryMetric(
      entry, ukm::builders::HistoryClusters::kNumQueriesName, num_queries);
  EXPECT_TRUE(ukm_recorder.EntryHasMetric(
      entry, ukm::builders::HistoryClusters::kNumTogglesToBasicHistoryName));
  ukm_recorder.ExpectEntryMetric(
      entry, ukm::builders::HistoryClusters::kNumTogglesToBasicHistoryName,
      num_toggles_to_basic_history);
}

}  // namespace

class HistoryClustersMetricsBrowserTest : public InProcessBrowserTest {
 public:
  HistoryClustersMetricsBrowserTest() {
    feature_list_.InitWithFeatures({history_clusters::internal::kJourneys}, {});
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(HistoryClustersMetricsBrowserTest,
                       NoUKMEventOnOtherPages) {
  ukm::TestAutoSetUkmRecorder ukm_recorder;
  EXPECT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL("https://foo.com")));
  auto entries =
      ukm_recorder.GetEntriesByName(ukm::builders::HistoryClusters::kEntryName);
  EXPECT_EQ(0u, entries.size());
}

// Flaky on Win and Linux. http://crbug.com/1282122
#if defined(OS_WIN) || defined(OS_LINUX)
#define MAYBE_DirectNavigationNoInteraction \
  DISABLED_DirectNavigationNoInteraction
#else
#define MAYBE_DirectNavigationNoInteraction DirectNavigationNoInteraction
#endif
IN_PROC_BROWSER_TEST_F(HistoryClustersMetricsBrowserTest,
                       MAYBE_DirectNavigationNoInteraction) {
  base::HistogramTester histogram_tester;
  ukm::TestAutoSetUkmRecorder ukm_recorder;
  EXPECT_TRUE(ui_test_utils::NavigateToURL(
      browser(), GURL(chrome::kChromeUIHistoryClustersURL)));
  EXPECT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL("https://foo.com")));
  auto entries =
      ukm_recorder.GetEntriesByName(ukm::builders::HistoryClusters::kEntryName);
  EXPECT_EQ(1u, entries.size());
  auto* entry = entries[0];
  ValidateHistoryClustersUKMEntry(
      entry, HistoryClustersInitialState::kDirectNavigation,
      HistoryClustersFinalState::kCloseTab, 0, 0);
  histogram_tester.ExpectUniqueSample(
      "History.Clusters.Actions.InitialState",
      HistoryClustersInitialState::kDirectNavigation, 1);
  histogram_tester.ExpectUniqueSample("History.Clusters.Actions.FinalState",
                                      HistoryClustersFinalState::kCloseTab, 1);
  histogram_tester.ExpectUniqueSample("History.Clusters.Actions.DidMakeQuery",
                                      false, 1);
  histogram_tester.ExpectTotalCount("History.Clusters.Actions.NumQueries", 0);
}

// TODO(crbug.com/1282087): Flaky on Linux and Windows.
#if defined(OS_LINUX) || defined(OS_WIN)
#define MAYBE_DirectNavigationWithQuery DISABLED_DirectNavigationWithQuery
#else
#define MAYBE_DirectNavigationWithQuery DirectNavigationWithQuery
#endif
IN_PROC_BROWSER_TEST_F(HistoryClustersMetricsBrowserTest,
                       MAYBE_DirectNavigationWithQuery) {
  base::HistogramTester histogram_tester;
  ukm::TestAutoSetUkmRecorder ukm_recorder;

  EXPECT_TRUE(ui_test_utils::NavigateToURL(
      browser(), GURL(chrome::kChromeUIHistoryClustersURL)));
  EXPECT_TRUE(content::WaitForLoadStop(
      browser()->tab_strip_model()->GetActiveWebContents()));
  history_clusters::HistoryClustersHandler* page_handler =
      browser()
          ->tab_strip_model()
          ->GetActiveWebContents()
          ->GetWebUI()
          ->GetController()
          ->template GetAs<HistoryUI>()
          ->GetHistoryClustersHandlerForTesting();

  auto query_params = history_clusters::mojom::QueryParams::New();
  query_params->query = "cat";
  page_handler->QueryClusters(std::move(query_params));
  query_params = history_clusters::mojom::QueryParams::New();
  query_params->query = "dog";
  page_handler->QueryClusters(std::move(query_params));

  EXPECT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL("https://foo.com")));
  auto entries =
      ukm_recorder.GetEntriesByName(ukm::builders::HistoryClusters::kEntryName);
  EXPECT_EQ(1u, entries.size());
  auto* entry = entries[0];
  ValidateHistoryClustersUKMEntry(
      entry, HistoryClustersInitialState::kDirectNavigation,
      HistoryClustersFinalState::kCloseTab, 2, 0);
  histogram_tester.ExpectUniqueSample(
      "History.Clusters.Actions.InitialState",
      HistoryClustersInitialState::kDirectNavigation, 1);
  histogram_tester.ExpectUniqueSample("History.Clusters.Actions.FinalState",
                                      HistoryClustersFinalState::kCloseTab, 1);
  histogram_tester.ExpectUniqueSample("History.Clusters.Actions.DidMakeQuery",
                                      true, 1);
  histogram_tester.ExpectUniqueSample("History.Clusters.Actions.NumQueries", 2,
                                      1);
}

// Disabled on Windows, ChromeOS, and Linux due to flakes: crbug.com/1263465.
#if defined(OS_CHROMEOS) || defined(OS_WIN) || defined(OS_LINUX)
#define MAYBE_DirectNavigationWithToggleToBasic \
  DISABLED_DirectNavigationWithToggleToBasic
#else
#define MAYBE_DirectNavigationWithToggleToBasic \
  DirectNavigationWithToggleToBasic
#endif
IN_PROC_BROWSER_TEST_F(HistoryClustersMetricsBrowserTest,
                       MAYBE_DirectNavigationWithToggleToBasic) {
  base::HistogramTester histogram_tester;
  ukm::TestAutoSetUkmRecorder ukm_recorder;

  EXPECT_TRUE(ui_test_utils::NavigateToURL(
      browser(), GURL(chrome::kChromeUIHistoryClustersURL)));
  bool toggled_to_basic = false;
  EXPECT_TRUE(content::ExecuteScriptAndExtractBool(
      browser()->tab_strip_model()->GetActiveWebContents(), R"(
        const polymerPath =
            'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
        import(polymerPath).then((polymerModule)=> {
          polymerModule.flush();
          const historyApp = document.querySelector('#history-app');
          historyApp.shadowRoot.querySelector('cr-tabs').selected = 0;
          window.domAutomationController.send(true);
        });)",
      &toggled_to_basic));
  EXPECT_TRUE(toggled_to_basic);

  EXPECT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL("https://foo.com")));
  auto entries =
      ukm_recorder.GetEntriesByName(ukm::builders::HistoryClusters::kEntryName);
  EXPECT_EQ(1u, entries.size());
  auto* ukm_entry = entries[0];
  ValidateHistoryClustersUKMEntry(
      ukm_entry, HistoryClustersInitialState::kDirectNavigation,
      HistoryClustersFinalState::kCloseTab, 0, 1);
  histogram_tester.ExpectUniqueSample(
      "History.Clusters.Actions.InitialState",
      HistoryClustersInitialState::kDirectNavigation, 1);
  histogram_tester.ExpectUniqueSample("History.Clusters.Actions.FinalState",
                                      HistoryClustersFinalState::kCloseTab, 1);
  histogram_tester.ExpectUniqueSample("History.Clusters.Actions.DidMakeQuery",
                                      false, 1);
  histogram_tester.ExpectTotalCount("History.Clusters.Actions.NumQueries", 0);
}

// TODO(manukh): Adjust the expectations for the navigation tests.
IN_PROC_BROWSER_TEST_F(HistoryClustersMetricsBrowserTest,
                       DISABLED_IndirectNavigation) {
  base::HistogramTester histogram_tester;
  ukm::TestAutoSetUkmRecorder ukm_recorder;

  EXPECT_TRUE(ui_test_utils::NavigateToURL(browser(),
                                           GURL(chrome::kChromeUIHistoryURL)));
  EXPECT_TRUE(content::ExecJs(
      browser()->tab_strip_model()->GetActiveWebContents(),
      "document.querySelector('#history-app').shadowRoot.querySelector('#"
      "content-side-bar').shadowRoot.querySelector('#historyClusters').click()",
      content::EXECUTE_SCRIPT_DEFAULT_OPTIONS, 1 /* world_id */));

  EXPECT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL("https://foo.com")));
  auto entries =
      ukm_recorder.GetEntriesByName(ukm::builders::HistoryClusters::kEntryName);
  EXPECT_EQ(1u, entries.size());
  auto* ukm_entry = entries[0];
  ValidateHistoryClustersUKMEntry(
      ukm_entry, HistoryClustersInitialState::kIndirectNavigation,
      HistoryClustersFinalState::kCloseTab, 0, 0);
  histogram_tester.ExpectUniqueSample(
      "History.Clusters.Actions.InitialState",
      HistoryClustersInitialState::kIndirectNavigation, 1);
  histogram_tester.ExpectUniqueSample("History.Clusters.Actions.FinalState",
                                      HistoryClustersFinalState::kCloseTab, 1);
  histogram_tester.ExpectUniqueSample("History.Clusters.Actions.DidMakeQuery",
                                      false, 1);
  histogram_tester.ExpectTotalCount("History.Clusters.Actions.NumQueries", 0);
}

}  // namespace history_clusters
