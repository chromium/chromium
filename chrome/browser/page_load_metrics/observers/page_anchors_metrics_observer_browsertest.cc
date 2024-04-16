// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_load_metrics/observers/page_anchors_metrics_observer.h"

#include <string_view>

#include "base/memory/raw_ptr.h"
#include "base/test/scoped_run_loop_timeout.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/ukm/test_ukm_recorder.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/dns/mock_host_resolver.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_source.h"
#include "testing/gtest/include/gtest/gtest.h"

class PageAnchorsMetricsObserverBrowserTest
    : public InProcessBrowserTest,
      public testing::WithParamInterface<bool>,
      public content::WebContentsObserver {
 public:
  PageAnchorsMetricsObserverBrowserTest() {
    // Report all anchors to avoid non-deterministic behavior.
    std::map<std::string, std::string> params;
    params["random_anchor_sampling_period"] = "1";

    feature_list_.InitAndEnableFeatureWithParameters(
        blink::features::kNavigationPredictor, params);
  }
  ~PageAnchorsMetricsObserverBrowserTest() override = default;

  void SetUp() override {
    https_server_ = std::make_unique<net::EmbeddedTestServer>(
        net::EmbeddedTestServer::TYPE_HTTPS);
    https_server_->ServeFilesFromSourceDirectory(
        "chrome/test/data/navigation_predictor");
    ASSERT_TRUE(https_server_->Start());

    InProcessBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    host_resolver()->ClearRules();
    ukm_recorder_ = std::make_unique<ukm::TestAutoSetUkmRecorder>();
  }

  const GURL GetTestURL(const char* file) const {
    return https_server_->GetURL(file);
  }

  void NavigateTo(const GURL& url) {
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
    base::RunLoop().RunUntilIdle();
  }

  void NavigateAway() {
    ASSERT_TRUE(
        ui_test_utils::NavigateToURL(browser(), GURL(url::kAboutBlankURL)));
    base::RunLoop().RunUntilIdle();
  }

  void ResetUKM() {
    ukm_recorder_ = std::make_unique<ukm::TestAutoSetUkmRecorder>();
  }

  size_t WaitForUKMEntry(const std::string& entry_name,
                         size_t expected_entries_size) {
    while (ukm_recorder_->GetEntriesByName(entry_name).size() <
           expected_entries_size) {
      base::test::ScopedRunLoopTimeout loop_timeout(FROM_HERE,
                                                    base::Seconds(5));
      base::RunLoop run_loop;
      ukm_recorder_->SetOnAddEntryCallback(entry_name, run_loop.QuitClosure());
      run_loop.Run();
    }
    return ukm_recorder_->GetEntriesByName(entry_name).size();
  }

  std::vector<raw_ptr<const ukm::mojom::UkmEntry, VectorExperimental>>
  GetEntriesByName(std::string_view entry_name) const {
    return ukm_recorder_->GetEntriesByName(entry_name);
  }

  void VerifyNthUKMEntry(ukm::SourceId ukm_source_id,
                         const std::string& entry_name,
                         const std::string& metric_name,
                         int64_t expected_value,
                         size_t expected_entries_size = 1) {
    ASSERT_TRUE(expected_entries_size > 0);
    auto get_entries = [this](ukm::SourceId ukm_source_id,
                              const std::string& entry_name) {
      std::vector<const ukm::mojom::UkmEntry*> entries;
      for (const ukm::mojom::UkmEntry* entry :
           ukm_recorder_->GetEntriesByName(entry_name)) {
        if (entry->source_id == ukm_source_id) {
          entries.push_back(entry);
        }
      }
      return entries;
    };
    if (get_entries(ukm_source_id, entry_name).size() < expected_entries_size) {
      base::test::ScopedRunLoopTimeout loop_timeout(FROM_HERE,
                                                    base::Seconds(5));
      base::RunLoop run_loop;
      ukm_recorder_->SetOnAddEntryCallback(entry_name, run_loop.QuitClosure());
      run_loop.Run();
    }
    auto entries = get_entries(ukm_source_id, entry_name);
    ASSERT_EQ(expected_entries_size, entries.size());

    const auto* entry = entries.back();

    const int64_t* value =
        ukm::TestUkmRecorder::GetEntryMetric(entry, metric_name);
    ASSERT_TRUE(value != nullptr);
    EXPECT_EQ(expected_value, *value);
  }

  content::WebContents* web_contents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

 private:
  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<net::EmbeddedTestServer> https_server_;
  std::unique_ptr<ukm::TestAutoSetUkmRecorder> ukm_recorder_;
};

