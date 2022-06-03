// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/strcat.h"
#include "base/test/bind.h"
#include "chrome/browser/federated_learning/floc_id_provider.h"
#include "chrome/browser/federated_learning/floc_id_provider_factory.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/subresource_filter/subresource_filter_browser_test_harness.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/webui/federated_learning/floc_internals.mojom.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/embedder_support/switches.h"
#include "components/federated_learning/features/features.h"
#include "components/history/core/browser/history_service.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/page_load_metrics/browser/page_load_metrics_test_waiter.h"
#include "components/subresource_filter/core/common/test_ruleset_utils.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_frame_navigation_observer.h"
#include "content/public/test/test_host_resolver.h"
#include "content/public/test/test_navigation_observer.h"
#include "net/dns/mock_host_resolver.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/mojom/federated_learning/floc.mojom.h"

namespace {

const char kFlocInternalsUrl[] = "chrome://floc-internals/";

class FixedFlocIdProvider : public federated_learning::FlocIdProvider {
 public:
  FixedFlocIdProvider() = default;
  ~FixedFlocIdProvider() override = default;

  blink::mojom::InterestCohortPtr GetInterestCohortForJsApi(
      const GURL& url,
      const absl::optional<url::Origin>& top_frame_origin) const override {
    return nullptr;
  }

  federated_learning::mojom::WebUIFlocStatusPtr GetFlocStatusForWebUi()
      const override {
    return status_->Clone();
  }

  void MaybeRecordFlocToUkm(ukm::SourceId source_id) override {}

  base::Time GetApproximateNextComputeTime() const override {
    return base::Time();
  }

  void SetFlocStatus(federated_learning::mojom::WebUIFlocStatusPtr status) {
    status_ = std::move(status);
  }

 private:
  federated_learning::mojom::WebUIFlocStatusPtr status_ =
      federated_learning::mojom::WebUIFlocStatus::New();
};

}  //  namespace

class FlocInternalsBrowserTest : public InProcessBrowserTest {
 public:
  // BrowserTestBase::SetUpInProcessBrowserTestFixture
  void SetUpInProcessBrowserTestFixture() override {
    subscription_ =
        BrowserContextDependencyManager::GetInstance()
            ->RegisterCreateServicesCallbackForTesting(base::BindRepeating(
                &FlocInternalsBrowserTest::OnWillCreateBrowserContextServices,
                base::Unretained(this)));
  }

  content::WebContents* web_contents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  FixedFlocIdProvider* fixed_floc_id_provider() {
    return static_cast<FixedFlocIdProvider*>(
        federated_learning::FlocIdProviderFactory::GetForProfile(
            browser()->profile()));
  }

  std::vector<std::string> GetPageContents() {
    // Executing javascript in the WebUI requires using an isolated world in
    // which to execute the script because WebUI has a default CSP policy
    // denying "eval()", which is what EvalJs uses under the hood.
    return base::SplitString(
        EvalJs(web_contents()->GetMainFrame(), "document.body.innerText",
               content::EXECUTE_SCRIPT_DEFAULT_OPTIONS,
               /*world_id=*/1)
            .ExtractString(),
        "\n", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  }

 private:
  void OnWillCreateBrowserContextServices(content::BrowserContext* context) {
    federated_learning::FlocIdProviderFactory::GetInstance()->SetTestingFactory(
        context, base::BindRepeating(
                     &FlocInternalsBrowserTest::CreateFixedFlocIdProvider,
                     base::Unretained(this)));
  }

  std::unique_ptr<KeyedService> CreateFixedFlocIdProvider(
      content::BrowserContext* context) {
    return std::make_unique<FixedFlocIdProvider>();
  }

  base::CallbackListSubscription subscription_;
};

IN_PROC_BROWSER_TEST_F(FlocInternalsBrowserTest, EmptyResponse) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL(kFlocInternalsUrl)));

  std::vector<std::string> content_lines = GetPageContents();

  EXPECT_EQ(12u, content_lines.size());
  EXPECT_EQ("FLoC Status", content_lines[0]);
  EXPECT_EQ("id: N/A", content_lines[1]);
  EXPECT_EQ("version: N/A", content_lines[2]);
  EXPECT_EQ("last compute time: N/A", content_lines[3]);
  EXPECT_EQ("Features Enabled Status", content_lines[4]);
  EXPECT_EQ("FlocPagesWithAdResourcesDefaultIncludedInFlocComputation: false",
            content_lines[5]);
  EXPECT_EQ("InterestCohortAPIOriginTrial: false", content_lines[6]);
  EXPECT_EQ("InterestCohortFeaturePolicy: false", content_lines[7]);
  EXPECT_EQ("Parameters", content_lines[8]);
  EXPECT_EQ("FlocIdScheduledUpdateInterval: 0d-0h-0m-0s", content_lines[9]);
  EXPECT_EQ("FlocIdMinimumHistoryDomainSizeRequired: 0", content_lines[10]);
  EXPECT_EQ("FlocIdFinchConfigVersion: 0", content_lines[11]);
}

