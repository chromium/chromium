// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/contextual_cueing/contextual_cueing_controller.h"

#include "base/strings/strcat.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/contextual_cueing/features.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/optimization_guide/proto/features/contextual_cueing.pb.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/test/browser_test.h"
#include "ui/base/window_open_disposition.h"

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

  void SeedExecutionResult(
      optimization_guide::OptimizationGuideModelExecutionResult result) {
    OptimizationGuideKeyedServiceFactory::GetInstance()
        ->GetForProfile(browser()->profile())
        ->AddExecutionResultForTesting(
            optimization_guide::ModelBasedCapabilityKey::kContextualCueing,
            std::move(result));
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

IN_PROC_BROWSER_TEST_F(ContextualCueingControllerBrowserTest,
                       PassesFilterButModelExecutionFailed) {
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), GURL("https://www.example.com")));

  base::HistogramTester histogram_tester;

  // Seed empty execution result.
  optimization_guide::OptimizationGuideModelExecutionResult result(
      optimization_guide::proto::Any(), /*execution_info=*/nullptr);
  SeedExecutionResult(std::move(result));

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

  histogram_tester.ExpectUniqueSample(
      "ContextualCueing.V2.Decision",
      ContextualCueingDecision::kModelExecutionResponseFailedToParse, 1);
}

IN_PROC_BROWSER_TEST_F(ContextualCueingControllerBrowserTest,
                       PassesFilterButModelExecutionSucceeded) {
  // Navigate current Chrome tab to a valid URL (and will be in the background
  // in final state).
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), GURL("https://www.someurl.com")));

  // Create a new tab (will be in the background in final state).
  chrome::NewTab(browser());

  // Navigate to a new eligible tab to be in the foreground (current active
  // tab).
  ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL("https://www.example.com"),
      WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));

  base::HistogramTester histogram_tester;

  // Seed empty execution result.
  optimization_guide::proto::ContextualCueingResponse response;
  response.mutable_anchored_message_cue()->set_action_text("Action text");
  response.mutable_anchored_message_cue()->set_anchored_message_text(
      "Anchored message text");
  optimization_guide::proto::Any response_any;
  response.SerializeToString(response_any.mutable_value());
  response_any.set_type_url(
      base::StrCat({"type.googleapis.com/", response.GetTypeName()}));
  optimization_guide::OptimizationGuideModelExecutionResult result(
      response_any, /*execution_info=*/nullptr);
  SeedExecutionResult(std::move(result));

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

  histogram_tester.ExpectUniqueSample("ContextualCueing.V2.Decision",
                                      ContextualCueingDecision::kSuccess, 1);
  // There are three total tabs (one is active, one is valid as a background
  // tab, and the other is a new tab). Active and non HTTP/HTTPS tabs are
  // skipped.
  histogram_tester.ExpectUniqueSample(
      "ContextualCueing.V2.NumRequestedBackgroundTabs", 1, 1);
}

}  // namespace
}  // namespace contextual_cueing