// Disabled for being flaky. See crbug.com/1471215
IN_PROC_BROWSER_TEST_F(PageAnchorsMetricsObserverBrowserTest,
                       DISABLED_NavigateAwayShouldRecordUkmData) {
  ResetUKM();
  NavigateTo(GetTestURL("/1.html"));

  // Wait for anchor elements to be added to the UKM before navigating away.
  const auto ukm_source_id =
      web_contents()->GetPrimaryMainFrame()->GetPageUkmSourceId();
  VerifyNthUKMEntry(
      ukm_source_id,
      ukm::builders::NavigationPredictorAnchorElementMetrics::kEntryName,
      ukm::builders::NavigationPredictorAnchorElementMetrics::kAnchorIndexName,
      0);

  // Navigate away and check for user interactions in UKM records.
  NavigateAway();
  VerifyNthUKMEntry(
      ukm_source_id,
      ukm::builders::NavigationPredictorUserInteractions::kEntryName,
      ukm::builders::NavigationPredictorUserInteractions::kAnchorIndexName, 0);
}

IN_PROC_BROWSER_TEST_F(PageAnchorsMetricsObserverBrowserTest,
                       BFCacheShouldRecordUkmData) {
  // Start with page1 and then navigate to page2.
  ResetUKM();
  NavigateTo(GetTestURL("/1.html"));
  NavigateTo(GetTestURL("/2.html"));

  // Wait for anchor elements to be added to the UKM before navigating back.
  const auto ukm_source_id =
      web_contents()->GetPrimaryMainFrame()->GetPageUkmSourceId();
  VerifyNthUKMEntry(
      ukm_source_id,
      ukm::builders::NavigationPredictorAnchorElementMetrics::kEntryName,
      ukm::builders::NavigationPredictorAnchorElementMetrics::kAnchorIndexName,
      0);

  // Navigate back to page1 and check for user interactions in UKM records.
  ASSERT_TRUE(HistoryGoBack(web_contents()));
  VerifyNthUKMEntry(
      ukm_source_id,
      ukm::builders::NavigationPredictorUserInteractions::kEntryName,
      ukm::builders::NavigationPredictorUserInteractions::kAnchorIndexName, 0);
}