IN_PROC_BROWSER_TEST_F(FlocInternalsBrowserTest, PopulatedResponse) {
  federated_learning::mojom::WebUIFlocStatusPtr status =
      federated_learning::mojom::WebUIFlocStatus::New();
  status->id = "123";
  status->version = "chrome.2.1";
  status->compute_time = base::Time::FromDoubleT(1000);
  status->feature_pages_with_ad_resources_default_included_in_floc_computation =
      true;
  status->feature_interest_cohort_api_origin_trial = false;
  status->feature_interest_cohort_feature_policy = true;
  status->feature_param_scheduled_update_interval =
      base::Days(7) + base::Seconds(1);
  status->feature_param_minimum_history_domain_size_required = 99;
  status->feature_param_finch_config_version = 2;

  fixed_floc_id_provider()->SetFlocStatus(std::move(status));

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL(kFlocInternalsUrl)));

  std::vector<std::string> content_lines = GetPageContents();

  EXPECT_EQ(12u, content_lines.size());
  EXPECT_EQ("FLoC Status", content_lines[0]);
  EXPECT_EQ("id: 123", content_lines[1]);
  EXPECT_EQ("version: chrome.2.1", content_lines[2]);

  // It's hard to test the exact time string as it depends on the locale.
  EXPECT_NE("last compute time: N/A", content_lines[3]);
  EXPECT_NE("last compute time: Invalid Date", content_lines[3]);

  EXPECT_EQ("Features Enabled Status", content_lines[4]);
  EXPECT_EQ("FlocPagesWithAdResourcesDefaultIncludedInFlocComputation: true",
            content_lines[5]);
  EXPECT_EQ("InterestCohortAPIOriginTrial: false", content_lines[6]);
  EXPECT_EQ("InterestCohortFeaturePolicy: true", content_lines[7]);
  EXPECT_EQ("Parameters", content_lines[8]);
  EXPECT_EQ("FlocIdScheduledUpdateInterval: 7d-0h-0m-1s", content_lines[9]);
  EXPECT_EQ("FlocIdMinimumHistoryDomainSizeRequired: 99", content_lines[10]);
  EXPECT_EQ("FlocIdFinchConfigVersion: 2", content_lines[11]);
}

IN_PROC_BROWSER_TEST_F(FlocInternalsBrowserTest, ResponseWithExtremeValues) {
  federated_learning::mojom::WebUIFlocStatusPtr status =
      federated_learning::mojom::WebUIFlocStatus::New();
  status->compute_time =
      base::Time::FromDoubleT(std::numeric_limits<double>::max());
  status->feature_param_scheduled_update_interval = base::TimeDelta::Max();
  status->feature_param_minimum_history_domain_size_required =
      std::numeric_limits<int>::max();

  fixed_floc_id_provider()->SetFlocStatus(std::move(status));

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL(kFlocInternalsUrl)));

  std::vector<std::string> content_lines = GetPageContents();

  EXPECT_EQ(12u, content_lines.size());
  EXPECT_EQ("last compute time: Invalid Date", content_lines[3]);
  EXPECT_EQ("FlocIdScheduledUpdateInterval: +inf", content_lines[9]);
  EXPECT_EQ("FlocIdMinimumHistoryDomainSizeRequired: 2147483647",
            content_lines[10]);
}

IN_PROC_BROWSER_TEST_F(FlocInternalsBrowserTest, ResponseWithEmptyId) {
  federated_learning::mojom::WebUIFlocStatusPtr status =
      federated_learning::mojom::WebUIFlocStatus::New();
  status->version = "chrome.2.1";
  status->compute_time = base::Time::FromDoubleT(1000);

  fixed_floc_id_provider()->SetFlocStatus(std::move(status));

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL(kFlocInternalsUrl)));

  std::vector<std::string> content_lines = GetPageContents();

  EXPECT_EQ(12u, content_lines.size());
  EXPECT_EQ("FLoC Status", content_lines[0]);
  EXPECT_EQ("id: N/A", content_lines[1]);
  EXPECT_EQ("version: N/A", content_lines[2]);

  // It's hard to test the exact time string as it depends on the locale.
  EXPECT_NE("last compute time: N/A", content_lines[3]);
  EXPECT_NE("last compute time: Invalid Date", content_lines[3]);
}
