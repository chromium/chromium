// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/subresource_filter/subresource_filter_browser_test_harness.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "components/infobars/content/content_infobar_manager.h"
#include "components/infobars/core/infobar.h"
#include "components/infobars/core/infobar_delegate.h"
#include "components/infobars/core/infobar_manager.h"
#include "components/messages/android/message_enums.h"
#include "components/messages/android/messages_feature.h"
#include "components/messages/android/test/messages_test_helper.h"
#include "components/page_load_metrics/browser/observers/ad_metrics/ad_intervention_browser_test_utils.h"
#include "components/page_load_metrics/browser/page_load_metrics_test_waiter.h"
#include "components/subresource_filter/content/browser/content_subresource_filter_throttle_manager.h"
#include "components/subresource_filter/core/common/common_features.h"
#include "components/subresource_filter/core/common/test_ruleset_utils.h"
#include "components/subresource_filter/core/mojom/subresource_filter.mojom.h"
#include "components/ukm/test_ukm_recorder.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/display/screen_info.h"
#include "ui/gfx/geometry/rect.h"
#include "url/gurl.h"

namespace {

const char kAdsInterventionRecordedHistogram[] =
    "SubresourceFilter.PageLoad.AdsInterventionTriggered";

}  // namespace

class AdDensityViolationBrowserTestInfobarUi
    : public subresource_filter::SubresourceFilterBrowserTest {
 public:
  AdDensityViolationBrowserTestInfobarUi() = default;

  void SetUp() override {
    std::vector<base::test::FeatureRef> enabled = {
        subresource_filter::kAdTagging,
        subresource_filter::kAdsInterventionsEnforced};
    std::vector<base::test::FeatureRef> disabled = {
        messages::kMessagesForAndroidAdsBlocked};

    feature_list_.InitWithFeatures(enabled, disabled);
    subresource_filter::SubresourceFilterBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    SubresourceFilterBrowserTest::SetUpOnMainThread();
    SetRulesetWithRules(
        {subresource_filter::testing::CreateSuffixRule("ad_iframe_writer.js")});
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(
    AdDensityViolationBrowserTestInfobarUi,
    MobilePageAdDensityByHeightAbove30_AdInterventionTriggered_InfobarUi) {
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

  int document_height = page_load_metrics::GetDocumentHeight(web_contents);

  int frame_width = 100;  // Ad density by height is independent of frame width.
  int frame_height = document_height * 0.45;

  // Create the frame with b.com as origin to not get caught by
  // restricted ad tagging.
  page_load_metrics::CreateAndWaitForIframeAtRect(
      web_contents, waiter.get(),
      embedded_test_server()->GetURL("b.com", "/ads_observer/pixel.png"),
      gfx::Rect(0, 0, frame_width, frame_height));

  // Delete the page load metrics test waiter instead of reinitializing it
  // for the next page load.
  waiter.reset();

  EXPECT_TRUE(content::NavigateToURL(web_contents, url));

  // blank_with_adiframe_writer loads a script tagged as an ad, verify it is not
  // loaded and the subresource filter UI for ad blocking is shown.
  EXPECT_FALSE(
      WasParsedScriptElementLoaded(web_contents->GetPrimaryMainFrame()));
  EXPECT_EQ(infobars::ContentInfoBarManager::FromWebContents(web_contents)
                ->infobars()
                .size(),
            1u);
  EXPECT_EQ(infobars::ContentInfoBarManager::FromWebContents(web_contents)
                ->infobars()[0]
                ->delegate()
                ->GetIdentifier(),
            infobars::InfoBarDelegate::ADS_BLOCKED_INFOBAR_DELEGATE_ANDROID);
  histogram_tester.ExpectBucketCount(
      kAdsInterventionRecordedHistogram,
      static_cast<int>(subresource_filter::mojom::AdsViolation::
                           kMobileAdDensityByHeightAbove30),
      1);
}

IN_PROC_BROWSER_TEST_F(
    AdDensityViolationBrowserTestInfobarUi,
    MobilePageAdDensityByHeightBelow30_AdInterventionNotTriggered_InfobarUi) {
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

  int document_height = page_load_metrics::GetDocumentHeight(web_contents);

  int frame_width = 100;  // Ad density by height is independent of frame width.
  int frame_height = document_height * 0.25;

  // Create the frame with b.com as origin to not get caught by
  // restricted ad tagging.
  page_load_metrics::CreateAndWaitForIframeAtRect(
      web_contents, waiter.get(),
      embedded_test_server()->GetURL("b.com", "/ads_observer/pixel.png"),
      gfx::Rect(0, 0, frame_width, frame_height));

  // Delete the page load metrics test waiter instead of reinitializing it
  // for the next page load.
  waiter.reset();

  EXPECT_TRUE(content::NavigateToURL(web_contents, url));

  // blank_with_adiframe_writer loads a script tagged as an ad, verify it is
  // loaded as ads are not blocked and the subresource filter UI is not
  // shown.
  EXPECT_TRUE(
      WasParsedScriptElementLoaded(web_contents->GetPrimaryMainFrame()));

  // No ads blocked infobar should be shown as we have not triggered the
  // intervention.
  EXPECT_EQ(infobars::ContentInfoBarManager::FromWebContents(web_contents)
                ->infobars()
                .size(),
            0u);
  histogram_tester.ExpectTotalCount(kAdsInterventionRecordedHistogram, 0);
}

class AdDensityViolationBrowserTestMessagesUi
    : public subresource_filter::SubresourceFilterBrowserTest {
 public:
  AdDensityViolationBrowserTestMessagesUi() = default;

  void SetUp() override {
    std::vector<base::test::FeatureRef> enabled = {
        subresource_filter::kAdTagging,
        subresource_filter::kAdsInterventionsEnforced,
        messages::kMessagesForAndroidAdsBlocked};
    std::vector<base::test::FeatureRef> disabled = {};

    feature_list_.InitWithFeatures(enabled, disabled);
    subresource_filter::SubresourceFilterBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    SubresourceFilterBrowserTest::SetUpOnMainThread();
    SetRulesetWithRules(
        {subresource_filter::testing::CreateSuffixRule("ad_iframe_writer.js")});
  }

  messages::MessagesTestHelper messages_test_helper;

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(
    AdDensityViolationBrowserTestMessagesUi,
    MobilePageAdDensityByHeightAbove30_AdInterventionTriggered_MessagesUi) {
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

  int document_height = page_load_metrics::GetDocumentHeight(web_contents);

  int frame_width = 100;  // Ad density by height is independent of frame width.
  int frame_height = document_height * 0.45;

  // Create the frame with b.com as origin to not get caught by
  // restricted ad tagging.
  page_load_metrics::CreateAndWaitForIframeAtRect(
      web_contents, waiter.get(),
      embedded_test_server()->GetURL("b.com", "/ads_observer/pixel.png"),
      gfx::Rect(0, 0, frame_width, frame_height));

  // Delete the page load metrics test waiter instead of reinitializing it
  // for the next page load.
  waiter.reset();

  EXPECT_TRUE(content::NavigateToURL(web_contents, url));

  // blank_with_adiframe_writer loads a script tagged as an ad, verify it is not
  // loaded and the subresource filter UI for ad blocking is shown.
  EXPECT_FALSE(
      WasParsedScriptElementLoaded(web_contents->GetPrimaryMainFrame()));

  EXPECT_EQ(messages_test_helper.GetMessageCount(
                web_contents->GetTopLevelNativeWindow()),
            1);
  EXPECT_EQ(messages_test_helper.GetMessageIdentifier(
                web_contents->GetTopLevelNativeWindow(), 0),
            static_cast<int>(messages::MessageIdentifier::ADS_BLOCKED));

  histogram_tester.ExpectBucketCount(
      kAdsInterventionRecordedHistogram,
      static_cast<int>(subresource_filter::mojom::AdsViolation::
                           kMobileAdDensityByHeightAbove30),
      1);
}

IN_PROC_BROWSER_TEST_F(
    AdDensityViolationBrowserTestMessagesUi,
    MobilePageAdDensityByHeightBelow30_AdInterventionNotTriggered_MessagesUi) {
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

  int document_height = page_load_metrics::GetDocumentHeight(web_contents);

  int frame_width = 100;  // Ad density by height is independent of frame width.
  int frame_height = document_height * 0.25;

  // Create the frame with b.com as origin to not get caught by
  // restricted ad tagging.
  page_load_metrics::CreateAndWaitForIframeAtRect(
      web_contents, waiter.get(),
      embedded_test_server()->GetURL("b.com", "/ads_observer/pixel.png"),
      gfx::Rect(0, 0, frame_width, frame_height));

  // Delete the page load metrics test waiter instead of reinitializing it
  // for the next page load.
  waiter.reset();

  EXPECT_TRUE(content::NavigateToURL(web_contents, url));

  // blank_with_adiframe_writer loads a script tagged as an ad, verify it is
  // loaded as ads are not blocked and the subresource filter UI is not
  // shown.
  EXPECT_TRUE(
      WasParsedScriptElementLoaded(web_contents->GetPrimaryMainFrame()));

  // No ads blocked message should be shown as we have not triggered the
  // intervention.
  EXPECT_EQ(messages_test_helper.GetMessageCount(
                web_contents->GetTopLevelNativeWindow()),
            0);
  histogram_tester.ExpectTotalCount(kAdsInterventionRecordedHistogram, 0);
}

class AdDensityViolationBrowserTestWithoutEnforcement
    : public subresource_filter::SubresourceFilterBrowserTest {
 public:
  AdDensityViolationBrowserTestWithoutEnforcement() = default;

  void SetUp() override {
    std::vector<base::test::FeatureRef> enabled = {
        subresource_filter::kAdTagging};
    std::vector<base::test::FeatureRef> disabled = {
        subresource_filter::kAdsInterventionsEnforced};

    feature_list_.InitWithFeatures(enabled, disabled);
    subresource_filter::SubresourceFilterBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    SubresourceFilterBrowserTest::SetUpOnMainThread();
    SetRulesetWithRules(
        {subresource_filter::testing::CreateSuffixRule("ad_iframe_writer.js")});
  }

  messages::MessagesTestHelper messages_test_helper;

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(
    AdDensityViolationBrowserTestWithoutEnforcement,
    MobilePageAdDensityByHeightAbove30_NoAdInterventionTriggered) {
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

  int document_height = page_load_metrics::GetDocumentHeight(web_contents);

  int frame_width = 100;  // Ad density by height is independent of frame width.
  int frame_height = document_height * 0.45;

  // Create the frame with b.com as origin to not get caught by
  // restricted ad tagging.
  page_load_metrics::CreateAndWaitForIframeAtRect(
      web_contents, waiter.get(),
      embedded_test_server()->GetURL("b.com", "/ads_observer/pixel.png"),
      gfx::Rect(0, 0, frame_width, frame_height));

  // Delete the page load metrics test waiter instead of reinitializing it
  // for the next page load.
  waiter.reset();

  EXPECT_TRUE(content::NavigateToURL(web_contents, url));

  // We are not enforcing ad blocking on ads violations, site should load
  // as expected without subresource filter UI.
  EXPECT_TRUE(
      WasParsedScriptElementLoaded(web_contents->GetPrimaryMainFrame()));

  // No ads blocked prompt should be shown as we have not triggered the
  // intervention.
  EXPECT_EQ(infobars::ContentInfoBarManager::FromWebContents(web_contents)
                ->infobars()
                .size(),
            0u);
  EXPECT_EQ(messages_test_helper.GetMessageCount(
                web_contents->GetTopLevelNativeWindow()),
            0);
  histogram_tester.ExpectBucketCount(
      kAdsInterventionRecordedHistogram,
      static_cast<int>(subresource_filter::mojom::AdsViolation::
                           kMobileAdDensityByHeightAbove30),
      1);
}