// TODO(crbug.com/40273087): Test is flaky.
IN_PROC_BROWSER_TEST_F(PageAnchorsMetricsObserverBrowserTest,
                       DISABLED_TestDifferentUKMSourceIdsPerNavigation) {
  // Start with page 1.
  ResetUKM();
  NavigateTo(GetTestURL("/1.html"));

  // Navigate to page 2.
  size_t anchor_elements_entries_size = WaitForUKMEntry(
      ukm::builders::NavigationPredictorAnchorElementMetrics::kEntryName, 1u);
  NavigateTo(GetTestURL("/2.html"));
  // UserInteractions for page 1 should be recorded to UKM.
  size_t user_interactions_entries_size = WaitForUKMEntry(
      ukm::builders::NavigationPredictorUserInteractions::kEntryName, 1u);

  // Navigate back to page 1.
  anchor_elements_entries_size = WaitForUKMEntry(
      ukm::builders::NavigationPredictorAnchorElementMetrics::kEntryName,
      anchor_elements_entries_size + 1u);
  ASSERT_TRUE(HistoryGoBack(web_contents()));
  // UserInteractions for page 2 should be recorded to UKM.
  user_interactions_entries_size = WaitForUKMEntry(
      ukm::builders::NavigationPredictorUserInteractions::kEntryName,
      user_interactions_entries_size + 1u);

  // Upon restoring from BFCache the `AnchorElementData` for page 1 should be
  // recorded to UKM.
  anchor_elements_entries_size = WaitForUKMEntry(
      ukm::builders::NavigationPredictorAnchorElementMetrics::kEntryName,
      anchor_elements_entries_size + 1u);
  // Do some interactions.
  EXPECT_TRUE(content::ExecJs(web_contents(),
                              R"(
          let a = document.createElement("a");
          a.id = "link";
          a.href = "https://www.google.com";
          a.text = "Google";
          document.body.appendChild(a);
        )"));
  base::RunLoop().RunUntilIdle();

  // Navigate forward to page 2.
  anchor_elements_entries_size = WaitForUKMEntry(
      ukm::builders::NavigationPredictorAnchorElementMetrics::kEntryName,
      anchor_elements_entries_size + 1u);
  ASSERT_TRUE(HistoryGoForward(web_contents()));
  // UserInteractions for page 1 should be recorded to UKM.
  user_interactions_entries_size = WaitForUKMEntry(
      ukm::builders::NavigationPredictorUserInteractions::kEntryName,
      user_interactions_entries_size + 1u);

  // Upon restoring from BFCache the `AnchorElementData` for page 2 should be
  // recorded to UKM.
  anchor_elements_entries_size = WaitForUKMEntry(
      ukm::builders::NavigationPredictorAnchorElementMetrics::kEntryName,
      anchor_elements_entries_size + 1u);

  // Do some interactions.
  EXPECT_TRUE(content::ExecJs(web_contents(),
                              R"(
          let a = document.createElement("a");
          a.id = "link";
          a.href = "https://www.example.com";
          a.text = "Example";
          document.body.appendChild(a);
        )"));
  base::RunLoop().RunUntilIdle();

  // Navigate away from page 2.
  anchor_elements_entries_size = WaitForUKMEntry(
      ukm::builders::NavigationPredictorAnchorElementMetrics::kEntryName,
      anchor_elements_entries_size + 1u);
  NavigateAway();
  // UserInteractions for page 2 should be recorded to UKM.
  user_interactions_entries_size = WaitForUKMEntry(
      ukm::builders::NavigationPredictorUserInteractions::kEntryName,
      user_interactions_entries_size + 1u);

  // Check that we used 4 different SourceIds.
  base::flat_set<ukm::SourceId> source_ids;
  for (const ukm::mojom::UkmEntry* entry : GetEntriesByName(
           ukm::builders::NavigationPredictorUserInteractions::kEntryName)) {
    source_ids.insert(entry->source_id);
  }
  EXPECT_EQ(4u, source_ids.size());
}

IN_PROC_BROWSER_TEST_F(PageAnchorsMetricsObserverBrowserTest,
                       NavigateToSameUrlTwice) {
  // Start with page1 and then navigate to page2.
  ResetUKM();
  NavigateTo(GetTestURL("/1.html"));
  NavigateTo(GetTestURL("/1.html"));

  // Wait for anchor elements to be added to the UKM before navigating back.
  const auto ukm_source_id =
      web_contents()->GetPrimaryMainFrame()->GetPageUkmSourceId();
  VerifyNthUKMEntry(
      ukm_source_id,
      ukm::builders::NavigationPredictorAnchorElementMetrics::kEntryName,
      ukm::builders::NavigationPredictorAnchorElementMetrics::kAnchorIndexName,
      0);

  // Navigate back to page1 and check for user interactions in UKM records.
  ASSERT_TRUE(HistoryGoBack(web_contents()));
  VerifyNthUKMEntry(
      ukm_source_id,
      ukm::builders::NavigationPredictorUserInteractions::kEntryName,
      ukm::builders::NavigationPredictorUserInteractions::kAnchorIndexName, 0);
}
