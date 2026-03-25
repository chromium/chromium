// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/contextual_cueing/contextual_cueing_controller.h"

#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/contextual_cueing/features.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/test/browser_test.h"

namespace contextual_cueing {
namespace {

class ContextualCueingControllerBrowserTest : public InProcessBrowserTest {
 public:
  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(kContextualCueingV2);
    InProcessBrowserTest::SetUp();
  }

  ContextualCueingController* contextual_cueing_controller() {
    return browser()->GetFeatures().contextual_cueing_controller();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(ContextualCueingControllerBrowserTest,
                       NoLongerActiveTabAfterCategoryClassification) {
  base::HistogramTester histogram_tester;

  // Time stamp won't match whatever navigated since it does not match the
  // active tab.
  contextual_cueing_controller()->OnPageContentAnnotated(
      page_content_annotations::HistoryVisit(base::Time::Now(),
                                             GURL("https://www.example.com")),
      page_content_annotations::PageContentAnnotationsResult::
          CreateCategoryResults({
              page_content_annotations::Category(
                  page_content_annotations::CategoryType::kEducation, 0.9),
              page_content_annotations::Category(
                  page_content_annotations::CategoryType::kShopping, 0.4),
          }));

  histogram_tester.ExpectUniqueSample(
      "ContextualCueing.V2.Decision",
      ContextualCueingDecision::kNoLongerActiveTabAfterCategoryClassification,
      1);
}

IN_PROC_BROWSER_TEST_F(ContextualCueingControllerBrowserTest,
                       FailedCategoryClassification) {
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), GURL("https://www.example.com")));

  base::HistogramTester histogram_tester;

  content::WebContents* active_web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(active_web_contents);
  contextual_cueing_controller()->OnPageContentAnnotated(
      page_content_annotations::HistoryVisit(
          active_web_contents->GetController()
              .GetLastCommittedEntry()
              ->GetTimestamp(),
          GURL("https://www.example.com")),
      page_content_annotations::PageContentAnnotationsResult::
          CreateCategoryResults({
              page_content_annotations::Category(
                  page_content_annotations::CategoryType::kEducation, 0.4),
              page_content_annotations::Category(
                  page_content_annotations::CategoryType::kShopping, 0.2),
          }));

  histogram_tester.ExpectUniqueSample(
      "ContextualCueing.V2.Decision",
      ContextualCueingDecision::kFailedCategoryClassification, 1);
}

IN_PROC_BROWSER_TEST_F(ContextualCueingControllerBrowserTest, PassesFilter) {
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), GURL("https://www.example.com")));

  base::HistogramTester histogram_tester;

  content::WebContents* active_web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(active_web_contents);
  contextual_cueing_controller()->OnPageContentAnnotated(
      page_content_annotations::HistoryVisit(
          active_web_contents->GetController()
              .GetLastCommittedEntry()
              ->GetTimestamp(),
          GURL("https://www.example.com")),
      page_content_annotations::PageContentAnnotationsResult::
          CreateCategoryResults({
              page_content_annotations::Category(
                  page_content_annotations::CategoryType::kEducation, 0.9),
              page_content_annotations::Category(
                  page_content_annotations::CategoryType::kShopping, 0.2),
          }));

  // No decision made yet as it passed all the checks implemented so far.
  histogram_tester.ExpectTotalCount("ContextualCueing.V2.Decision", 0);
}

}  // namespace
}  // namespace contextual_cueing
