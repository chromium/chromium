// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/strcat.h"
#include "base/test/bind_test_util.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/embedder_support/switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/url_loader_interceptor.h"
#include "third_party/blink/public/common/features.h"

namespace federated_learning {

constexpr char kOriginTrialTestPublicKey[] =
    "dRCs+TocuKkocNKa0AtZ4awrt9XKH2SQCI6o4FY6BNA=";

constexpr char kBaseDataDir[] = "chrome/test/data/federated_learning";

class FlocOriginTrialBrowserTest : public InProcessBrowserTest {
 public:
  FlocOriginTrialBrowserTest() = default;

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();

    // We use a URLLoaderInterceptor, rather than the EmbeddedTestServer, since
    // the origin trial token in the response is associated with a fixed
    // origin, whereas EmbeddedTestServer serves content on a random port.
    url_loader_interceptor_ = std::make_unique<content::URLLoaderInterceptor>(
        base::BindLambdaForTesting(
            [&](content::URLLoaderInterceptor::RequestParams* params) -> bool {
              if (params->url_request.url.path_piece() !=
                  "/interest_cohort_api_origin_trial.html") {
                return false;
              }

              content::URLLoaderInterceptor::WriteResponse(
                  base::StrCat(
                      {kBaseDataDir, params->url_request.url.path_piece()}),
                  params->client.get());
              return true;
            }));
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitchASCII(embedder_support::kOriginTrialPublicKey,
                                    kOriginTrialTestPublicKey);
  }

  void TearDownOnMainThread() override { url_loader_interceptor_.reset(); }

  bool HasInterestCohortApi(const content::ToRenderFrameHost& adapter) {
    return EvalJs(adapter, R"(
      document.interestCohort instanceof Function
    )")
        .ExtractBool();
  }

  GURL OriginTrialEnabledURL() const {
    return GURL("https://example.test/interest_cohort_api_origin_trial.html");
  }

  GURL OriginTrialDisabledURL() const {
    return GURL("https://disabled.test/interest_cohort_api_origin_trial.html");
  }

  content::WebContents* web_contents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

 private:
  std::unique_ptr<content::URLLoaderInterceptor> url_loader_interceptor_;
};

IN_PROC_BROWSER_TEST_F(FlocOriginTrialBrowserTest, OriginTrialEnabled) {
  ui_test_utils::NavigateToURL(browser(), OriginTrialEnabledURL());

  EXPECT_TRUE(HasInterestCohortApi(web_contents()));
}

IN_PROC_BROWSER_TEST_F(FlocOriginTrialBrowserTest, OriginTrialDisabled) {
  ui_test_utils::NavigateToURL(browser(), OriginTrialDisabledURL());

  EXPECT_FALSE(HasInterestCohortApi(web_contents()));
}

}  // namespace federated_learning
