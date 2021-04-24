// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/prefetch/prefetch_proxy/prefetch_proxy_tab_helper.h"

#include <memory>
#include <string>
#include <vector>

#include "base/barrier_closure.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/optional.h"
#include "base/strings/string_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/navigation_predictor/navigation_predictor_keyed_service.h"
#include "chrome/browser/navigation_predictor/navigation_predictor_keyed_service_factory.h"
#include "chrome/browser/net/prediction_options.h"
#include "chrome/browser/prefetch/prefetch_proxy/prefetch_proxy_features.h"
#include "chrome/browser/prefetch/prefetch_proxy/prefetch_proxy_service.h"
#include "chrome/browser/prefetch/prefetch_proxy/prefetch_proxy_service_factory.h"
#include "chrome/browser/prefetch/prefetch_proxy/prefetched_mainframe_response_container.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_settings.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/service_worker_context.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/fake_service_worker_context.h"
#include "content/public/test/mock_navigation_handle.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/base/isolation_info.h"
#include "net/base/load_flags.h"
#include "net/base/net_errors.h"
#include "net/cookies/canonical_cookie.h"
#include "net/cookies/cookie_access_result.h"
#include "net/cookies/cookie_util.h"
#include "net/http/http_status_code.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/public/mojom/cookie_manager.mojom.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "services/network/test/test_network_connection_tracker.h"
#include "services/network/test/test_url_loader_factory.h"
#include "services/network/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace {

const int kTotalTimeDuration = 1337;

const int kConnectTimeDuration = 123;

const char kHTMLMimeType[] = "text/html";

const char kHTMLBody[] = R"(
      <!DOCTYPE HTML>
      <html>
        <head></head>
        <body></body>
      </html>)";

}  // namespace

class TestPrefetchProxyTabHelper : public PrefetchProxyTabHelper {
 public:
  explicit TestPrefetchProxyTabHelper(content::WebContents* web_contents)
      : PrefetchProxyTabHelper(web_contents) {}

  void SetURLLoaderFactory(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory) {
    url_loader_factory_ = url_loader_factory;
  }

  network::mojom::URLLoaderFactory* GetURLLoaderFactory() override {
    return url_loader_factory_.get();
  }

 private:
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
};

class PrefetchProxyTabHelperTestObserver
    : public PrefetchProxyTabHelper::Observer {
 public:
  explicit PrefetchProxyTabHelperTestObserver(
      PrefetchProxyTabHelper* tab_helper) {
    tab_helper->AddObserverForTesting(this);
  }

  void WaitForNEligibilityChecks(size_t n) {
    if (on_eligibility_result_calls_before_wait_ >= n) {
      return;
    }
    base::RunLoop run_loop;
    on_eligibility_result_callback_ = base::BarrierClosure(
        n - on_eligibility_result_calls_before_wait_, run_loop.QuitClosure());
    run_loop.Run();
  }

  void OnNewEligiblePrefetchStarted() override {
    if (!on_eligibility_result_callback_) {
      on_eligibility_result_calls_before_wait_++;
      return;
    }
    on_eligibility_result_callback_.Run();
  }

 private:
  size_t on_eligibility_result_calls_before_wait_ = 0;
  base::RepeatingClosure on_eligibility_result_callback_;
};

class PrefetchProxyTabHelperTestBase : public ChromeRenderViewHostTestHarness {
 public:
  PrefetchProxyTabHelperTestBase()
      : test_shared_loader_factory_(
            base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
                &test_url_loader_factory_)) {}
  ~PrefetchProxyTabHelperTestBase() override = default;

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();

    tab_helper_ = std::make_unique<TestPrefetchProxyTabHelper>(web_contents());
    tab_helper_->SetURLLoaderFactory(test_shared_loader_factory_);
    tab_helper_->SetServiceWorkerContextForTest(&service_worker_context_);

    SetDataSaverEnabled(true);
  }

  void TearDown() override {
    tab_helper_.reset();
    ChromeRenderViewHostTestHarness::TearDown();
  }

  void SetDataSaverEnabled(bool enabled) {
    data_reduction_proxy::DataReductionProxySettings::
        SetDataSaverEnabledForTesting(profile()->GetPrefs(), enabled);
  }

  void MakeNavigationPrediction(content::WebContents* web_contents,
                                const GURL& doc_url,
                                const std::vector<GURL>& predicted_urls) {
    NavigationPredictorKeyedServiceFactory::GetForProfile(profile())
        ->OnPredictionUpdated(
            web_contents, doc_url,
            NavigationPredictorKeyedService::PredictionSource::
                kAnchorElementsParsedFromWebPage,
            predicted_urls);
    task_environment()->RunUntilIdle();
  }

  void MakeExternalAndroidAppNavigationPrediction(
      const std::vector<GURL>& predicted_urls) {
    NavigationPredictorKeyedServiceFactory::GetForProfile(profile())
        ->OnPredictionUpdatedByExternalAndroidApp({"com.example.foo"},
                                                  predicted_urls);
    task_environment()->RunUntilIdle();
  }

  void TriggerRedirectHistogramRecording() {
    content::MockNavigationHandle handle(web_contents());
    tab_helper_->DidStartNavigation(&handle);
  }

  int RequestCount() { return test_url_loader_factory_.NumPending(); }

  PrefetchProxyTabHelper* tab_helper() const { return tab_helper_.get(); }

  int64_t ordered_eligible_pages_bitmask() const {
    return tab_helper_->srp_metrics().ordered_eligible_pages_bitmask_;
  }

  size_t prefetch_eligible_count() const {
    return tab_helper_->srp_metrics().prefetch_eligible_count_;
  }

  size_t prefetch_attempted_count() const {
    return tab_helper_->srp_metrics().prefetch_attempted_count_;
  }

  size_t prefetch_successful_count() const {
    return tab_helper_->srp_metrics().prefetch_successful_count_;
  }

  size_t prefetch_total_redirect_count() const {
    return tab_helper_->srp_metrics().prefetch_total_redirect_count_;
  }

  size_t predicted_urls_count() const {
    return tab_helper_->srp_metrics().predicted_urls_count_;
  }

  base::Optional<base::TimeDelta> navigation_to_prefetch_start() const {
    return tab_helper_->srp_metrics().navigation_to_prefetch_start_;
  }

  bool HasAfterSRPMetrics() {
    return tab_helper_->after_srp_metrics().has_value();
  }

  size_t after_srp_prefetch_eligible_count() const {
    DCHECK(tab_helper_->after_srp_metrics());
    return tab_helper_->after_srp_metrics()->prefetch_eligible_count_;
  }

  base::Optional<size_t> after_srp_clicked_link_srp_position() const {
    DCHECK(tab_helper_->after_srp_metrics());
    return tab_helper_->after_srp_metrics()->clicked_link_srp_position_;
  }

  void Navigate(const GURL& url) {
    content::MockNavigationHandle handle(web_contents());
    handle.set_url(url);
    tab_helper_->DidStartNavigation(&handle);
    handle.set_has_committed(true);
    handle.set_redirect_chain({url});
    tab_helper_->DidFinishNavigation(&handle);
  }

  void NavigateSomewhere() { Navigate(GURL("https://test.com")); }

  void NavigateSameDocument() {
    content::MockNavigationHandle handle(web_contents());
    handle.set_url(GURL("https://test.com"));
    handle.set_is_same_document(true);
    tab_helper_->DidStartNavigation(&handle);
    handle.set_has_committed(true);
    handle.set_redirect_chain({GURL("https://test.com")});
    tab_helper_->DidFinishNavigation(&handle);
  }

  void NavigateAndVerifyPrefetchStatus(
      const GURL& url,
      PrefetchProxyPrefetchStatus expected_status) {
    // Navigate to trigger an after-srp page load where the status for the given
    // url should be placed into the after srp metrics.
    Navigate(url);

    ASSERT_TRUE(tab_helper_->after_srp_metrics().has_value());
    ASSERT_TRUE(tab_helper_->after_srp_metrics()->prefetch_status_.has_value());
    EXPECT_EQ(expected_status,
              tab_helper_->after_srp_metrics()->prefetch_status_.value());
  }

  void VerifyIsolationInfo(const net::IsolationInfo& isolation_info) {
    EXPECT_FALSE(isolation_info.IsEmpty());
    EXPECT_TRUE(isolation_info.network_isolation_key().IsFullyPopulated());
    EXPECT_FALSE(isolation_info.network_isolation_key().IsTransient());
    EXPECT_FALSE(isolation_info.site_for_cookies().IsNull());
  }

  network::ResourceRequest VerifyCommonRequestState(const GURL& url,
                                                    int request_count = 1) {
    SCOPED_TRACE(url.spec());
    EXPECT_EQ(RequestCount(), request_count);

    network::TestURLLoaderFactory::PendingRequest* request =
        test_url_loader_factory_.GetPendingRequest(0);

    EXPECT_EQ(request->request.url, url);
    EXPECT_EQ(request->request.method, "GET");
    EXPECT_TRUE(request->request.enable_load_timing);
    EXPECT_EQ(request->request.load_flags,
              net::LOAD_DISABLE_CACHE | net::LOAD_PREFETCH);
    EXPECT_EQ(request->request.credentials_mode,
              network::mojom::CredentialsMode::kInclude);

    EXPECT_TRUE(request->request.trusted_params.has_value());
    VerifyIsolationInfo(request->request.trusted_params->isolation_info);

    return request->request;
  }

  std::string RequestHeader(const std::string& key) {
    if (test_url_loader_factory_.NumPending() != 1)
      return std::string();

    network::TestURLLoaderFactory::PendingRequest* request =
        test_url_loader_factory_.GetPendingRequest(0);

    std::string value;
    if (request->request.headers.GetHeader(key, &value))
      return value;

    return std::string();
  }

  void MakeResponseAndWait(
      net::HttpStatusCode http_status,
      net::Error net_error,
      const std::string& mime_type,
      std::vector<std::pair<std::string, std::string>> headers,
      const std::string& body) {
    network::TestURLLoaderFactory::PendingRequest* request =
        test_url_loader_factory_.GetPendingRequest(0);
    ASSERT_TRUE(request);

    auto head = network::CreateURLResponseHead(http_status);

    head->response_time = base::Time::Now();
    head->request_time = head->response_time -
                         base::TimeDelta::FromMilliseconds(kTotalTimeDuration);

    head->load_timing.connect_timing.connect_end =
        base::TimeTicks::Now() - base::TimeDelta::FromMinutes(2);
    head->load_timing.connect_timing.connect_start =
        head->load_timing.connect_timing.connect_end -
        base::TimeDelta::FromMilliseconds(kConnectTimeDuration);

    head->mime_type = mime_type;
    for (const auto& header : headers) {
      head->headers->AddHeader(header.first, header.second);
    }
    network::URLLoaderCompletionStatus status(net_error);
    test_url_loader_factory_.AddResponse(request->request.url, std::move(head),
                                         body, status);
    task_environment()->RunUntilIdle();
    // Clear responses in the network service so we can inspect the next request
    // that comes in before it is responded to.
    ClearResponses();
  }

  void ClearResponses() { test_url_loader_factory_.ClearResponses(); }

  bool SetCookie(content::BrowserContext* browser_context,
                 const GURL& url,
                 const std::string& value) {
    bool result = false;
    base::RunLoop run_loop;
    mojo::Remote<network::mojom::CookieManager> cookie_manager;
    content::BrowserContext::GetDefaultStoragePartition(browser_context)
        ->GetNetworkContext()
        ->GetCookieManager(cookie_manager.BindNewPipeAndPassReceiver());
    std::unique_ptr<net::CanonicalCookie> cc(net::CanonicalCookie::Create(
        url, value, base::Time::Now(), base::nullopt /* server_time */));
    EXPECT_TRUE(cc.get());

    net::CookieOptions options;
    options.set_include_httponly();
    options.set_same_site_cookie_context(
        net::CookieOptions::SameSiteCookieContext::MakeInclusive());
    cookie_manager->SetCanonicalCookie(
        *cc.get(), url, options,
        base::BindOnce(
            [](bool* result, base::RunLoop* run_loop,
               net::CookieAccessResult set_cookie_access_result) {
              *result = set_cookie_access_result.status.IsInclude();
              run_loop->Quit();
            },
            &result, &run_loop));
    run_loop.Run();
    return result;
  }

 protected:
  network::TestURLLoaderFactory test_url_loader_factory_;
  scoped_refptr<network::SharedURLLoaderFactory> test_shared_loader_factory_;

  content::FakeServiceWorkerContext service_worker_context_;
  base::test::ScopedFeatureList scoped_feature_list_;

 private:
  std::unique_ptr<TestPrefetchProxyTabHelper> tab_helper_;
};

