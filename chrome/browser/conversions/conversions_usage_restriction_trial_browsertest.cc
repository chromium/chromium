// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/bind.h"
#include "base/strings/strcat.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/embedder_support/origin_trials/features.h"
#include "components/embedder_support/switches.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/url_loader_interceptor.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace {
constexpr char kBaseDataDir[] = "content/test/data/conversions";
constexpr char kOriginTrialTestPublicKey[] =
    "dRCs+TocuKkocNKa0AtZ4awrt9XKH2SQCI6o4FY6BNA=";
}  // namespace

// Chrome layer equivalent to
// content/browser/conversion_origin_trial_browsertest.cc. Usage restrictions
// are not implemented within content/shell.
class ConversionsUsageRestrictionTrialBrowserTestBase
    : public InProcessBrowserTest {
 public:
  ConversionsUsageRestrictionTrialBrowserTestBase() = default;

  void SetUpOnMainThread() override {
    // We use a URLLoaderInterceptor, rather than the EmbeddedTestServer, since
    // the origin trial token in the response is associated with a fixed
    // origin, whereas EmbeddedTestServer serves content on a random port.
    url_loader_interceptor_ =
        std::make_unique<content::URLLoaderInterceptor>(base::BindRepeating(
            [](content::URLLoaderInterceptor::RequestParams* params) -> bool {
              if (params->url_request.url.path_piece() !=
                      "/impression_with_third_party_trial.html" &&
                  params->url_request.url.path_piece() !=
                      "/third_party_token_injector.js") {
                return false;
              }

              content::URLLoaderInterceptor::WriteResponse(
                  base::StrCat(
                      {kBaseDataDir, params->url_request.url.path_piece()}),
                  params->client.get());
              return true;
            }));
  }

  void TearDownOnMainThread() override { url_loader_interceptor_.reset(); }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitchASCII(embedder_support::kOriginTrialPublicKey,
                                    kOriginTrialTestPublicKey);
  }

 private:
  std::unique_ptr<content::URLLoaderInterceptor> url_loader_interceptor_;
};

class ConversionsOriginTrialSubsetExclusionBrowserTest
    : public ConversionsUsageRestrictionTrialBrowserTestBase {
 public:
  ConversionsOriginTrialSubsetExclusionBrowserTest() {
    // Disable the alternative usage feature, this should prevent the OT token
    // from enabling the API.
    feature_list_.InitWithFeatures(
        {features::kConversionMeasurement},
        {embedder_support::kConversionMeasurementAPIAlternativeUsage});
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(ConversionsOriginTrialSubsetExclusionBrowserTest,
                       InSubsetExclusion_TrialDisabled) {
  EXPECT_TRUE(ui_test_utils::NavigateToURL(
      browser(),
      GURL("https://example.test/impression_with_third_party_trial.html")));

  EXPECT_EQ(false, EvalJs(browser()->tab_strip_model()->GetActiveWebContents(),
                          "document.featurePolicy.features().includes('"
                          "conversion-measurement')"));
}

class ConversionsOriginTrialNoSubsetExclusionBrowserTest
    : public ConversionsUsageRestrictionTrialBrowserTestBase {
 public:
  ConversionsOriginTrialNoSubsetExclusionBrowserTest() {
    // Enable the alternative usage feature, this should allow the OT token to
    // enable the API.
    feature_list_.InitWithFeatures(
        {features::kConversionMeasurement,
         embedder_support::kConversionMeasurementAPIAlternativeUsage},
        {});
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(ConversionsOriginTrialNoSubsetExclusionBrowserTest,
                       OutOfSubsetExclusion_TrialEnabled) {
  EXPECT_TRUE(ui_test_utils::NavigateToURL(
      browser(),
      GURL("https://example.test/impression_with_third_party_trial.html")));

  EXPECT_EQ(true, EvalJs(browser()->tab_strip_model()->GetActiveWebContents(),
                         "document.featurePolicy.features().includes('"
                         "conversion-measurement')"));
}
