// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/previews/previews_lite_page_redirect_predictor.h"

#include <vector>

#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/previews/previews_lite_page_redirect_url_loader_interceptor.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/previews/core/previews_features.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/mock_navigation_handle.h"
#include "content/public/test/web_contents_tester.h"
#include "url/gurl.h"

namespace {
const char kTestUrl[] = "https://www.test.com/";
}

class TestPreviewsLitePageRedirectPredictor
    : public PreviewsLitePageRedirectPredictor {
 public:
  TestPreviewsLitePageRedirectPredictor(content::WebContents* web_contents,
                                        bool data_saver_enabled,
                                        bool ect_is_slow,
                                        bool page_is_blacklisted,
                                        bool is_visible)
      : PreviewsLitePageRedirectPredictor(web_contents),
        data_saver_enabled_(data_saver_enabled),
        ect_is_slow_(ect_is_slow),
        page_is_blacklisted_(page_is_blacklisted),
        is_visible_(is_visible) {}

  // PreviewsLitePageRedirectPredictor:
  bool DataSaverIsEnabled() const override { return data_saver_enabled_; }
  bool ECTIsSlow() const override { return ect_is_slow_; }
  bool PageIsBlacklisted(
      content::NavigationHandle* navigation_handle) const override {
    return page_is_blacklisted_;
  }
  bool IsVisible() const override { return is_visible_; }

  void set_ect_is_slow(bool ect_is_slow) { ect_is_slow_ = ect_is_slow; }
  void set_is_visible(bool is_visible) { is_visible_ = is_visible; }

 private:
  bool data_saver_enabled_;
  bool ect_is_slow_;
  bool page_is_blacklisted_;
  bool is_visible_;
};