class PrefetchProxyTabHelperIsolateDisabledTest
    : public PrefetchProxyTabHelperTestBase {
 public:
  PrefetchProxyTabHelperIsolateDisabledTest() {
    scoped_feature_list_.InitAndDisableFeature(features::kIsolatePrerenders);
  }
};

TEST_F(PrefetchProxyTabHelperIsolateDisabledTest, FeatureDisabled) {
  base::HistogramTester histogram_tester;

  NavigateSomewhere();
  GURL doc_url("https://www.google.com/search?q=cats");
  GURL prediction_url("https://www.cat-food.com/");
  MakeNavigationPrediction(web_contents(), doc_url, {prediction_url});

  EXPECT_EQ(RequestCount(), 0);
  EXPECT_EQ(predicted_urls_count(), 0U);
  EXPECT_EQ(prefetch_eligible_count(), 0U);
  EXPECT_EQ(prefetch_attempted_count(), 0U);
  EXPECT_EQ(prefetch_successful_count(), 0U);
  EXPECT_EQ(prefetch_total_redirect_count(), 0U);
  EXPECT_FALSE(navigation_to_prefetch_start().has_value());

  histogram_tester.ExpectTotalCount("PrefetchProxy.Prefetch.Mainframe.RespCode",
                                    0);
  histogram_tester.ExpectTotalCount(
      "PrefetchProxy.Prefetch.Mainframe.BodyLength", 0);
  histogram_tester.ExpectTotalCount(
      "PrefetchProxy.Prefetch.Mainframe.TotalTime", 0);
  histogram_tester.ExpectTotalCount(
      "PrefetchProxy.Prefetch.Mainframe.ConnectTime", 0);

  Navigate(prediction_url);

  histogram_tester.ExpectTotalCount(
      "PrefetchProxy.Prefetch.Mainframe.TotalRedirects", 0);

  EXPECT_FALSE(HasAfterSRPMetrics());
}

class PrefetchProxyTabHelperDataSaverDisabledTest
    : public PrefetchProxyTabHelperTestBase {
 public:
  PrefetchProxyTabHelperDataSaverDisabledTest() {
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        features::kIsolatePrerenders,
        {{"lite_mode_only", "true"},
         {"ineligible_decoy_request_probability", "0"}});
  }
};

TEST_F(PrefetchProxyTabHelperDataSaverDisabledTest,
       DataSaverDisabled_Required) {
  base::HistogramTester histogram_tester;

  SetDataSaverEnabled(false);

  NavigateSomewhere();
  GURL doc_url("https://www.google.com/search?q=cats");
  GURL prediction_url("https://www.cat-food.com/");
  MakeNavigationPrediction(web_contents(), doc_url, {prediction_url});

  EXPECT_EQ(RequestCount(), 0);
  EXPECT_EQ(predicted_urls_count(), 0U);
  EXPECT_EQ(prefetch_eligible_count(), 0U);
  EXPECT_EQ(prefetch_attempted_count(), 0U);
  EXPECT_EQ(prefetch_successful_count(), 0U);
  EXPECT_EQ(prefetch_total_redirect_count(), 0U);
  EXPECT_FALSE(navigation_to_prefetch_start().has_value());

  histogram_tester.ExpectTotalCount("PrefetchProxy.Prefetch.Mainframe.RespCode",
                                    0);
  histogram_tester.ExpectTotalCount(
      "PrefetchProxy.Prefetch.Mainframe.BodyLength", 0);
  histogram_tester.ExpectTotalCount(
      "PrefetchProxy.Prefetch.Mainframe.TotalTime", 0);
  histogram_tester.ExpectTotalCount(
      "PrefetchProxy.Prefetch.Mainframe.ConnectTime", 0);

  Navigate(prediction_url);

  histogram_tester.ExpectTotalCount(
      "PrefetchProxy.Prefetch.Mainframe.TotalRedirects", 0);

  EXPECT_FALSE(HasAfterSRPMetrics());
}

class PrefetchProxyTabHelperDataSaverDisabledNotRequiredTest
    : public PrefetchProxyTabHelperTestBase {
 public:
  PrefetchProxyTabHelperDataSaverDisabledNotRequiredTest() {
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        features::kIsolatePrerenders,
        {{"lite_mode_only", "false"},
         {"ineligible_decoy_request_probability", "0"}});
  }
};

TEST_F(PrefetchProxyTabHelperDataSaverDisabledNotRequiredTest,
       DataSaverDisabled_NotRequired) {
  base::HistogramTester histogram_tester;

  SetDataSaverEnabled(false);

  NavigateSomewhere();
  GURL doc_url("https://www.google.com/search?q=cats");
  GURL prediction_url("https://www.cat-food.com/");
  MakeNavigationPrediction(web_contents(), doc_url, {prediction_url});

  EXPECT_EQ(RequestCount(), 1);
  EXPECT_EQ(predicted_urls_count(), 1U);
  EXPECT_EQ(prefetch_eligible_count(), 1U);
  EXPECT_EQ(prefetch_attempted_count(), 1U);
  EXPECT_EQ(prefetch_successful_count(), 0U);
  EXPECT_EQ(prefetch_total_redirect_count(), 0U);
  EXPECT_TRUE(navigation_to_prefetch_start().has_value());
}

class PrefetchProxyTabHelperTest : public PrefetchProxyTabHelperTestBase {
 public:
  PrefetchProxyTabHelperTest() {
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        features::kIsolatePrerenders,
        {{"ineligible_decoy_request_probability", "0"}});
  }
};

TEST_F(PrefetchProxyTabHelperTest, GoogleSRPOnly) {
  base::HistogramTester histogram_tester;

  NavigateSomewhere();
  GURL doc_url("https://www.not-google.com/search?q=cats");
  GURL prediction_url("https://www.cat-food.com/");
  MakeNavigationPrediction(web_contents(), doc_url, {prediction_url});

  EXPECT_EQ(RequestCount(), 0);
  EXPECT_EQ(predicted_urls_count(), 0U);
  EXPECT_EQ(prefetch_eligible_count(), 0U);
  EXPECT_EQ(prefetch_attempted_count(), 0U);
  EXPECT_EQ(prefetch_successful_count(), 0U);
  EXPECT_EQ(prefetch_total_redirect_count(), 0U);
  EXPECT_FALSE(navigation_to_prefetch_start().has_value());

  histogram_tester.ExpectTotalCount("PrefetchProxy.Prefetch.Mainframe.RespCode",
                                    0);
  histogram_tester.ExpectTotalCount(
      "PrefetchProxy.Prefetch.Mainframe.BodyLength", 0);
  histogram_tester.ExpectTotalCount(
      "PrefetchProxy.Prefetch.Mainframe.TotalTime", 0);
  histogram_tester.ExpectTotalCount(
      "PrefetchProxy.Prefetch.Mainframe.ConnectTime", 0);

  Navigate(prediction_url);

  histogram_tester.ExpectTotalCount(
      "PrefetchProxy.Prefetch.Mainframe.TotalRedirects", 0);

  EXPECT_FALSE(HasAfterSRPMetrics());
}

TEST_F(PrefetchProxyTabHelperTest, SRPOnly) {
  base::HistogramTester histogram_tester;

  NavigateSomewhere();
  GURL doc_url("https://www.google.com/photos?q=cats");
  GURL prediction_url("https://www.cat-food.com/");
  MakeNavigationPrediction(web_contents(), doc_url, {prediction_url});

  EXPECT_EQ(RequestCount(), 0);
  EXPECT_EQ(predicted_urls_count(), 0U);
  EXPECT_EQ(prefetch_eligible_count(), 0U);
  EXPECT_EQ(prefetch_attempted_count(), 0U);
  EXPECT_EQ(prefetch_successful_count(), 0U);
  EXPECT_EQ(prefetch_total_redirect_count(), 0U);
  EXPECT_FALSE(navigation_to_prefetch_start().has_value());

  histogram_tester.ExpectTotalCount("PrefetchProxy.Prefetch.Mainframe.RespCode",
                                    0);
  histogram_tester.ExpectTotalCount(
      "PrefetchProxy.Prefetch.Mainframe.BodyLength", 0);
  histogram_tester.ExpectTotalCount(
      "PrefetchProxy.Prefetch.Mainframe.TotalTime", 0);
  histogram_tester.ExpectTotalCount(
      "PrefetchProxy.Prefetch.Mainframe.ConnectTime", 0);

  Navigate(prediction_url);

  histogram_tester.ExpectTotalCount(
      "PrefetchProxy.Prefetch.Mainframe.TotalRedirects", 0);

  EXPECT_FALSE(HasAfterSRPMetrics());
}

TEST_F(PrefetchProxyTabHelperTest, HTTPSPredictionsOnly) {
  base::HistogramTester histogram_tester;

  NavigateSomewhere();
  GURL doc_url("https://www.google.com/search?q=cats");
  GURL prediction_url("http://www.cat-food.com/");
  MakeNavigationPrediction(web_contents(), doc_url, {prediction_url});

  EXPECT_EQ(RequestCount(), 0);
  EXPECT_EQ(predicted_urls_count(), 1U);
  EXPECT_EQ(prefetch_eligible_count(), 0U);
  EXPECT_EQ(prefetch_attempted_count(), 0U);
  EXPECT_EQ(prefetch_successful_count(), 0U);
  EXPECT_EQ(prefetch_total_redirect_count(), 0U);
  EXPECT_FALSE(navigation_to_prefetch_start().has_value());

  histogram_tester.ExpectTotalCount("PrefetchProxy.Prefetch.Mainframe.RespCode",
                                    0);
  histogram_tester.ExpectTotalCount(
      "PrefetchProxy.Prefetch.Mainframe.BodyLength", 0);
  histogram_tester.ExpectTotalCount(
      "PrefetchProxy.Prefetch.Mainframe.TotalTime", 0);
  histogram_tester.ExpectTotalCount(
      "PrefetchProxy.Prefetch.Mainframe.ConnectTime", 0);

  NavigateAndVerifyPrefetchStatus(
      prediction_url,
      PrefetchProxyPrefetchStatus::kPrefetchNotEligibleSchemeIsNotHttps);
  EXPECT_EQ(after_srp_prefetch_eligible_count(), 0U);
  EXPECT_EQ(base::Optional<size_t>(0), after_srp_clicked_link_srp_position());

  histogram_tester.ExpectTotalCount(
      "PrefetchProxy.Prefetch.Mainframe.TotalRedirects", 0);
}

TEST_F(PrefetchProxyTabHelperTest, DontFetchGoogleLinks) {
  base::HistogramTester histogram_tester;

  NavigateSomewhere();
  GURL doc_url("https://www.google.com/search?q=cats");
  GURL prediction_url("http://www.google.com/user");
  MakeNavigationPrediction(web_contents(), doc_url, {prediction_url});

  EXPECT_EQ(RequestCount(), 0);
  EXPECT_EQ(predicted_urls_count(), 1U);
  EXPECT_EQ(prefetch_eligible_count(), 0U);
  EXPECT_EQ(prefetch_attempted_count(), 0U);
  EXPECT_EQ(prefetch_successful_count(), 0U);
  EXPECT_EQ(prefetch_total_redirect_count(), 0U);
  EXPECT_FALSE(navigation_to_prefetch_start().has_value());

  histogram_tester.ExpectTotalCount("PrefetchProxy.Prefetch.Mainframe.RespCode",
                                    0);
  histogram_tester.ExpectTotalCount(
      "PrefetchProxy.Prefetch.Mainframe.BodyLength", 0);
  histogram_tester.ExpectTotalCount(
      "PrefetchProxy.Prefetch.Mainframe.TotalTime", 0);
  histogram_tester.ExpectTotalCount(
      "PrefetchProxy.Prefetch.Mainframe.ConnectTime", 0);

  NavigateAndVerifyPrefetchStatus(
      prediction_url,
      PrefetchProxyPrefetchStatus::kPrefetchNotEligibleGoogleDomain);
  EXPECT_EQ(after_srp_prefetch_eligible_count(), 0U);
  EXPECT_EQ(base::Optional<size_t>(0), after_srp_clicked_link_srp_position());

  histogram_tester.ExpectTotalCount(
      "PrefetchProxy.Prefetch.Mainframe.TotalRedirects", 0);
}

