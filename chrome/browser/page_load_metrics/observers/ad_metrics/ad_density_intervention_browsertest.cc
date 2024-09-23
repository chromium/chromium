// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/subresource_filter/subresource_filter_browser_test_harness.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "components/infobars/content/content_infobar_manager.h"
#include "components/infobars/core/infobar.h"
#include "components/infobars/core/infobar_delegate.h"
#include "components/infobars/core/infobar_manager.h"
#include "components/page_load_metrics/browser/page_load_metrics_test_waiter.h"
#include "components/subresource_filter/content/browser/content_subresource_filter_throttle_manager.h"
#include "components/subresource_filter/core/common/common_features.h"
#include "components/subresource_filter/core/common/test_ruleset_utils.h"
#include "components/subresource_filter/core/mojom/subresource_filter.mojom.h"
#include "components/ukm/test_ukm_recorder.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace {

const char kAdsInterventionRecordedHistogram[] =
    "SubresourceFilter.PageLoad.AdsInterventionTriggered";

// Gets the body height of the document embedded in |web_contents|.
int GetDocumentHeight(content::WebContents* web_contents) {
  return EvalJs(web_contents, "document.body.scrollHeight").ExtractInt();
}

}  // namespace

class AdDensityViolationBrowserTest
    : public subresource_filter::SubresourceFilterBrowserTest {
 public:
  AdDensityViolationBrowserTest() = default;

  void SetUp() override {
    std::vector<base::test::FeatureRef> enabled = {
        subresource_filter::kAdTagging,
        subresource_filter::kAdsInterventionsEnforced};
    std::vector<base::test::FeatureRef> disabled = {};

    feature_list_.InitWithFeatures(enabled, disabled);
    subresource_filter::SubresourceFilterBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    SubresourceFilterBrowserTest::SetUpOnMainThread();
    SetRulesetWithRules(
        {subresource_filter::testing::CreateSuffixRule("ad_iframe_writer.js")});
  }

  void CreateAndWaitForIframeAtRect(
      content::WebContents* web_contents,
      page_load_metrics::PageLoadMetricsTestWaiter* waiter,
      int x,
      int y,
      int width,
      int height) {
    waiter->AddMainFrameIntersectionExpectation(gfx::Rect(x, y, width, height));

    // Create the frame with b.com as origin to not get caught by
    // restricted ad tagging.
    EXPECT_TRUE(ExecJs(
        web_contents,
        content::JsReplace("let frame = createAdIframeAtRect($1, $2, $3, $4); "
                           "frame.src = $5",
                           x, y, width, height,
                           embedded_test_server()
                               ->GetURL("b.com", "/ads_observer/pixel.png")
                               .spec())));

    waiter->Wait();
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// TODO(crbug.com/40916871): Fix this test on Mac.
#if BUILDFLAG(IS_MAC)
#define MAYBE_DesktopPageAdDensityByHeightAbove30_AdInterventionNotTriggered \
  DISABLED_DesktopPageAdDensityByHeightAbove30_AdInterventionNotTriggered
#else
#define MAYBE_DesktopPageAdDensityByHeightAbove30_AdInterventionNotTriggered \
  DesktopPageAdDensityByHeightAbove30_AdInterventionNotTriggered
#endif
// TODO(crbug.com/40727827): Replace this heavy-weight browsertest with
// a unit test.
IN_PROC_BROWSER_TEST_F(
    AdDensityViolationBrowserTest,
    MAYBE_DesktopPageAdDensityByHeightAbove30_AdInterventionNotTriggered) {
  base::HistogramTester histogram_tester;
  ukm::TestAutoSetUkmRecorder ukm_recorder;

  content::WebContents* web_contents =
      chrome_test_utils::GetActiveWebContents(this);
  auto waiter = std::make_unique<page_load_metrics::PageLoadMetricsTestWaiter>(
      web_contents);
  const GURL url(embedded_test_server()->GetURL(
      "a.com", "/ads_observer/blank_with_adiframe_writer.html"));

  waiter->SetMainFrameIntersectionExpectation();
  EXPECT_TRUE(content::NavigateToURL(web_contents, url));
  waiter->Wait();

  int document_width =
      EvalJs(web_contents, "document.body.scrollWidth").ExtractInt();
  int document_height = GetDocumentHeight(web_contents);

  // Set to document width so ad density is 100%.
  int frame_width = document_width;
  int frame_height = document_height;

  CreateAndWaitForIframeAtRect(web_contents, waiter.get(), 400, 400,
                               frame_width, frame_height);

  // Delete the page load metrics test waiter instead of reinitializing it
  // for the next page load.
  waiter.reset();

  EXPECT_TRUE(content::NavigateToURL(web_contents, url));

  // We are not enforcing ad blocking on ads violations, site should load
  // as expected without subresource filter UI.
  EXPECT_TRUE(
      WasParsedScriptElementLoaded(web_contents->GetPrimaryMainFrame()));
  // No ads blocked infobar should be shown as we have not triggered the
  // intervention.
  histogram_tester.ExpectBucketCount(
      "SubresourceFilter.Actions2",
      subresource_filter::SubresourceFilterAction::kUIShown, 0);
  histogram_tester.ExpectTotalCount(kAdsInterventionRecordedHistogram, 0);
}
