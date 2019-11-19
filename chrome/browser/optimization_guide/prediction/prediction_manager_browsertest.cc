// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/base64.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/optimization_guide/optimization_guide_session_statistic.h"
#include "chrome/browser/optimization_guide/prediction/prediction_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_switches.h"
#include "components/optimization_guide/optimization_guide_features.h"
#include "components/optimization_guide/optimization_guide_switches.h"
#include "components/page_load_metrics/browser/page_load_metrics_test_waiter.h"
#include "components/previews/core/previews_switches.h"
#include "content/public/test/browser_test_utils.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"

namespace optimization_guide {

class PredictionManagerBrowserTest : public InProcessBrowserTest {
 public:
  PredictionManagerBrowserTest() = default;
  ~PredictionManagerBrowserTest() override = default;

  void SetUp() override {
    scoped_feature_list_.InitWithFeatures(
        {optimization_guide::features::kOptimizationHints,
         optimization_guide::features::kOptimizationHintsFetching,
         optimization_guide::features::kOptimizationTargetPrediction},
        {});
    InProcessBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    https_server_.reset(
        new net::EmbeddedTestServer(net::EmbeddedTestServer::TYPE_HTTPS));
    https_server_->ServeFilesFromSourceDirectory(GetChromeTestDataDir());
    ASSERT_TRUE(https_server_->Start());
    https_url_with_content_ = https_server_->GetURL("/english_page.html");
    https_url_without_content_ = https_server_->GetURL("/empty.html");
  }

  void TearDownOnMainThread() override {
    EXPECT_TRUE(https_server_->ShutdownAndWaitUntilComplete());
    InProcessBrowserTest::TearDownOnMainThread();
  }

  void SetUpCommandLine(base::CommandLine* cmd) override {
    cmd->AppendSwitch(
        data_reduction_proxy::switches::kEnableDataReductionProxy);
    // Add switch to avoid having to see the infobar in the test.
    cmd->AppendSwitch(previews::switches::kDoNotRequireLitePageRedirectInfoBar);
    cmd->AppendSwitchASCII(optimization_guide::switches::kFetchHintsOverride,
                           "whatever.com,somehost.com");
  }

  void RegisterWithKeyedService() {
    OptimizationGuideKeyedServiceFactory::GetForProfile(browser()->profile())
        ->RegisterOptimizationTypesAndTargets(
            {optimization_guide::proto::NOSCRIPT},
            {optimization_guide::proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD});
  }

  std::unique_ptr<page_load_metrics::PageLoadMetricsTestWaiter>
  CreatePageLoadMetricsTestWaiter() {
    content::WebContents* web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    return std::make_unique<page_load_metrics::PageLoadMetricsTestWaiter>(
        web_contents);
  }

  GURL https_url_with_content() { return https_url_with_content_; }
  GURL https_url_without_content() { return https_url_without_content_; }

 private:
  GURL https_url_with_content_, https_url_without_content_;
  std::unique_ptr<net::EmbeddedTestServer> https_server_;
  base::test::ScopedFeatureList scoped_feature_list_;

  DISALLOW_COPY_AND_ASSIGN(PredictionManagerBrowserTest);
};

IN_PROC_BROWSER_TEST_F(PredictionManagerBrowserTest,
                       FCPReachedSessionStatisticsUpdated) {
  OptimizationGuideKeyedService* keyed_service =
      OptimizationGuideKeyedServiceFactory::GetForProfile(browser()->profile());

  RegisterWithKeyedService();
  auto waiter = CreatePageLoadMetricsTestWaiter();
  waiter->AddPageExpectation(
      page_load_metrics::PageLoadMetricsTestWaiter::TimingField::kFirstPaint);
  ui_test_utils::NavigateToURL(browser(), https_url_with_content());
  waiter->Wait();

  const OptimizationGuideSessionStatistic* session_fcp =
      keyed_service->GetPredictionManager()
          ->GetFCPSessionStatisticsForTesting();
  EXPECT_TRUE(session_fcp);
  EXPECT_EQ(1u, session_fcp->GetNumberOfSamples());
}

IN_PROC_BROWSER_TEST_F(PredictionManagerBrowserTest,
                       NoFCPSessionStatisticsUnchanged) {
  OptimizationGuideKeyedService* keyed_service =
      OptimizationGuideKeyedServiceFactory::GetForProfile(browser()->profile());

  RegisterWithKeyedService();
  auto waiter = CreatePageLoadMetricsTestWaiter();
  waiter->AddPageExpectation(
      page_load_metrics::PageLoadMetricsTestWaiter::TimingField::kFirstPaint);
  ui_test_utils::NavigateToURL(browser(), https_url_with_content());
  waiter->Wait();

  const OptimizationGuideSessionStatistic* session_fcp =
      keyed_service->GetPredictionManager()
          ->GetFCPSessionStatisticsForTesting();
  float current_mean = session_fcp->GetMean();

  waiter = CreatePageLoadMetricsTestWaiter();
  waiter->AddPageExpectation(
      page_load_metrics::PageLoadMetricsTestWaiter::TimingField::kFirstLayout);
  ui_test_utils::NavigateToURL(browser(), https_url_without_content());
  waiter->Wait();
  EXPECT_EQ(1u, session_fcp->GetNumberOfSamples());
  EXPECT_EQ(current_mean, session_fcp->GetMean());
}

}  // namespace optimization_guide