TEST_F(PrefetchProxyTabHelperTest, DontFetchIPAddresses) {
  base::HistogramTester histogram_tester;

  NavigateSomewhere();
  GURL doc_url("https://www.google.com/search?q=cats");
  GURL prediction_url("https://123.234.123.234/meow");
  MakeNavigationPrediction(web_contents(), doc_url, {prediction_url});

  EXPECT_EQ(RequestCount(), 0);
  EXPECT_EQ(predicted_urls_count(), 1U);
  EXPECT_EQ(prefetch_eligible_count(), 0U);
  EXPECT_EQ(prefetch_attempted_count(), 0U);
  EXPECT_EQ(prefetch_successful_count(), 0U);
  EXPECT_EQ(prefetch_total_redirect_count(), 0U);
  EXPECT_FALSE(navigation_to_prefetch_start().has_value());

  histogram_tester.ExpectTotalCount("PrefetchProxy.Prefetch.Mainframe.RespCode",
                                    0);
  histogram_tester.ExpectTotalCount(
      "PrefetchProxy.Prefetch.Mainframe.BodyLength", 0);
  histogram_tester.ExpectTotalCount(
      "PrefetchProxy.Prefetch.Mainframe.TotalTime", 0);
  histogram_tester.ExpectTotalCount(
      "PrefetchProxy.Prefetch.Mainframe.ConnectTime", 0);

  NavigateAndVerifyPrefetchStatus(
      prediction_url,
      PrefetchProxyPrefetchStatus::kPrefetchNotEligibleHostIsIPAddress);
  EXPECT_EQ(after_srp_prefetch_eligible_count(), 0U);
  EXPECT_EQ(base::Optional<size_t>(0), after_srp_clicked_link_srp_position());

  histogram_tester.ExpectTotalCount(
      "PrefetchProxy.Prefetch.Mainframe.TotalRedirects", 0);
}

TEST_F(PrefetchProxyTabHelperTest, WrongWebContents) {
  base::HistogramTester histogram_tester;

  NavigateSomewhere();
  GURL doc_url("https://www.google.com/search?q=cats");
  GURL prediction_url("https://www.cat-food.com/");
  MakeNavigationPrediction(nullptr, doc_url, {prediction_url});

  EXPECT_EQ(RequestCount(), 0);
  EXPECT_EQ(predicted_urls_count(), 0U);
  EXPECT_EQ(prefetch_eligible_count(), 0U);
  EXPECT_EQ(prefetch_attempted_count(), 0U);
  EXPECT_EQ(prefetch_successful_count(), 0U);
  EXPECT_EQ(prefetch_total_redirect_count(), 0U);
  EXPECT_FALSE(navigation_to_prefetch_start().has_value());

  histogram_tester.ExpectTotalCount("PrefetchProxy.Prefetch.Mainframe.RespCode",
                                    0);
  histogram_tester.ExpectTotalCount(
      "PrefetchProxy.Prefetch.Mainframe.BodyLength", 0);
  histogram_tester.ExpectTotalCount(
      "PrefetchProxy.Prefetch.Mainframe.TotalTime", 0);
  histogram_tester.ExpectTotalCount(
      "PrefetchProxy.Prefetch.Mainframe.ConnectTime", 0);

  Navigate(prediction_url);

  EXPECT_FALSE(HasAfterSRPMetrics());

  histogram_tester.ExpectTotalCount(
      "PrefetchProxy.Prefetch.Mainframe.TotalRedirects", 0);
}

TEST_F(PrefetchProxyTabHelperTest, HasPurposePrefetchHeader) {
  base::HistogramTester histogram_tester;

  NavigateSomewhere();
  GURL doc_url("https://www.google.com/search?q=cats");
  GURL prediction_url("https://www.cat-food.com/");
  MakeNavigationPrediction(web_contents(), doc_url, {prediction_url});

  VerifyCommonRequestState(prediction_url);
  EXPECT_EQ(RequestHeader("Purpose"), "prefetch");

  EXPECT_EQ(predicted_urls_count(), 1U);
  EXPECT_EQ(prefetch_eligible_count(), 1U);
  EXPECT_EQ(prefetch_attempted_count(), 1U);
  EXPECT_EQ(prefetch_successful_count(), 0U);
  EXPECT_EQ(prefetch_total_redirect_count(), 0U);
  EXPECT_TRUE(navigation_to_prefetch_start().has_value());
}

TEST_F(PrefetchProxyTabHelperTest, NoCookies) {
  base::HistogramTester histogram_tester;

  NavigateSomewhere();
  GURL doc_url("https://www.google.com/search?q=cats");
  GURL prediction_url("https://www.cat-food.com/");

  ASSERT_TRUE(SetCookie(profile(), prediction_url, "testing"));

  MakeNavigationPrediction(web_contents(), doc_url, {prediction_url});
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(RequestCount(), 0);
  EXPECT_EQ(predicted_urls_count(), 1U);
  EXPECT_EQ(prefetch_eligible_count(), 0U);
  EXPECT_EQ(prefetch_attempted_count(), 0U);
  EXPECT_EQ(prefetch_successful_count(), 0U);
  EXPECT_EQ(prefetch_total_redirect_count(), 0U);
  EXPECT_FALSE(navigation_to_prefetch_start().has_value());

  histogram_tester.ExpectTotalCount("PrefetchProxy.Prefetch.Mainframe.RespCode",
                                    0);
  histogram_tester.ExpectTotalCount(
      "PrefetchProxy.Prefetch.Mainframe.BodyLength", 0);
  histogram_tester.ExpectTotalCount(
      "PrefetchProxy.Prefetch.Mainframe.TotalTime", 0);
  histogram_tester.ExpectTotalCount(
      "PrefetchProxy.Prefetch.Mainframe.ConnectTime", 0);

  NavigateAndVerifyPrefetchStatus(
      prediction_url,
      PrefetchProxyPrefetchStatus::kPrefetchNotEligibleUserHasCookies);
  EXPECT_EQ(after_srp_prefetch_eligible_count(), 0U);
  EXPECT_EQ(base::Optional<size_t>(0), after_srp_clicked_link_srp_position());

  histogram_tester.ExpectTotalCount(
      "PrefetchProxy.Prefetch.Mainframe.TotalRedirects", 0);
}

TEST_F(PrefetchProxyTabHelperTest, 2XXOnly) {
  base::HistogramTester histogram_tester;

  NavigateSomewhere();
  GURL doc_url("https://www.google.com/search?q=cats");
  GURL prediction_url("https://www.cat-food.com/");
  MakeNavigationPrediction(web_contents(), doc_url, {prediction_url});

  VerifyCommonRequestState(prediction_url);
  MakeResponseAndWait(net::HTTP_NOT_FOUND, net::OK, kHTMLMimeType,
                      /*headers=*/{}, kHTMLBody);

  EXPECT_EQ(predicted_urls_count(), 1U);
  EXPECT_EQ(prefetch_eligible_count(), 1U);
  EXPECT_EQ(prefetch_attempted_count(), 1U);
  EXPECT_EQ(prefetch_successful_count(), 0U);
  EXPECT_EQ(prefetch_total_redirect_count(), 0U);
  EXPECT_TRUE(navigation_to_prefetch_start().has_value());

  histogram_tester.ExpectUniqueSample(
      "PrefetchProxy.Prefetch.Mainframe.NetError", net::OK, 1);
  histogram_tester.ExpectUniqueSample(
      "PrefetchProxy.Prefetch.Mainframe.RespCode", net::HTTP_NOT_FOUND, 1);
  histogram_tester.ExpectUniqueSample(
      "PrefetchProxy.Prefetch.Mainframe.BodyLength", base::size(kHTMLBody), 1);
  histogram_tester.ExpectUniqueSample(
      "PrefetchProxy.Prefetch.Mainframe.TotalTime", kTotalTimeDuration, 1);
  histogram_tester.ExpectUniqueSample(
      "PrefetchProxy.Prefetch.Mainframe.ConnectTime", kConnectTimeDuration, 1);

  NavigateAndVerifyPrefetchStatus(
      prediction_url, PrefetchProxyPrefetchStatus::kPrefetchFailedNon2XX);
  EXPECT_EQ(after_srp_prefetch_eligible_count(), 1U);
  EXPECT_EQ(base::Optional<size_t>(0), after_srp_clicked_link_srp_position());

  histogram_tester.ExpectUniqueSample(
      "PrefetchProxy.Prefetch.Mainframe.TotalRedirects", 0, 1);
}

TEST_F(PrefetchProxyTabHelperTest, NetErrorOKOnly) {
  base::HistogramTester histogram_tester;

  NavigateSomewhere();
  GURL doc_url("https://www.google.com/search?q=cats");
  GURL prediction_url("https://www.cat-food.com/");
  MakeNavigationPrediction(web_contents(), doc_url, {prediction_url});

  VerifyCommonRequestState(prediction_url);
  MakeResponseAndWait(net::HTTP_OK, net::ERR_FAILED, kHTMLMimeType,
                      /*headers=*/{}, kHTMLBody);

  EXPECT_EQ(predicted_urls_count(), 1U);
  EXPECT_EQ(prefetch_eligible_count(), 1U);
  EXPECT_EQ(prefetch_attempted_count(), 1U);
  EXPECT_EQ(prefetch_successful_count(), 0U);
  EXPECT_EQ(prefetch_total_redirect_count(), 0U);
  EXPECT_TRUE(navigation_to_prefetch_start().has_value());

  histogram_tester.ExpectUniqueSample(
      "PrefetchProxy.Prefetch.Mainframe.NetError", std::abs(net::ERR_FAILED),
      1);
  histogram_tester.ExpectTotalCount("PrefetchProxy.Prefetch.Mainframe.RespCode",
                                    0);
  histogram_tester.ExpectTotalCount(
      "PrefetchProxy.Prefetch.Mainframe.BodyLength", 0);
  histogram_tester.ExpectTotalCount(
      "PrefetchProxy.Prefetch.Mainframe.TotalTime", 0);
  histogram_tester.ExpectTotalCount(
      "PrefetchProxy.Prefetch.Mainframe.ConnectTime", 0);

  NavigateAndVerifyPrefetchStatus(
      prediction_url, PrefetchProxyPrefetchStatus::kPrefetchFailedNetError);
  EXPECT_EQ(after_srp_prefetch_eligible_count(), 1U);
  EXPECT_EQ(base::Optional<size_t>(0), after_srp_clicked_link_srp_position());

  histogram_tester.ExpectUniqueSample(
      "PrefetchProxy.Prefetch.Mainframe.TotalRedirects", 0, 1);
}

