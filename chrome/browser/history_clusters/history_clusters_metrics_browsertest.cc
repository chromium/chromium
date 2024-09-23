// Copyright 2021 The Chromium Authors
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
#include "chrome/browser/ui/webui/history_clusters/history_clusters_handler.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/history_clusters/core/features.h"
#include "components/history_clusters/core/url_constants.h"
#include "components/ukm/test_ukm_recorder.h"
#include "components/variations/active_field_trials.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_source.h"
#include "ui/webui/resources/cr_components/history_clusters/history_clusters.mojom.h"

namespace history_clusters {

namespace {

enum class UiTab {
  kBasicHistory = 0,
  kClustersUi = 1,
};

void ValidateHistoryClustersUKMEntry(const ukm::mojom::UkmEntry* entry,
                                     HistoryClustersInitialState init_state,
                                     int num_queries,
                                     int num_toggles_to_basic_history) {
  ukm::TestAutoSetUkmRecorder ukm_recorder;
  EXPECT_TRUE(ukm_recorder.EntryHasMetric(
      entry, ukm::builders::HistoryClusters::kInitialStateName));
  ukm_recorder.ExpectEntryMetric(
      entry, ukm::builders::HistoryClusters::kInitialStateName,
      static_cast<int>(init_state));
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

  // Toggle to the specified `tab`, either the basic history (0) or clusters UI
  // (1).
  void ToggleToUi(UiTab tab) {
    std::string tab_string = tab == UiTab::kClustersUi ? "1" : "0";
    std::string execute_string = "";
    execute_string += R"(
        import('chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js')
          .then((polymerModule)=> {
            polymerModule.flush();
            const historyApp = document.querySelector('#history-app');
            const tab = )" +
                      tab_string + R"(;
            historyApp.shadowRoot.querySelector('cr-tabs').selected = tab;
            return true;
          });)";
    EXPECT_EQ(true, content::EvalJs(
                        browser()->tab_strip_model()->GetActiveWebContents(),
                        execute_string));
  }

  // Creates and follows an anchor link. Since we can't differentiate between
  // that and actual visit links, it'll log the final state as `kLinkClick`,
  // which is useful since the browser tests won't populate journeys, and we
  // have no other way to trigger `kLinkClick`.
  void FollowBrowserManagedLink() {
    std::string execute_string = "";
    execute_string += R"(
        import('chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js')
          .then((polymerModule)=> {
            polymerModule.flush();
            let link = document.createElement('a');
            link.href = 'https://google.com';
            link.click();
            return true;
          });)";
    EXPECT_EQ(true, content::EvalJs(
                        browser()->tab_strip_model()->GetActiveWebContents(),
                        execute_string));
  }

  // Navigates to the history clusters UI with `PAGE_TRANSITION_RELOAD`. Assumes
  // the current URL is also the history clusters UI.
  void RefreshHistoryClusters() {
    NavigateParams params(browser(), GURL(GetChromeUIHistoryClustersURL()),
                          ui::PAGE_TRANSITION_RELOAD);
    ui_test_utils::NavigateToURL(&params);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(HistoryClustersMetricsBrowserTest,
                       NoUKMEventOnOtherPages) {
  base::HistogramTester histogram_tester;
  ukm::TestAutoSetUkmRecorder ukm_recorder;
  // Navigate to and away from a site. The UKM events are recorded when leaving
  // a page.
  EXPECT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL("https://foo.com")));
  EXPECT_TRUE(
      ui_test_utils::NavigateToURL(browser(), GURL("https://foo2.com")));
  auto entries =
      ukm_recorder.GetEntriesByName(ukm::builders::HistoryClusters::kEntryName);
  EXPECT_EQ(0u, entries.size());
  histogram_tester.ExpectTotalCount("History.Clusters.Actions.DidMakeQuery", 0);
  histogram_tester.ExpectTotalCount("History.Clusters.Actions.NumQueries", 0);
}

// Flaky on Win, Linux and Mac. http://crbug.com/1282122
#if BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_MAC)
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
      browser(), GURL(GetChromeUIHistoryClustersURL())));
  EXPECT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL("https://foo.com")));
  auto entries =
      ukm_recorder.GetEntriesByName(ukm::builders::HistoryClusters::kEntryName);
  EXPECT_EQ(1u, entries.size());
  auto* entry = entries[0].get();
  ValidateHistoryClustersUKMEntry(
      entry, HistoryClustersInitialState::kDirectNavigation, 0, 0);
  histogram_tester.ExpectUniqueSample(
      "History.Clusters.Actions.InitialState",
      HistoryClustersInitialState::kDirectNavigation, 1);
  histogram_tester.ExpectUniqueSample("History.Clusters.Actions.DidMakeQuery",
                                      false, 1);
  histogram_tester.ExpectTotalCount("History.Clusters.Actions.NumQueries", 0);
}

// TODO(crbug.com/40812616): Flaky on Linux, Windows and Mac.
#if BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_WIN) || \
    BUILDFLAG(IS_MAC)