// True for preresolve testing, false for preconnect.
class PreviewsLitePageRedirectPredictorUnitTest
    : public ChromeRenderViewHostTestHarness,
      public testing::WithParamInterface<bool> {
 public:
  void RunTest(bool feature_enabled,
               bool data_saver_enabled,
               bool ect_is_slow,
               bool page_is_blacklisted,
               bool is_visible) {
    if (feature_enabled) {
      scoped_feature_list_.InitAndEnableFeatureWithParameters(
          previews::features::kLitePageServerPreviews,
          {
              {"preresolve_on_slow_connections", GetParam() ? "true" : "false"},
              {"preconnect_on_slow_connections",
               !GetParam() ? "true" : "false"},
          });
    } else {
      scoped_feature_list_.InitAndDisableFeature(
          previews::features::kLitePageServerPreviews);
    }
    predictor_.reset(new TestPreviewsLitePageRedirectPredictor(
        web_contents(), data_saver_enabled, ect_is_slow, page_is_blacklisted,
        is_visible));
    test_handle_.reset(
        new content::MockNavigationHandle(GURL(kTestUrl), main_rfh()));
    std::vector<GURL> redirect_chain;
    redirect_chain.push_back(GURL(kTestUrl));
    test_handle_->set_redirect_chain(redirect_chain);
    content::RenderFrameHostTester::For(main_rfh())
        ->InitializeRenderFrameIfNeeded();
  }

  void SimulateWillProcessResponse() { SimulateCommit(); }

  void SimulateCommit() {
    test_handle_->set_has_committed(true);
    test_handle_->set_url(GURL(kTestUrl));
  }

  void CallDidFinishNavigation() {
    predictor()->DidFinishNavigation(test_handle_.get());
  }

  std::string GetParamedHistogramName() {
    if (GetParam())
      return "Previews.ServerLitePage.PreresolvedToPreviewServer";
    return "Previews.ServerLitePage.PreconnectedToPreviewServer";
  }

  std::string GetOtherHistogramName() {
    if (!GetParam())
      return "Previews.ServerLitePage.PreresolvedToPreviewServer";
    return "Previews.ServerLitePage.PreconnectedToPreviewServer";
  }

  TestPreviewsLitePageRedirectPredictor* predictor() const {
    return predictor_.get();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<TestPreviewsLitePageRedirectPredictor> predictor_;
  std::unique_ptr<content::MockNavigationHandle> test_handle_;
};

TEST_P(PreviewsLitePageRedirectPredictorUnitTest, AllConditionsMet_Origin) {
  RunTest(true /* feature_enabled */, true /* data_saver_enabled */,
          true /* ect_is_slow */, false /* page_is_blacklisted */,
          true /* is_visible */);

  base::HistogramTester histogram_tester;

  content::WebContentsTester::For(web_contents())
      ->NavigateAndCommit(GURL(kTestUrl));

  EXPECT_TRUE(predictor()->ShouldActOnPage(nullptr));
  histogram_tester.ExpectUniqueSample(
      "Previews.ServerLitePage.PredictorToggled", true, 1);
  histogram_tester.ExpectUniqueSample(GetParamedHistogramName(), true, 1);
  histogram_tester.ExpectTotalCount(GetOtherHistogramName(), 0);
}

TEST_P(PreviewsLitePageRedirectPredictorUnitTest, AllConditionsMet_Preview) {
  RunTest(true /* feature_enabled */, true /* data_saver_enabled */,
          true /* ect_is_slow */, false /* page_is_blacklisted */,
          true /* is_visible */);

  base::HistogramTester histogram_tester;

  content::WebContentsTester::For(web_contents())
      ->NavigateAndCommit(
          previews::GetLitePageRedirectURLForURL(GURL(kTestUrl)));

  EXPECT_TRUE(predictor()->ShouldActOnPage(nullptr));
  histogram_tester.ExpectUniqueSample(
      "Previews.ServerLitePage.PredictorToggled", true, 1);
  histogram_tester.ExpectUniqueSample(GetParamedHistogramName(), false, 1);
  histogram_tester.ExpectTotalCount(GetOtherHistogramName(), 0);
}

TEST_P(PreviewsLitePageRedirectPredictorUnitTest, FeatureDisabled) {
  RunTest(false /* feature_enabled */, true /* data_saver_enabled */,
          true /* ect_is_slow */, false /* page_is_blacklisted */,
          true /* is_visible */);

  content::WebContentsTester::For(web_contents())
      ->NavigateAndCommit(GURL(kTestUrl));

  EXPECT_FALSE(predictor()->ShouldActOnPage(nullptr));
}

TEST_P(PreviewsLitePageRedirectPredictorUnitTest, DataSaverDisabled) {
  RunTest(true /* feature_enabled */, false /* data_saver_enabled */,
          true /* ect_is_slow */, false /* page_is_blacklisted */,
          true /* is_visible */);

  content::WebContentsTester::For(web_contents())
      ->NavigateAndCommit(GURL(kTestUrl));

  EXPECT_FALSE(predictor()->ShouldActOnPage(nullptr));
}

TEST_P(PreviewsLitePageRedirectPredictorUnitTest, ECTNotSlow) {
  RunTest(true /* feature_enabled */, true /* data_saver_enabled */,
          false /* ect_is_slow */, false /* page_is_blacklisted */,
          true /* is_visible */);

  content::WebContentsTester::For(web_contents())
      ->NavigateAndCommit(GURL(kTestUrl));

  EXPECT_FALSE(predictor()->ShouldActOnPage(nullptr));
}

TEST_P(PreviewsLitePageRedirectPredictorUnitTest, ECTNotSlowOnPreview) {
  RunTest(true /* feature_enabled */, true /* data_saver_enabled */,
          false /* ect_is_slow */, false /* page_is_blacklisted */,
          true /* is_visible */);

  content::WebContentsTester::For(web_contents())
      ->NavigateAndCommit(
          previews::GetLitePageRedirectURLForURL(GURL(kTestUrl)));

  EXPECT_TRUE(predictor()->ShouldActOnPage(nullptr));
}

TEST_P(PreviewsLitePageRedirectPredictorUnitTest, PageBlacklisted) {
  RunTest(true /* feature_enabled */, true /* data_saver_enabled */,
          true /* ect_is_slow */, true /* page_is_blacklisted */,
          true /* is_visible */);

  content::WebContentsTester::For(web_contents())
      ->NavigateAndCommit(GURL(kTestUrl));

  EXPECT_FALSE(predictor()->ShouldActOnPage(nullptr));
}

TEST_P(PreviewsLitePageRedirectPredictorUnitTest, NotVisible) {
  RunTest(true /* feature_enabled */, true /* data_saver_enabled */,
          true /* ect_is_slow */, false /* page_is_blacklisted */,
          false /* is_visible */);

  content::WebContentsTester::For(web_contents())
      ->NavigateAndCommit(GURL(kTestUrl));

  EXPECT_FALSE(predictor()->ShouldActOnPage(nullptr));
}

TEST_P(PreviewsLitePageRedirectPredictorUnitTest, InsecurePage) {
  RunTest(true /* feature_enabled */, true /* data_saver_enabled */,
          true /* ect_is_slow */, false /* page_is_blacklisted */,
          true /* is_visible */);

  content::WebContentsTester::For(web_contents())
      ->NavigateAndCommit(GURL("http://test.com"));

  EXPECT_FALSE(predictor()->ShouldActOnPage(nullptr));
}

TEST_P(PreviewsLitePageRedirectPredictorUnitTest,
       ToggleMultipleTimes_Navigations) {
  RunTest(true /* feature_enabled */, true /* data_saver_enabled */,
          true /* ect_is_slow */, false /* page_is_blacklisted */,
          true /* is_visible */);

  base::HistogramTester histogram_tester;

  content::WebContentsTester::For(web_contents())
      ->NavigateAndCommit(GURL(kTestUrl));
  EXPECT_TRUE(predictor()->ShouldActOnPage(nullptr));

  content::WebContentsTester::For(web_contents())
      ->NavigateAndCommit(GURL(kTestUrl));
  EXPECT_TRUE(predictor()->ShouldActOnPage(nullptr));

  histogram_tester.ExpectBucketCount("Previews.ServerLitePage.PredictorToggled",
                                     true, 2);
  histogram_tester.ExpectBucketCount("Previews.ServerLitePage.PredictorToggled",
                                     false, 1);
  histogram_tester.ExpectUniqueSample(GetParamedHistogramName(), true, 2);
  histogram_tester.ExpectTotalCount(GetOtherHistogramName(), 0);
}

TEST_P(PreviewsLitePageRedirectPredictorUnitTest, ToggleMultipleTimes_ECT) {
  RunTest(true /* feature_enabled */, true /* data_saver_enabled */,
          true /* ect_is_slow */, false /* page_is_blacklisted */,
          true /* is_visible */);

  base::HistogramTester histogram_tester;

  content::WebContentsTester::For(web_contents())
      ->NavigateAndCommit(GURL(kTestUrl));
  EXPECT_TRUE(predictor()->ShouldActOnPage(nullptr));

  predictor()->set_ect_is_slow(false);
  predictor()->OnEffectiveConnectionTypeChanged(
      net::EFFECTIVE_CONNECTION_TYPE_4G);

  EXPECT_FALSE(predictor()->ShouldActOnPage(nullptr));

  histogram_tester.ExpectBucketCount("Previews.ServerLitePage.PredictorToggled",
                                     true, 1);
  histogram_tester.ExpectBucketCount("Previews.ServerLitePage.PredictorToggled",
                                     false, 1);
  histogram_tester.ExpectUniqueSample(GetParamedHistogramName(), true, 1);
  histogram_tester.ExpectTotalCount(GetOtherHistogramName(), 0);
}

TEST_P(PreviewsLitePageRedirectPredictorUnitTest,
       ToggleMultipleTimes_Visibility) {
  RunTest(true /* feature_enabled */, true /* data_saver_enabled */,
          true /* ect_is_slow */, false /* page_is_blacklisted */,
          true /* is_visible */);

  base::HistogramTester histogram_tester;

  content::WebContentsTester::For(web_contents())
      ->NavigateAndCommit(GURL(kTestUrl));
  EXPECT_TRUE(predictor()->ShouldActOnPage(nullptr));

  predictor()->set_is_visible(false);
  predictor()->OnVisibilityChanged(content::Visibility::HIDDEN);
  EXPECT_FALSE(predictor()->ShouldActOnPage(nullptr));

  histogram_tester.ExpectBucketCount("Previews.ServerLitePage.PredictorToggled",
                                     true, 1);
  histogram_tester.ExpectBucketCount("Previews.ServerLitePage.PredictorToggled",
                                     false, 1);
  histogram_tester.ExpectUniqueSample(GetParamedHistogramName(), true, 1);
  histogram_tester.ExpectTotalCount(GetOtherHistogramName(), 0);
}

// True if preresolving, false for preconnecting.
INSTANTIATE_TEST_SUITE_P(/* empty prefix */,
                         PreviewsLitePageRedirectPredictorUnitTest,
                         testing::Bool());