TEST_F(PrefetchProxyTabHelperTest, NonHTML) {
  base::HistogramTester histogram_tester;

  NavigateSomewhere();
  GURL doc_url("https://www.google.com/search?q=cats");
  GURL prediction_url("https://www.cat-food.com/");
  MakeNavigationPrediction(web_contents(), doc_url, {prediction_url});

  std::string body = "console.log('Hello world');";
  VerifyCommonRequestState(prediction_url);
  MakeResponseAndWait(net::HTTP_OK, net::OK, "application/javascript",
                      /*headers=*/{}, body);

  EXPECT_EQ(predicted_urls_count(), 1U);
  EXPECT_EQ(prefetch_eligible_count(), 1U);
  EXPECT_EQ(prefetch_attempted_count(), 1U);
  EXPECT_EQ(prefetch_successful_count(), 0U);
  EXPECT_EQ(prefetch_total_redirect_count(), 0U);
  EXPECT_TRUE(navigation_to_prefetch_start().has_value());

  histogram_tester.ExpectUniqueSample(
      "PrefetchProxy.Prefetch.Mainframe.NetError", net::OK, 1);
  histogram_tester.ExpectUniqueSample(
      "PrefetchProxy.Prefetch.Mainframe.RespCode", net::HTTP_OK, 1);
  histogram_tester.ExpectUniqueSample(
      "PrefetchProxy.Prefetch.Mainframe.BodyLength", body.size(), 1);
  histogram_tester.ExpectUniqueSample(
      "PrefetchProxy.Prefetch.Mainframe.TotalTime", kTotalTimeDuration, 1);
  histogram_tester.ExpectUniqueSample(
      "PrefetchProxy.Prefetch.Mainframe.ConnectTime", kConnectTimeDuration, 1);

  NavigateAndVerifyPrefetchStatus(
      prediction_url, PrefetchProxyPrefetchStatus::kPrefetchFailedNotHTML);
  EXPECT_EQ(after_srp_prefetch_eligible_count(), 1U);
  EXPECT_EQ(base::Optional<size_t>(0), after_srp_clicked_link_srp_position());

  histogram_tester.ExpectUniqueSample(
      "PrefetchProxy.Prefetch.Mainframe.TotalRedirects", 0, 1);
}

TEST_F(PrefetchProxyTabHelperTest, UserSettingDisabled) {
  base::HistogramTester histogram_tester;

  profile()->GetPrefs()->SetInteger(
      prefs::kNetworkPredictionOptions,
      chrome_browser_net::NETWORK_PREDICTION_NEVER);

  NavigateSomewhere();
  GURL doc_url("https://www.google.com/search?q=cats");
  GURL prediction_url("https://www.cat-food.com/");
  MakeNavigationPrediction(web_contents(), doc_url, {prediction_url});
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(RequestCount(), 0);
  EXPECT_EQ(predicted_urls_count(), 0U);
  EXPECT_EQ(prefetch_eligible_count(), 0U);
  EXPECT_EQ(prefetch_attempted_count(), 0U);
  EXPECT_EQ(prefetch_successful_count(), 0U);
  EXPECT_EQ(prefetch_total_redirect_count(), 0U);
  EXPECT_FALSE(navigation_to_prefetch_start().has_value());

  histogram_tester.ExpectTotalCount("PrefetchProxy.Prefetch.Mainframe.RespCode",
                                    0);
  histogram_tester.ExpectTotalCount(
      "PrefetchProxy.Prefetch.Mainframe.BodyLength", 0);
  histogram_tester.ExpectTotalCount(
      "PrefetchProxy.Prefetch.Mainframe.TotalTime", 0);
  histogram_tester.ExpectTotalCount(
      "PrefetchProxy.Prefetch.Mainframe.ConnectTime", 0);

  Navigate(prediction_url);

  EXPECT_FALSE(HasAfterSRPMetrics());

  histogram_tester.ExpectTotalCount(
      "PrefetchProxy.Prefetch.Mainframe.TotalRedirects", 0);
}

// Verify that prefetch proxy is not triggered if the predictions for next
// likely navigations are provided by external Android app.
TEST_F(PrefetchProxyTabHelperTest, ExternalAndroidApp) {
  NavigateSomewhere();
  GURL doc_url("https://www.google.com/search?q=cats");
  GURL prediction_url("https://www.cat-food.com/");
  MakeExternalAndroidAppNavigationPrediction({prediction_url});
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(RequestCount(), 0);
}

TEST_F(PrefetchProxyTabHelperTest, IgnoreSameDocNavigations) {
  base::HistogramTester histogram_tester;

  NavigateSomewhere();
  GURL doc_url("https://www.google.com/search?q=cats");
  GURL prediction_url("https://www.cat-food.com/");
  MakeNavigationPrediction(web_contents(), doc_url, {prediction_url});

  network::ResourceRequest request = VerifyCommonRequestState(prediction_url);
  MakeResponseAndWait(net::HTTP_OK, net::OK, kHTMLMimeType,
                      {{"X-Testing", "Hello World"}}, kHTMLBody);

  NavigateSameDocument();

  EXPECT_EQ(predicted_urls_count(), 1U);
  EXPECT_EQ(prefetch_eligible_count(), 1U);
  EXPECT_EQ(prefetch_attempted_count(), 1U);
  EXPECT_EQ(prefetch_successful_count(), 1U);
  EXPECT_EQ(prefetch_total_redirect_count(), 0U);
  EXPECT_TRUE(navigation_to_prefetch_start().has_value());

  histogram_tester.ExpectUniqueSample(
      "PrefetchProxy.Prefetch.Mainframe.NetError", net::OK, 1);
  histogram_tester.ExpectUniqueSample(
      "PrefetchProxy.Prefetch.Mainframe.RespCode", net::HTTP_OK, 1);
  histogram_tester.ExpectUniqueSample(
      "PrefetchProxy.Prefetch.Mainframe.BodyLength", base::size(kHTMLBody), 1);
  histogram_tester.ExpectUniqueSample(
      "PrefetchProxy.Prefetch.Mainframe.TotalTime", kTotalTimeDuration, 1);
  histogram_tester.ExpectUniqueSample(
      "PrefetchProxy.Prefetch.Mainframe.ConnectTime", kConnectTimeDuration, 1);

  NavigateAndVerifyPrefetchStatus(
      prediction_url, PrefetchProxyPrefetchStatus::kPrefetchSuccessful);
  EXPECT_EQ(after_srp_prefetch_eligible_count(), 1U);
  EXPECT_EQ(base::Optional<size_t>(0), after_srp_clicked_link_srp_position());

  histogram_tester.ExpectUniqueSample(
      "PrefetchProxy.Prefetch.Mainframe.TotalRedirects", 0, 1);
}

TEST_F(PrefetchProxyTabHelperTest, SuccessCase) {
  base::HistogramTester histogram_tester;

  NavigateSomewhere();
  GURL doc_url("https://www.google.com/search?q=cats");
  GURL prediction_url("https://www.cat-food.com/");
  MakeNavigationPrediction(web_contents(), doc_url, {prediction_url});

  network::ResourceRequest request = VerifyCommonRequestState(prediction_url);
  MakeResponseAndWait(net::HTTP_OK, net::OK, kHTMLMimeType,
                      {{"X-Testing", "Hello World"}}, kHTMLBody);

  std::unique_ptr<PrefetchedMainframeResponseContainer> resp =
      tab_helper()->TakePrefetchResponse(prediction_url);
  ASSERT_TRUE(resp);
  EXPECT_EQ(*resp->TakeBody(), kHTMLBody);

  network::mojom::URLResponseHeadPtr head = resp->TakeHead();
  EXPECT_TRUE(head->headers->HasHeaderValue("X-Testing", "Hello World"));

  EXPECT_TRUE(resp->isolation_info().IsEqualForTesting(
      request.trusted_params->isolation_info));
  VerifyIsolationInfo(resp->isolation_info());

  EXPECT_EQ(predicted_urls_count(), 1U);
  EXPECT_EQ(prefetch_eligible_count(), 1U);
  EXPECT_EQ(prefetch_attempted_count(), 1U);
  EXPECT_EQ(prefetch_successful_count(), 1U);
  EXPECT_EQ(prefetch_total_redirect_count(), 0U);
  EXPECT_TRUE(navigation_to_prefetch_start().has_value());

  histogram_tester.ExpectUniqueSample(
      "PrefetchProxy.Prefetch.Mainframe.NetError", net::OK, 1);
  histogram_tester.ExpectUniqueSample(
      "PrefetchProxy.Prefetch.Mainframe.RespCode", net::HTTP_OK, 1);
  histogram_tester.ExpectUniqueSample(
      "PrefetchProxy.Prefetch.Mainframe.BodyLength", base::size(kHTMLBody), 1);
  histogram_tester.ExpectUniqueSample(
      "PrefetchProxy.Prefetch.Mainframe.TotalTime", kTotalTimeDuration, 1);
  histogram_tester.ExpectUniqueSample(
      "PrefetchProxy.Prefetch.Mainframe.ConnectTime", kConnectTimeDuration, 1);

  NavigateAndVerifyPrefetchStatus(
      prediction_url, PrefetchProxyPrefetchStatus::kPrefetchSuccessful);
  EXPECT_EQ(after_srp_prefetch_eligible_count(), 1U);
  EXPECT_EQ(base::Optional<size_t>(0), after_srp_clicked_link_srp_position());

  histogram_tester.ExpectUniqueSample(
      "PrefetchProxy.Prefetch.Mainframe.TotalRedirects", 0, 1);
}

TEST_F(PrefetchProxyTabHelperTest, AfterSRPLinkNotOnSRP) {
  base::HistogramTester histogram_tester;

  NavigateSomewhere();
  GURL doc_url("https://www.google.com/search?q=cats");
  GURL prediction_url("https://www.cat-food.com/");
  MakeNavigationPrediction(web_contents(), doc_url, {prediction_url});

  network::ResourceRequest request = VerifyCommonRequestState(prediction_url);
  MakeResponseAndWait(net::HTTP_OK, net::OK, kHTMLMimeType,
                      {{"X-Testing", "Hello World"}}, kHTMLBody);

  EXPECT_EQ(predicted_urls_count(), 1U);
  EXPECT_EQ(prefetch_eligible_count(), 1U);
  EXPECT_EQ(prefetch_attempted_count(), 1U);
  EXPECT_EQ(prefetch_successful_count(), 1U);
  EXPECT_EQ(prefetch_total_redirect_count(), 0U);
  EXPECT_TRUE(navigation_to_prefetch_start().has_value());

  histogram_tester.ExpectUniqueSample(
      "PrefetchProxy.Prefetch.Mainframe.NetError", net::OK, 1);
  histogram_tester.ExpectUniqueSample(
      "PrefetchProxy.Prefetch.Mainframe.RespCode", net::HTTP_OK, 1);
  histogram_tester.ExpectUniqueSample(
      "PrefetchProxy.Prefetch.Mainframe.BodyLength", base::size(kHTMLBody), 1);
  histogram_tester.ExpectUniqueSample(
      "PrefetchProxy.Prefetch.Mainframe.TotalTime", kTotalTimeDuration, 1);
  histogram_tester.ExpectUniqueSample(
      "PrefetchProxy.Prefetch.Mainframe.ConnectTime", kConnectTimeDuration, 1);

  NavigateAndVerifyPrefetchStatus(
      GURL("https://wasnt-on-srp.com"),
      PrefetchProxyPrefetchStatus::kNavigatedToLinkNotOnSRP);
  EXPECT_EQ(after_srp_prefetch_eligible_count(), 1U);
  EXPECT_EQ(base::nullopt, after_srp_clicked_link_srp_position());

  histogram_tester.ExpectUniqueSample(
      "PrefetchProxy.Prefetch.Mainframe.TotalRedirects", 0, 1);
}

TEST_F(PrefetchProxyTabHelperTest, OrderedBitMask) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      "isolated-prerender-unlimited-prefetches");

  NavigateSomewhere();
  MakeNavigationPrediction(web_contents(),
                           GURL("https://www.google.com/search?q=cats"),
                           {
                               GURL("http://not-eligible-1.com"),
                               GURL("http://not-eligible-2.com"),
                               GURL("https://eligible-1.com"),
                               GURL("https://eligible-2.com"),
                               GURL("https://eligible-3.com"),
                               GURL("http://not-eligible-3.com"),
                           });

  EXPECT_EQ(predicted_urls_count(), 6U);
  EXPECT_EQ(prefetch_eligible_count(), 3U);
  EXPECT_EQ(ordered_eligible_pages_bitmask(), 0b011100);
}