#define MAYBE_DirectNavigationWithQuery DISABLED_DirectNavigationWithQuery
#else
#define MAYBE_DirectNavigationWithQuery DirectNavigationWithQuery
#endif
IN_PROC_BROWSER_TEST_F(HistoryClustersMetricsBrowserTest,
                       MAYBE_DirectNavigationWithQuery) {
  base::HistogramTester histogram_tester;
  ukm::TestAutoSetUkmRecorder ukm_recorder;

  EXPECT_TRUE(ui_test_utils::NavigateToURL(
      browser(), GURL(GetChromeUIHistoryClustersURL())));
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

  page_handler->StartQueryClusters("cat", std::nullopt, false);
  page_handler->StartQueryClusters("dog", std::nullopt, false);

  EXPECT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL("https://foo.com")));
  auto entries =
      ukm_recorder.GetEntriesByName(ukm::builders::HistoryClusters::kEntryName);
  EXPECT_EQ(1u, entries.size());
  auto* entry = entries[0].get();
  ValidateHistoryClustersUKMEntry(
      entry, HistoryClustersInitialState::kDirectNavigation, 2, 0);
  histogram_tester.ExpectUniqueSample(
      "History.Clusters.Actions.InitialState",
      HistoryClustersInitialState::kDirectNavigation, 1);
  histogram_tester.ExpectUniqueSample("History.Clusters.Actions.DidMakeQuery",
                                      true, 1);
  histogram_tester.ExpectUniqueSample("History.Clusters.Actions.NumQueries", 2,
                                      1);
}

// Disabled on Windows, ChromeOS, and Linux due to flakes: crbug.com/1263465.
// Disabled on Mac due to flakes: crbug.com/1288805.
#if BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_MAC)
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
      browser(), GURL(GetChromeUIHistoryClustersURL())));
  ToggleToUi(UiTab::kBasicHistory);

  EXPECT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL("https://foo.com")));
  auto entries =
      ukm_recorder.GetEntriesByName(ukm::builders::HistoryClusters::kEntryName);
  EXPECT_EQ(1u, entries.size());
  auto* ukm_entry = entries[0].get();
  ValidateHistoryClustersUKMEntry(
      ukm_entry, HistoryClustersInitialState::kDirectNavigation, 0, 1);
  histogram_tester.ExpectUniqueSample(
      "History.Clusters.Actions.InitialState",
      HistoryClustersInitialState::kDirectNavigation, 1);
  histogram_tester.ExpectUniqueSample("History.Clusters.Actions.DidMakeQuery",
                                      false, 1);
  histogram_tester.ExpectTotalCount("History.Clusters.Actions.NumQueries", 0);
}

IN_PROC_BROWSER_TEST_F(
    HistoryClustersMetricsBrowserTest,
    DISABLED_DirectNavigationWithToggleToBasicAndToggleBack) {
  base::HistogramTester histogram_tester;
  ukm::TestAutoSetUkmRecorder ukm_recorder;

  EXPECT_TRUE(ui_test_utils::NavigateToURL(
      browser(), GURL(GetChromeUIHistoryClustersURL())));
  ToggleToUi(UiTab::kBasicHistory);
  ToggleToUi(UiTab::kClustersUi);

  EXPECT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL("https://foo.com")));
  auto entries =
      ukm_recorder.GetEntriesByName(ukm::builders::HistoryClusters::kEntryName);
  EXPECT_EQ(1u, entries.size());
  auto* ukm_entry = entries[0].get();
  ValidateHistoryClustersUKMEntry(
      ukm_entry, HistoryClustersInitialState::kIndirectNavigation, 0, 1);
  histogram_tester.ExpectUniqueSample(
      "History.Clusters.Actions.InitialState",
      HistoryClustersInitialState::kIndirectNavigation, 1);
  histogram_tester.ExpectUniqueSample("History.Clusters.Actions.DidMakeQuery",
                                      false, 1);
  histogram_tester.ExpectTotalCount("History.Clusters.Actions.NumQueries", 0);
}

// Assumed to be flaky since the above tests are flaky.
#if BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_WIN) || \
    BUILDFLAG(IS_MAC)
#define MAYBE_IndirectNavigation DISABLED_IndirectNavigation
#else
#define MAYBE_IndirectNavigation IndirectNavigation
#endif
IN_PROC_BROWSER_TEST_F(HistoryClustersMetricsBrowserTest,
                       MAYBE_IndirectNavigation) {
  base::HistogramTester histogram_tester;
  ukm::TestAutoSetUkmRecorder ukm_recorder;

  EXPECT_TRUE(ui_test_utils::NavigateToURL(browser(),
                                           GURL(chrome::kChromeUIHistoryURL)));
  ToggleToUi(UiTab::kClustersUi);

  EXPECT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL("https://foo.com")));
  auto entries =
      ukm_recorder.GetEntriesByName(ukm::builders::HistoryClusters::kEntryName);
  EXPECT_EQ(1u, entries.size());
  auto* ukm_entry = entries[0].get();
  ValidateHistoryClustersUKMEntry(
      ukm_entry, HistoryClustersInitialState::kIndirectNavigation, 0, 0);
  histogram_tester.ExpectUniqueSample(
      "History.Clusters.Actions.InitialState",
      HistoryClustersInitialState::kIndirectNavigation, 1);
  histogram_tester.ExpectUniqueSample("History.Clusters.Actions.DidMakeQuery",
                                      false, 1);
  histogram_tester.ExpectTotalCount("History.Clusters.Actions.NumQueries", 0);
}

}  // namespace history_clusters