TEST_F(PrefetchProxyTabHelperTest, NumberOfPrefetches_UnlimitedByCmdLine) {
  base::HistogramTester histogram_tester;

  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      "isolated-prerender-unlimited-prefetches");

  NavigateSomewhere();
  GURL doc_url("https://www.google.com/search?q=cats");
  GURL prediction_url_1("https://www.cat-food.com/");
  GURL prediction_url_2("https://www.dogs-r-dumb.com/");
  GURL prediction_url_3("https://www.catz-rule.com/");
  MakeNavigationPrediction(
      web_contents(), doc_url,
      {prediction_url_1, prediction_url_2, prediction_url_3});

  VerifyCommonRequestState(prediction_url_1);
  MakeResponseAndWait(net::HTTP_OK, net::OK, kHTMLMimeType, {}, kHTMLBody);
  VerifyCommonRequestState(prediction_url_2);
  // Failed responses do not retry or attempt more requests in the list.
  MakeResponseAndWait(net::HTTP_OK, net::ERR_FAILED, kHTMLMimeType, {},
                      kHTMLBody);
  VerifyCommonRequestState(prediction_url_3);
  MakeResponseAndWait(net::HTTP_OK, net::OK, kHTMLMimeType, {}, kHTMLBody);

  EXPECT_EQ(RequestCount(), 0);
  EXPECT_EQ(predicted_urls_count(), 3U);
  EXPECT_EQ(prefetch_eligible_count(), 3U);
  EXPECT_EQ(prefetch_attempted_count(), 3U);
  EXPECT_EQ(prefetch_successful_count(), 2U);
  EXPECT_EQ(prefetch_total_redirect_count(), 0U);
  EXPECT_TRUE(navigation_to_prefetch_start().has_value());

  histogram_tester.ExpectBucketCount(
      "PrefetchProxy.Prefetch.Mainframe.NetError", net::OK, 2);
  histogram_tester.ExpectBucketCount(
      "PrefetchProxy.Prefetch.Mainframe.NetError", std::abs(net::ERR_FAILED),
      1);
  histogram_tester.ExpectTotalCount("PrefetchProxy.Prefetch.Mainframe.NetError",
                                    3);

  histogram_tester.ExpectUniqueSample(
      "PrefetchProxy.Prefetch.Mainframe.RespCode", net::HTTP_OK, 2);

  histogram_tester.ExpectUniqueSample(
      "PrefetchProxy.Prefetch.Mainframe.BodyLength", base::size(kHTMLBody), 2);

  histogram_tester.ExpectUniqueSample(
      "PrefetchProxy.Prefetch.Mainframe.TotalTime", kTotalTimeDuration, 2);
  histogram_tester.ExpectUniqueSample(
      "PrefetchProxy.Prefetch.Mainframe.ConnectTime", kConnectTimeDuration, 2);

  NavigateAndVerifyPrefetchStatus(
      prediction_url_1, PrefetchProxyPrefetchStatus::kPrefetchSuccessful);
  EXPECT_EQ(after_srp_prefetch_eligible_count(), 3U);
  EXPECT_EQ(base::Optional<size_t>(0), after_srp_clicked_link_srp_position());

  histogram_tester.ExpectUniqueSample(
      "PrefetchProxy.Prefetch.Mainframe.TotalRedirects", 0, 1);
}

TEST_F(PrefetchProxyTabHelperTest, PrefetchingNotStartedWhileInvisible) {
  web_contents()->WasHidden();

  NavigateSomewhere();
  GURL doc_url("https://www.google.com/search?q=cats");
  GURL prediction_url("https://www.cat-food.com/");

  MakeNavigationPrediction(web_contents(), doc_url, {prediction_url});

  EXPECT_EQ(RequestCount(), 0);
  EXPECT_EQ(predicted_urls_count(), 1U);
  EXPECT_EQ(prefetch_eligible_count(), 1U);
  EXPECT_EQ(prefetch_attempted_count(), 0U);
  EXPECT_EQ(prefetch_successful_count(), 0U);
  EXPECT_EQ(prefetch_total_redirect_count(), 0U);
  EXPECT_TRUE(navigation_to_prefetch_start().has_value());
}

TEST_F(PrefetchProxyTabHelperTest, PrefetchingPausedWhenInvisible) {
  base::HistogramTester histogram_tester;

  NavigateSomewhere();
  GURL doc_url("https://www.google.com/search?q=cats");
  GURL prediction_url_1("https://www.cat-food.com/");
  GURL prediction_url_2("https://www.dogs-r-dumb.com/");

  MakeNavigationPrediction(web_contents(), doc_url,
                           {prediction_url_1, prediction_url_2});
  VerifyCommonRequestState(prediction_url_1);

  // When hidden, the current prefetch is allowed to finish.
  web_contents()->WasHidden();
  VerifyCommonRequestState(prediction_url_1);
  MakeResponseAndWait(net::HTTP_OK, net::OK, kHTMLMimeType, {}, kHTMLBody);

  // But no more prefetches should start when hidden.
  EXPECT_EQ(RequestCount(), 0);
  EXPECT_EQ(predicted_urls_count(), 2U);
  EXPECT_EQ(prefetch_eligible_count(), 2U);
  EXPECT_EQ(prefetch_attempted_count(), 1U);
  EXPECT_EQ(prefetch_successful_count(), 1U);
  EXPECT_EQ(prefetch_total_redirect_count(), 0U);
  EXPECT_TRUE(navigation_to_prefetch_start().has_value());

  histogram_tester.ExpectUniqueSample(
      "PrefetchProxy.Prefetch.Mainframe.NetError", net::OK, 1);
  histogram_tester.ExpectUniqueSample(
      "PrefetchProxy.Prefetch.Mainframe.RespCode", net::HTTP_OK, 1);
  histogram_tester.ExpectUniqueSample(
      "PrefetchProxy.Prefetch.Mainframe.BodyLength", base::size(kHTMLBody), 1);
  histogram_tester.ExpectUniqueSample(
      "PrefetchProxy.Prefetch.Mainframe.TotalTime", kTotalTimeDuration, 1);
  histogram_tester.ExpectUniqueSample(
      "PrefetchProxy.Prefetch.Mainframe.ConnectTime", kConnectTimeDuration, 1);

  TriggerRedirectHistogramRecording();
  histogram_tester.ExpectUniqueSample(
      "PrefetchProxy.Prefetch.Mainframe.TotalRedirects", 0, 1);
}

TEST_F(PrefetchProxyTabHelperTest, PrefetchingRestartedWhenVisible) {
  web_contents()->WasHidden();

  NavigateSomewhere();
  GURL doc_url("https://www.google.com/search?q=cats");
  GURL prediction_url("https://www.cat-food.com/");

  MakeNavigationPrediction(web_contents(), doc_url, {prediction_url});

  EXPECT_EQ(RequestCount(), 0);
  EXPECT_EQ(predicted_urls_count(), 1U);
  EXPECT_EQ(prefetch_eligible_count(), 1U);
  EXPECT_EQ(prefetch_attempted_count(), 0U);
  EXPECT_EQ(prefetch_successful_count(), 0U);
  EXPECT_EQ(prefetch_total_redirect_count(), 0U);
  EXPECT_TRUE(navigation_to_prefetch_start().has_value());
  base::TimeDelta first_nav_to_prefetch_start =
      navigation_to_prefetch_start().value();

  web_contents()->WasShown();

  VerifyCommonRequestState(prediction_url);
  EXPECT_EQ(prefetch_eligible_count(), 1U);
  EXPECT_EQ(prefetch_attempted_count(), 1U);
  EXPECT_EQ(prefetch_successful_count(), 0U);
  EXPECT_EQ(prefetch_total_redirect_count(), 0U);
  EXPECT_TRUE(navigation_to_prefetch_start().has_value());
  EXPECT_EQ(first_nav_to_prefetch_start,
            navigation_to_prefetch_start().value());
}

TEST_F(PrefetchProxyTabHelperTest, ServiceWorkerRegistered) {
  NavigateSomewhere();
  GURL doc_url("https://www.google.com/search?q=cats");
  GURL prediction_url("https://www.cat-food.com/");

  service_worker_context_.AddRegistrationToRegisteredOrigins(
      url::Origin::Create(prediction_url));

  MakeNavigationPrediction(web_contents(), doc_url, {prediction_url});

  EXPECT_EQ(RequestCount(), 0);
  EXPECT_EQ(predicted_urls_count(), 1U);
  EXPECT_EQ(prefetch_eligible_count(), 0U);
  EXPECT_EQ(prefetch_attempted_count(), 0U);
  EXPECT_EQ(prefetch_successful_count(), 0U);
  EXPECT_EQ(prefetch_total_redirect_count(), 0U);
  EXPECT_FALSE(navigation_to_prefetch_start().has_value());

  NavigateAndVerifyPrefetchStatus(
      prediction_url,
      PrefetchProxyPrefetchStatus::kPrefetchNotEligibleUserHasServiceWorker);
  EXPECT_EQ(after_srp_prefetch_eligible_count(), 0U);
  EXPECT_EQ(base::Optional<size_t>(0), after_srp_clicked_link_srp_position());
}

TEST_F(PrefetchProxyTabHelperTest, ServiceWorkerNotRegistered) {
  NavigateSomewhere();
  GURL doc_url("https://www.google.com/search?q=cats");
  GURL prediction_url("https://www.cat-food.com/");
  GURL service_worker_registration("https://www.service-worker.com/");

  service_worker_context_.AddRegistrationToRegisteredOrigins(
      url::Origin::Create(service_worker_registration));

  MakeNavigationPrediction(web_contents(), doc_url, {prediction_url});

  VerifyCommonRequestState(prediction_url);

  EXPECT_EQ(predicted_urls_count(), 1U);
  EXPECT_EQ(prefetch_eligible_count(), 1U);
  EXPECT_EQ(prefetch_attempted_count(), 1U);
  EXPECT_EQ(prefetch_successful_count(), 0U);
  EXPECT_EQ(prefetch_total_redirect_count(), 0U);
  EXPECT_TRUE(navigation_to_prefetch_start().has_value());
}

class PrefetchProxyTabHelperWithDecoyTest
    : public PrefetchProxyTabHelperTestBase {
 public:
  PrefetchProxyTabHelperWithDecoyTest() {
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        features::kIsolatePrerenders,
        {{"ineligible_decoy_request_probability", "1"}});
  }
};

TEST_F(PrefetchProxyTabHelperWithDecoyTest, Cookies) {
  base::HistogramTester histogram_tester;

  NavigateSomewhere();
  GURL doc_url("https://www.google.com/search?q=cats");
  GURL prediction_url("https://www.cat-food.com/");

  ASSERT_TRUE(SetCookie(profile(), prediction_url, "testing"));

  MakeNavigationPrediction(web_contents(), doc_url, {prediction_url});
  base::RunLoop().RunUntilIdle();

  // Expect a request to be put on the network, but not be used.
  EXPECT_EQ(RequestCount(), 1);
  VerifyCommonRequestState(prediction_url);
  MakeResponseAndWait(net::HTTP_OK, net::OK, kHTMLMimeType, {}, kHTMLBody);

  EXPECT_EQ(predicted_urls_count(), 1U);
  EXPECT_EQ(prefetch_eligible_count(), 0U);
  EXPECT_EQ(prefetch_attempted_count(), 0U);
  EXPECT_EQ(prefetch_successful_count(), 0U);
  EXPECT_EQ(prefetch_total_redirect_count(), 0U);
  EXPECT_TRUE(navigation_to_prefetch_start().has_value());

  histogram_tester.ExpectTotalCount("PrefetchProxy.Prefetch.Mainframe.RespCode",
                                    0);
  histogram_tester.ExpectTotalCount(
      "PrefetchProxy.Prefetch.Mainframe.BodyLength", 0);
  histogram_tester.ExpectTotalCount(
      "PrefetchProxy.Prefetch.Mainframe.TotalTime", 0);
  histogram_tester.ExpectTotalCount(
      "PrefetchProxy.Prefetch.Mainframe.ConnectTime", 0);

  NavigateAndVerifyPrefetchStatus(
      prediction_url, PrefetchProxyPrefetchStatus::kPrefetchIsPrivacyDecoy);
  EXPECT_EQ(after_srp_prefetch_eligible_count(), 0U);
  EXPECT_EQ(base::Optional<size_t>(0), after_srp_clicked_link_srp_position());

  histogram_tester.ExpectTotalCount(
      "PrefetchProxy.Prefetch.Mainframe.TotalRedirects", 0);
}

TEST_F(PrefetchProxyTabHelperWithDecoyTest, ServiceWorkerRegistered) {
  base::HistogramTester histogram_tester;

  NavigateSomewhere();
  GURL doc_url("https://www.google.com/search?q=cats");
  GURL prediction_url("https://www.cat-food.com/");

  service_worker_context_.AddRegistrationToRegisteredOrigins(
      url::Origin::Create(prediction_url));

  MakeNavigationPrediction(web_contents(), doc_url, {prediction_url});

  // Expect a request to be put on the network, but not be used.
  EXPECT_EQ(RequestCount(), 1);
  VerifyCommonRequestState(prediction_url);
  MakeResponseAndWait(net::HTTP_OK, net::OK, kHTMLMimeType, {}, kHTMLBody);

  EXPECT_EQ(predicted_urls_count(), 1U);
  EXPECT_EQ(prefetch_eligible_count(), 0U);
  EXPECT_EQ(prefetch_attempted_count(), 0U);
  EXPECT_EQ(prefetch_successful_count(), 0U);
  EXPECT_EQ(prefetch_total_redirect_count(), 0U);
  EXPECT_TRUE(navigation_to_prefetch_start().has_value());

  histogram_tester.ExpectTotalCount("PrefetchProxy.Prefetch.Mainframe.RespCode",
                                    0);
  histogram_tester.ExpectTotalCount(
      "PrefetchProxy.Prefetch.Mainframe.BodyLength", 0);
  histogram_tester.ExpectTotalCount(
      "PrefetchProxy.Prefetch.Mainframe.TotalTime", 0);
  histogram_tester.ExpectTotalCount(
      "PrefetchProxy.Prefetch.Mainframe.ConnectTime", 0);

  NavigateAndVerifyPrefetchStatus(
      prediction_url, PrefetchProxyPrefetchStatus::kPrefetchIsPrivacyDecoy);
  EXPECT_EQ(after_srp_prefetch_eligible_count(), 0U);
  EXPECT_EQ(base::Optional<size_t>(0), after_srp_clicked_link_srp_position());

  histogram_tester.ExpectTotalCount(
      "PrefetchProxy.Prefetch.Mainframe.TotalRedirects", 0);
}

class PrefetchProxyTabHelperBodyLimitTest
    : public PrefetchProxyTabHelperTestBase {
 public:
  PrefetchProxyTabHelperBodyLimitTest() {
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        features::kIsolatePrerenders,
        {{"max_mainframe_body_length_kb", "0"},
         {"ineligible_decoy_request_probability", "0"}});
  }
};

TEST_F(PrefetchProxyTabHelperBodyLimitTest, ResponseBodyLimit) {
  base::HistogramTester histogram_tester;

  NavigateSomewhere();
  GURL doc_url("https://www.google.com/search?q=cats");
  GURL prediction_url("https://www.cat-food.com/");
  MakeNavigationPrediction(web_contents(), doc_url, {prediction_url});

  network::ResourceRequest request = VerifyCommonRequestState(prediction_url);
  MakeResponseAndWait(net::HTTP_OK, net::OK, kHTMLMimeType, /*headers=*/{},
                      kHTMLBody);

  EXPECT_EQ(predicted_urls_count(), 1U);
  EXPECT_EQ(prefetch_eligible_count(), 1U);
  EXPECT_EQ(prefetch_attempted_count(), 1U);
  EXPECT_EQ(prefetch_successful_count(), 0U);
  EXPECT_EQ(prefetch_total_redirect_count(), 0U);
  EXPECT_TRUE(navigation_to_prefetch_start().has_value());

  histogram_tester.ExpectUniqueSample(
      "PrefetchProxy.Prefetch.Mainframe.NetError",
      std::abs(net::ERR_INSUFFICIENT_RESOURCES), 1);
  histogram_tester.ExpectTotalCount("PrefetchProxy.Prefetch.Mainframe.RespCode",
                                    0);
  histogram_tester.ExpectTotalCount(
      "PrefetchProxy.Prefetch.Mainframe.BodyLength", 0);
  histogram_tester.ExpectTotalCount(
      "PrefetchProxy.Prefetch.Mainframe.TotalTime", 0);
  histogram_tester.ExpectTotalCount(
      "PrefetchProxy.Prefetch.Mainframe.ConnectTime", 0);

  NavigateAndVerifyPrefetchStatus(
      prediction_url, PrefetchProxyPrefetchStatus::kPrefetchFailedNetError);
  EXPECT_EQ(after_srp_prefetch_eligible_count(), 1U);
  EXPECT_EQ(base::Optional<size_t>(0), after_srp_clicked_link_srp_position());

  histogram_tester.ExpectUniqueSample(
      "PrefetchProxy.Prefetch.Mainframe.TotalRedirects", 0, 1);
}

class PrefetchProxyTabHelperPredictionPositionsTest
    : public PrefetchProxyTabHelperTestBase {
 public:
  PrefetchProxyTabHelperPredictionPositionsTest() {
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        features::kIsolatePrerenders,
        {{"prefetch_positions", "0"},
         {"ineligible_decoy_request_probability", "0"}});
  }
};

TEST_F(PrefetchProxyTabHelperPredictionPositionsTest,
       EligiblePredictionPositions) {
  base::HistogramTester histogram_tester;

  NavigateSomewhere();
  GURL doc_url("https://www.google.com/search?q=cats");
  GURL ineligible_url("http://www.meow.com/");
  GURL eligible_url("https://www.cat-food.com/");
  MakeNavigationPrediction(web_contents(), doc_url,
                           {ineligible_url, eligible_url});

  EXPECT_EQ(RequestCount(), 0);
  EXPECT_EQ(predicted_urls_count(), 2U);
  EXPECT_EQ(prefetch_eligible_count(), 1U);
  EXPECT_EQ(prefetch_attempted_count(), 0U);
  EXPECT_EQ(prefetch_successful_count(), 0U);
  EXPECT_EQ(prefetch_total_redirect_count(), 0U);
  EXPECT_FALSE(navigation_to_prefetch_start().has_value());

  NavigateAndVerifyPrefetchStatus(
      eligible_url, PrefetchProxyPrefetchStatus::kPrefetchPositionIneligible);
  EXPECT_EQ(after_srp_prefetch_eligible_count(), 1U);
  EXPECT_EQ(base::Optional<size_t>(1), after_srp_clicked_link_srp_position());
}

class PrefetchProxyTabHelperNoPrefetchesTest
    : public PrefetchProxyTabHelperTestBase {
 public:
  PrefetchProxyTabHelperNoPrefetchesTest() {
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        features::kIsolatePrerenders,
        {{"max_srp_prefetches", "0"},
         {"ineligible_decoy_request_probability", "0"}});
  }
};

TEST_F(PrefetchProxyTabHelperNoPrefetchesTest, LimitedNumberOfPrefetches_Zero) {
  base::HistogramTester histogram_tester;

  NavigateSomewhere();
  GURL doc_url("https://www.google.com/search?q=cats");
  GURL prediction_url("https://www.cat-food.com/");
  MakeNavigationPrediction(web_contents(), doc_url, {prediction_url});
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(predicted_urls_count(), 1U);
  EXPECT_EQ(RequestCount(), 0);
  EXPECT_EQ(prefetch_eligible_count(), 1U);
  EXPECT_EQ(prefetch_attempted_count(), 0U);
  EXPECT_EQ(prefetch_successful_count(), 0U);
  EXPECT_EQ(prefetch_total_redirect_count(), 0U);
  EXPECT_TRUE(navigation_to_prefetch_start().has_value());

  histogram_tester.ExpectTotalCount("PrefetchProxy.Prefetch.Mainframe.RespCode",
                                    0);
  histogram_tester.ExpectTotalCount(
      "PrefetchProxy.Prefetch.Mainframe.BodyLength", 0);
  histogram_tester.ExpectTotalCount(
      "PrefetchProxy.Prefetch.Mainframe.TotalTime", 0);
  histogram_tester.ExpectTotalCount(
      "PrefetchProxy.Prefetch.Mainframe.ConnectTime", 0);

  NavigateAndVerifyPrefetchStatus(
      prediction_url, PrefetchProxyPrefetchStatus::kPrefetchNotStarted);
  EXPECT_EQ(after_srp_prefetch_eligible_count(), 1U);
  EXPECT_EQ(base::Optional<size_t>(0), after_srp_clicked_link_srp_position());

  histogram_tester.ExpectTotalCount(
      "PrefetchProxy.Prefetch.Mainframe.TotalRedirects", 0);
}

class PrefetchProxyTabHelperUnlimitedPrefetchesTest
    : public PrefetchProxyTabHelperTestBase {
 public:
  PrefetchProxyTabHelperUnlimitedPrefetchesTest() {
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        features::kIsolatePrerenders,
        {{"max_srp_prefetches", "-1"},
         {"ineligible_decoy_request_probability", "0"}});
  }
};

TEST_F(PrefetchProxyTabHelperUnlimitedPrefetchesTest,
       NumberOfPrefetches_UnlimitedByExperiment) {
  base::HistogramTester histogram_tester;

  NavigateSomewhere();
  GURL doc_url("https://www.google.com/search?q=cats");
  GURL prediction_url_1("https://www.cat-food.com/");
  GURL prediction_url_2("https://www.dogs-r-dumb.com/");
  GURL prediction_url_3("https://www.catz-rule.com/");
  MakeNavigationPrediction(
      web_contents(), doc_url,
      {prediction_url_1, prediction_url_2, prediction_url_3});

  VerifyCommonRequestState(prediction_url_1);
  MakeResponseAndWait(net::HTTP_OK, net::OK, kHTMLMimeType, {}, kHTMLBody);
  VerifyCommonRequestState(prediction_url_2);
  // Failed responses do not retry or attempt more requests in the list.
  MakeResponseAndWait(net::HTTP_OK, net::ERR_FAILED, kHTMLMimeType, {},
                      kHTMLBody);
  VerifyCommonRequestState(prediction_url_3);
  MakeResponseAndWait(net::HTTP_OK, net::OK, kHTMLMimeType, {}, kHTMLBody);

  EXPECT_EQ(RequestCount(), 0);
  EXPECT_EQ(predicted_urls_count(), 3U);
  EXPECT_EQ(prefetch_eligible_count(), 3U);
  EXPECT_EQ(prefetch_attempted_count(), 3U);
  EXPECT_EQ(prefetch_successful_count(), 2U);
  EXPECT_EQ(prefetch_total_redirect_count(), 0U);
  EXPECT_TRUE(navigation_to_prefetch_start().has_value());

  histogram_tester.ExpectBucketCount(
      "PrefetchProxy.Prefetch.Mainframe.NetError", net::OK, 2);
  histogram_tester.ExpectBucketCount(
      "PrefetchProxy.Prefetch.Mainframe.NetError", std::abs(net::ERR_FAILED),
      1);
  histogram_tester.ExpectTotalCount("PrefetchProxy.Prefetch.Mainframe.NetError",
                                    3);

  histogram_tester.ExpectUniqueSample(
      "PrefetchProxy.Prefetch.Mainframe.RespCode", net::HTTP_OK, 2);

  histogram_tester.ExpectUniqueSample(
      "PrefetchProxy.Prefetch.Mainframe.BodyLength", base::size(kHTMLBody), 2);

  histogram_tester.ExpectUniqueSample(
      "PrefetchProxy.Prefetch.Mainframe.TotalTime", kTotalTimeDuration, 2);
  histogram_tester.ExpectUniqueSample(
      "PrefetchProxy.Prefetch.Mainframe.ConnectTime", kConnectTimeDuration, 2);

  NavigateAndVerifyPrefetchStatus(
      prediction_url_3, PrefetchProxyPrefetchStatus::kPrefetchSuccessful);
  EXPECT_EQ(after_srp_prefetch_eligible_count(), 3U);
  EXPECT_EQ(base::Optional<size_t>(2), after_srp_clicked_link_srp_position());

  histogram_tester.ExpectUniqueSample(
      "PrefetchProxy.Prefetch.Mainframe.TotalRedirects", 0, 1);
}

class PrefetchProxyTabHelperConcurrentPrefetchesTest
    : public PrefetchProxyTabHelperTestBase {
 public:
  PrefetchProxyTabHelperConcurrentPrefetchesTest() {
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        features::kIsolatePrerenders,
        {{"max_concurrent_prefetches", "2"},
         {"max_srp_prefetches", "-1"},
         {"ineligible_decoy_request_probability", "0"}});
  }
};

TEST_F(PrefetchProxyTabHelperConcurrentPrefetchesTest, ConcurrentPrefetches) {
  base::HistogramTester histogram_tester;

  PrefetchProxyTabHelperTestObserver observer(tab_helper());

  NavigateSomewhere();
  GURL doc_url("https://www.google.com/search?q=cats");
  GURL prediction_url_1("https://www.cat-food.com/");
  GURL prediction_url_2("https://www.dogs-r-dumb.com/");
  MakeNavigationPrediction(web_contents(), doc_url,
                           {prediction_url_1, prediction_url_2});

  observer.WaitForNEligibilityChecks(2);
  EXPECT_EQ(RequestCount(), 2);

  VerifyCommonRequestState(prediction_url_1, 2);
  MakeResponseAndWait(net::HTTP_OK, net::OK, kHTMLMimeType, {}, kHTMLBody);
  VerifyCommonRequestState(prediction_url_2);
  MakeResponseAndWait(net::HTTP_OK, net::OK, kHTMLMimeType, {}, kHTMLBody);

  EXPECT_EQ(RequestCount(), 0);
  EXPECT_EQ(predicted_urls_count(), 2U);
  EXPECT_EQ(prefetch_eligible_count(), 2U);
  EXPECT_EQ(prefetch_attempted_count(), 2U);
  EXPECT_EQ(prefetch_successful_count(), 2U);
  EXPECT_EQ(prefetch_total_redirect_count(), 0U);
  EXPECT_TRUE(navigation_to_prefetch_start().has_value());

  histogram_tester.ExpectUniqueSample(
      "PrefetchProxy.Prefetch.Mainframe.NetError", net::OK, 2);
  histogram_tester.ExpectUniqueSample(
      "PrefetchProxy.Prefetch.Mainframe.RespCode", net::HTTP_OK, 2);
  histogram_tester.ExpectUniqueSample(
      "PrefetchProxy.Prefetch.Mainframe.BodyLength", base::size(kHTMLBody), 2);
  histogram_tester.ExpectUniqueSample(
      "PrefetchProxy.Prefetch.Mainframe.TotalTime", kTotalTimeDuration, 2);
  histogram_tester.ExpectUniqueSample(
      "PrefetchProxy.Prefetch.Mainframe.ConnectTime", kConnectTimeDuration, 2);

  NavigateAndVerifyPrefetchStatus(
      prediction_url_2, PrefetchProxyPrefetchStatus::kPrefetchSuccessful);
  EXPECT_EQ(after_srp_prefetch_eligible_count(), 2U);
  EXPECT_EQ(base::Optional<size_t>(1), after_srp_clicked_link_srp_position());

  histogram_tester.ExpectUniqueSample(
      "PrefetchProxy.Prefetch.Mainframe.TotalRedirects", 0, 1);
}

class PrefetchProxyTabHelperLimitedPrefetchesTest
    : public PrefetchProxyTabHelperTestBase {
 public:
  PrefetchProxyTabHelperLimitedPrefetchesTest() {
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        features::kIsolatePrerenders,
        {{"max_srp_prefetches", "2"},
         {"ineligible_decoy_request_probability", "0"}});
  }
};

TEST_F(PrefetchProxyTabHelperLimitedPrefetchesTest, LimitedNumberOfPrefetches) {
  base::HistogramTester histogram_tester;

  NavigateSomewhere();
  GURL doc_url("https://www.google.com/search?q=cats");
  GURL prediction_url_1("https://www.cat-food.com/");
  GURL prediction_url_2("https://www.dogs-r-dumb.com/");
  GURL prediction_url_3("https://www.catz-rule.com/");
  MakeNavigationPrediction(
      web_contents(), doc_url,
      {prediction_url_1, prediction_url_2, prediction_url_3});

  VerifyCommonRequestState(prediction_url_1);
  // Failed responses do not retry or attempt more requests in the list.
  MakeResponseAndWait(net::HTTP_OK, net::ERR_FAILED, kHTMLMimeType, {},
                      kHTMLBody);
  VerifyCommonRequestState(prediction_url_2);
  MakeResponseAndWait(net::HTTP_OK, net::OK, kHTMLMimeType, {}, kHTMLBody);

  EXPECT_EQ(RequestCount(), 0);
  EXPECT_EQ(predicted_urls_count(), 3U);
  EXPECT_EQ(prefetch_eligible_count(), 3U);
  EXPECT_EQ(prefetch_attempted_count(), 2U);
  EXPECT_EQ(prefetch_successful_count(), 1U);
  EXPECT_EQ(prefetch_total_redirect_count(), 0U);
  EXPECT_TRUE(navigation_to_prefetch_start().has_value());

  histogram_tester.ExpectBucketCount(
      "PrefetchProxy.Prefetch.Mainframe.NetError", net::OK, 1);
  histogram_tester.ExpectBucketCount(
      "PrefetchProxy.Prefetch.Mainframe.NetError", std::abs(net::ERR_FAILED),
      1);
  histogram_tester.ExpectTotalCount("PrefetchProxy.Prefetch.Mainframe.NetError",
                                    2);

  histogram_tester.ExpectUniqueSample(
      "PrefetchProxy.Prefetch.Mainframe.RespCode", net::HTTP_OK, 1);

  histogram_tester.ExpectUniqueSample(
      "PrefetchProxy.Prefetch.Mainframe.BodyLength", base::size(kHTMLBody), 1);

  histogram_tester.ExpectUniqueSample(
      "PrefetchProxy.Prefetch.Mainframe.TotalTime", kTotalTimeDuration, 1);
  histogram_tester.ExpectUniqueSample(
      "PrefetchProxy.Prefetch.Mainframe.ConnectTime", kConnectTimeDuration, 1);

  TriggerRedirectHistogramRecording();
  histogram_tester.ExpectUniqueSample(
      "PrefetchProxy.Prefetch.Mainframe.TotalRedirects", 0, 1);
}

class PrefetchProxyTabHelperRedirectTestBase
    : public PrefetchProxyTabHelperTestBase {
 public:
  PrefetchProxyTabHelperRedirectTestBase() = default;
  ~PrefetchProxyTabHelperRedirectTestBase() override = default;

  void WalkRedirectChainUntilFinalRequest(std::vector<GURL> redirect_chain) {
    ASSERT_GE(redirect_chain.size(), 2U)
        << "redirect_chain must contain the full redirect chain, with the "
           "first element being the first request url, the last element being "
           "the final request url, and any intermediate steps in the middle";

    // Since the prefetches do not follow redirects, but instead have to pause
    // and query the cookie jar every time, each step in the redirect chain
    // needs to be handled like a separate request/response pair.
    for (size_t i = 0; i < redirect_chain.size() - 1; i++) {
      network::TestURLLoaderFactory::Redirects redirects;
      net::RedirectInfo info;
      info.new_url = redirect_chain[i + 1];
      info.status_code = net::HTTP_TEMPORARY_REDIRECT;
      auto head = network::CreateURLResponseHead(net::HTTP_TEMPORARY_REDIRECT);
      redirects.push_back(std::make_pair(info, head->Clone()));

      network::TestURLLoaderFactory::PendingRequest* request =
          test_url_loader_factory_.GetPendingRequest(0);
      ASSERT_TRUE(request);
      EXPECT_EQ(request->request.url, redirect_chain[i]);

      test_url_loader_factory_.AddResponse(
          redirect_chain[i], std::move(head), "unused body during redirect",
          network::URLLoaderCompletionStatus(net::OK), std::move(redirects));

      task_environment()->RunUntilIdle();
    }
    // Clear responses in the network service so we can inspect the next
    // request that comes in before it is responded to.
    ClearResponses();
  }

  void MakeFinalResponse(
      const GURL& final_url,
      net::HttpStatusCode final_status,
      std::vector<std::pair<std::string, std::string>> final_headers,
      const std::string& final_body) {
    auto final_head = network::CreateURLResponseHead(final_status);

    final_head->response_time = base::Time::Now();
    final_head->request_time =
        final_head->response_time -
        base::TimeDelta::FromMilliseconds(kTotalTimeDuration);

    final_head->load_timing.connect_timing.connect_end =
        base::TimeTicks::Now() - base::TimeDelta::FromMinutes(2);
    final_head->load_timing.connect_timing.connect_start =
        final_head->load_timing.connect_timing.connect_end -
        base::TimeDelta::FromMilliseconds(kConnectTimeDuration);

    final_head->mime_type = kHTMLMimeType;

    for (const auto& header : final_headers) {
      final_head->headers->AddHeader(header.first, header.second);
    }
    network::TestURLLoaderFactory::PendingRequest* request =
        test_url_loader_factory_.GetPendingRequest(0);
    ASSERT_TRUE(request);
    EXPECT_EQ(final_url, request->request.url);
    test_url_loader_factory_.AddResponse(
        final_url, std::move(final_head), final_body,
        network::URLLoaderCompletionStatus(net::OK));
    task_environment()->RunUntilIdle();

    // Clear responses in the network service so we can inspect the next request
    // that comes in before it is responded to.
    ClearResponses();
  }

  void RunNoRedirectTest(const GURL& redirect_url) {
    GURL doc_url("https://www.google.com/search?q=cats");
    GURL prediction_url("https://www.cat-food.com/");

    MakeNavigationPrediction(web_contents(), doc_url, {prediction_url});

    VerifyCommonRequestState(prediction_url);
    WalkRedirectChainUntilFinalRequest({prediction_url, redirect_url});
    // Redirect should not be followed.
    EXPECT_EQ(RequestCount(), 0);
  }
};

class PrefetchProxyTabHelperRedirectWithDecoyTest
    : public PrefetchProxyTabHelperRedirectTestBase {
 public:
  PrefetchProxyTabHelperRedirectWithDecoyTest() {
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        features::kIsolatePrerenders,
        {{"ineligible_decoy_request_probability", "1"},
         {"max_srp_prefetches", "2"}});
  }
};

TEST_F(PrefetchProxyTabHelperRedirectWithDecoyTest, ServiceWorkerRegistered) {
  base::HistogramTester histogram_tester;

  NavigateSomewhere();
  GURL doc_url("https://www.google.com/search?q=cats");
  GURL prediction_url("https://www.cat-food.com/");
  GURL redirect_url("https://www.kitty-krunch.com/");

  service_worker_context_.AddRegistrationToRegisteredOrigins(
      url::Origin::Create(prediction_url));

  MakeNavigationPrediction(web_contents(), doc_url, {prediction_url});

  WalkRedirectChainUntilFinalRequest({prediction_url, redirect_url});
  MakeFinalResponse(redirect_url, net::HTTP_OK, {}, kHTMLBody);

  EXPECT_EQ(predicted_urls_count(), 1U);
  EXPECT_EQ(prefetch_eligible_count(), 0U);
  EXPECT_EQ(prefetch_attempted_count(), 0U);
  EXPECT_EQ(prefetch_successful_count(), 0U);
  EXPECT_EQ(prefetch_total_redirect_count(), 0U);
  EXPECT_TRUE(navigation_to_prefetch_start().has_value());

  histogram_tester.ExpectTotalCount("PrefetchProxy.Prefetch.Mainframe.RespCode",
                                    0);
  histogram_tester.ExpectTotalCount(
      "PrefetchProxy.Prefetch.Mainframe.BodyLength", 0);
  histogram_tester.ExpectTotalCount(
      "PrefetchProxy.Prefetch.Mainframe.TotalTime", 0);
  histogram_tester.ExpectTotalCount(
      "PrefetchProxy.Prefetch.Mainframe.ConnectTime", 0);

  NavigateAndVerifyPrefetchStatus(
      prediction_url, PrefetchProxyPrefetchStatus::kPrefetchIsPrivacyDecoy);
  EXPECT_EQ(after_srp_prefetch_eligible_count(), 0U);
  EXPECT_EQ(base::Optional<size_t>(0), after_srp_clicked_link_srp_position());

  histogram_tester.ExpectTotalCount(
      "PrefetchProxy.Prefetch.Mainframe.TotalRedirects", 0);
}

TEST_F(PrefetchProxyTabHelperRedirectWithDecoyTest,
       ServiceWorkerRegistered_ToGoogle) {
  base::HistogramTester histogram_tester;

  NavigateSomewhere();
  GURL doc_url("https://www.google.com/search?q=cats");
  GURL prediction_url("https://www.cat-food.com/");

  service_worker_context_.AddRegistrationToRegisteredOrigins(
      url::Origin::Create(prediction_url));

  MakeNavigationPrediction(web_contents(), doc_url, {prediction_url});

  RunNoRedirectTest(GURL("https://www.google.com/"));

  EXPECT_EQ(predicted_urls_count(), 2U);
  EXPECT_EQ(prefetch_eligible_count(), 0U);
  EXPECT_EQ(prefetch_attempted_count(), 0U);
  EXPECT_EQ(prefetch_successful_count(), 0U);
  EXPECT_EQ(prefetch_total_redirect_count(), 0U);
  EXPECT_TRUE(navigation_to_prefetch_start().has_value());

  histogram_tester.ExpectTotalCount("PrefetchProxy.Prefetch.Mainframe.RespCode",
                                    0);
  histogram_tester.ExpectTotalCount(
      "PrefetchProxy.Prefetch.Mainframe.BodyLength", 0);
  histogram_tester.ExpectTotalCount(
      "PrefetchProxy.Prefetch.Mainframe.TotalTime", 0);
  histogram_tester.ExpectTotalCount(
      "PrefetchProxy.Prefetch.Mainframe.ConnectTime", 0);

  NavigateAndVerifyPrefetchStatus(
      prediction_url, PrefetchProxyPrefetchStatus::kPrefetchIsPrivacyDecoy);
  EXPECT_EQ(after_srp_prefetch_eligible_count(), 0U);
  EXPECT_EQ(base::Optional<size_t>(0), after_srp_clicked_link_srp_position());

  histogram_tester.ExpectTotalCount(
      "PrefetchProxy.Prefetch.Mainframe.TotalRedirects", 0);
}

class PrefetchProxyTabHelperRedirectTest
    : public PrefetchProxyTabHelperRedirectTestBase {
 public:
  PrefetchProxyTabHelperRedirectTest() {
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        features::kIsolatePrerenders,
        {{"ineligible_decoy_request_probability", "0"}});
  }
};

TEST_F(PrefetchProxyTabHelperRedirectTest, NoRedirect_Cookies) {
  NavigateSomewhere();

  GURL site_with_cookies("https://cookies.com");
  ASSERT_TRUE(SetCookie(profile(), site_with_cookies, "testing"));
  RunNoRedirectTest(site_with_cookies);

  EXPECT_EQ(predicted_urls_count(), 1U);
  EXPECT_EQ(prefetch_eligible_count(), 1U);
  EXPECT_EQ(prefetch_attempted_count(), 1U);
  EXPECT_EQ(prefetch_successful_count(), 0U);
  EXPECT_EQ(prefetch_total_redirect_count(), 1U);
  EXPECT_TRUE(navigation_to_prefetch_start().has_value());

  NavigateAndVerifyPrefetchStatus(
      site_with_cookies,
      PrefetchProxyPrefetchStatus::kPrefetchNotEligibleUserHasCookies);
  EXPECT_EQ(after_srp_prefetch_eligible_count(), 1U);
  EXPECT_EQ(base::Optional<size_t>(0), after_srp_clicked_link_srp_position());
}

TEST_F(PrefetchProxyTabHelperRedirectTest, NoRedirect_Insecure) {
  NavigateSomewhere();

  GURL url("http://insecure.com");

  RunNoRedirectTest(url);

  EXPECT_EQ(predicted_urls_count(), 1U);
  EXPECT_EQ(prefetch_eligible_count(), 1U);
  EXPECT_EQ(prefetch_attempted_count(), 1U);
  EXPECT_EQ(prefetch_successful_count(), 0U);
  EXPECT_EQ(prefetch_total_redirect_count(), 1U);
  EXPECT_TRUE(navigation_to_prefetch_start().has_value());

  NavigateAndVerifyPrefetchStatus(
      url, PrefetchProxyPrefetchStatus::kPrefetchNotEligibleSchemeIsNotHttps);
  EXPECT_EQ(after_srp_prefetch_eligible_count(), 1U);
  EXPECT_EQ(base::Optional<size_t>(0), after_srp_clicked_link_srp_position());
}

TEST_F(PrefetchProxyTabHelperRedirectTest, NoRedirect_Insecure_Continued) {
  NavigateSomewhere();

  GURL url("http://insecure.com");

  RunNoRedirectTest(url);

  EXPECT_EQ(predicted_urls_count(), 1U);
  EXPECT_EQ(prefetch_eligible_count(), 1U);
  EXPECT_EQ(prefetch_attempted_count(), 1U);
  EXPECT_EQ(prefetch_successful_count(), 0U);
  EXPECT_EQ(prefetch_total_redirect_count(), 1U);
  EXPECT_TRUE(navigation_to_prefetch_start().has_value());

  GURL final_url("http://final.com/");

  content::MockNavigationHandle handle(web_contents());
  handle.set_url(url);
  tab_helper()->DidStartNavigation(&handle);
  handle.set_has_committed(true);
  handle.set_redirect_chain({
      url,
      final_url,
  });
  handle.set_url(final_url);
  tab_helper()->DidFinishNavigation(&handle);

  ASSERT_TRUE(tab_helper()->after_srp_metrics().has_value());
  ASSERT_TRUE(tab_helper()->after_srp_metrics()->prefetch_status_.has_value());
  EXPECT_EQ(PrefetchProxyPrefetchStatus::kPrefetchNotEligibleSchemeIsNotHttps,
            tab_helper()->after_srp_metrics()->prefetch_status_.value());

  EXPECT_EQ(after_srp_prefetch_eligible_count(), 1U);
  EXPECT_EQ(base::Optional<size_t>(0), after_srp_clicked_link_srp_position());
}

TEST_F(PrefetchProxyTabHelperRedirectTest, NoRedirect_Google) {
  NavigateSomewhere();

  GURL url("https://www.google.com");

  RunNoRedirectTest(url);

  EXPECT_EQ(predicted_urls_count(), 1U);
  EXPECT_EQ(prefetch_eligible_count(), 1U);
  EXPECT_EQ(prefetch_attempted_count(), 1U);
  EXPECT_EQ(prefetch_successful_count(), 0U);
  EXPECT_EQ(prefetch_total_redirect_count(), 1U);
  EXPECT_TRUE(navigation_to_prefetch_start().has_value());

  NavigateAndVerifyPrefetchStatus(
      url, PrefetchProxyPrefetchStatus::kPrefetchNotEligibleGoogleDomain);
  EXPECT_EQ(after_srp_prefetch_eligible_count(), 1U);
  EXPECT_EQ(base::Optional<size_t>(0), after_srp_clicked_link_srp_position());
}

TEST_F(PrefetchProxyTabHelperRedirectTest, NoRedirect_ServiceWorker) {
  NavigateSomewhere();

  GURL site_with_worker("https://service-worker.com");

  service_worker_context_.AddRegistrationToRegisteredOrigins(
      url::Origin::Create(site_with_worker));

  RunNoRedirectTest(site_with_worker);

  EXPECT_EQ(predicted_urls_count(), 1U);
  EXPECT_EQ(prefetch_eligible_count(), 1U);
  EXPECT_EQ(prefetch_attempted_count(), 1U);
  EXPECT_EQ(prefetch_successful_count(), 0U);
  EXPECT_EQ(prefetch_total_redirect_count(), 1U);
  EXPECT_TRUE(navigation_to_prefetch_start().has_value());

  NavigateAndVerifyPrefetchStatus(
      site_with_worker,
      PrefetchProxyPrefetchStatus::kPrefetchNotEligibleUserHasServiceWorker);
  EXPECT_EQ(after_srp_prefetch_eligible_count(), 1U);
  EXPECT_EQ(base::Optional<size_t>(0), after_srp_clicked_link_srp_position());
}

class PrefetchProxyTabHelperRedirectUnlimitedPrefetchesTest
    : public PrefetchProxyTabHelperRedirectTestBase {
 public:
  PrefetchProxyTabHelperRedirectUnlimitedPrefetchesTest() {
    // Enable unlimited prefetches so we can follow the redirect chain all the
    // way.
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        features::kIsolatePrerenders,
        {{"max_srp_prefetches", "-1"},
         {"ineligible_decoy_request_probability", "0"}});
  }
};

TEST_F(PrefetchProxyTabHelperRedirectUnlimitedPrefetchesTest,
       SuccessfulRedirect) {
  base::HistogramTester histogram_tester;
  NavigateSomewhere();
  GURL doc_url("https://www.google.com/search?q=cats");
  GURL prediction_url("https://www.cat-food.com/");
  GURL redirect_url("https://redirect-here.com");

  MakeNavigationPrediction(web_contents(), doc_url, {prediction_url});
  VerifyCommonRequestState(prediction_url);

  WalkRedirectChainUntilFinalRequest({prediction_url, redirect_url});
  MakeFinalResponse(redirect_url, net::HTTP_OK, {{"X-Testing", "Hello World"}},
                    kHTMLBody);

  std::unique_ptr<PrefetchedMainframeResponseContainer> resp =
      tab_helper()->TakePrefetchResponse(redirect_url);
  ASSERT_TRUE(resp);
  EXPECT_EQ(*resp->TakeBody(), kHTMLBody);

  network::mojom::URLResponseHeadPtr head = resp->TakeHead();
  EXPECT_TRUE(head->headers->HasHeaderValue("X-Testing", "Hello World"));

  EXPECT_EQ(predicted_urls_count(), 1U);
  EXPECT_EQ(prefetch_eligible_count(), 2U);
  EXPECT_EQ(prefetch_attempted_count(), 2U);
  EXPECT_EQ(prefetch_successful_count(), 1U);
  EXPECT_EQ(prefetch_total_redirect_count(), 1U);
  EXPECT_TRUE(navigation_to_prefetch_start().has_value());

  histogram_tester.ExpectUniqueSample(
      "PrefetchProxy.Prefetch.Mainframe.NetError", net::OK, 1);
  histogram_tester.ExpectUniqueSample(
      "PrefetchProxy.Prefetch.Mainframe.RespCode", net::HTTP_OK, 1);
  histogram_tester.ExpectUniqueSample(
      "PrefetchProxy.Prefetch.Mainframe.BodyLength", base::size(kHTMLBody), 1);
  histogram_tester.ExpectUniqueSample(
      "PrefetchProxy.Prefetch.Mainframe.TotalTime", kTotalTimeDuration, 1);
  histogram_tester.ExpectUniqueSample(
      "PrefetchProxy.Prefetch.Mainframe.ConnectTime", kConnectTimeDuration, 1);

  NavigateAndVerifyPrefetchStatus(
      redirect_url, PrefetchProxyPrefetchStatus::kPrefetchSuccessful);
  EXPECT_EQ(after_srp_prefetch_eligible_count(), 2U);
  EXPECT_EQ(base::Optional<size_t>(0), after_srp_clicked_link_srp_position());

  histogram_tester.ExpectUniqueSample(
      "PrefetchProxy.Prefetch.Mainframe.TotalRedirects", 1, 1);
}
