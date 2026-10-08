// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/analysis/network_request_proxying_url_loader_factory.h"

#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/scoped_refptr.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "chrome/browser/enterprise/connectors/test/deep_scanning_test_utils.h"
#include "chrome/browser/enterprise/connectors/test/pending_binary_upload_service.h"
#include "chrome/browser/policy/dm_token_utils.h"
#include "chrome/browser/safe_browsing/cloud_content_scanning/cloud_binary_upload_service_factory.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "components/enterprise/common/proto/connectors.pb.h"
#include "components/enterprise/connectors/core/cloud_content_scanning/binary_upload_request.h"
#include "components/enterprise/connectors/core/common.h"
#include "components/enterprise/connectors/core/features.h"
#include "components/policy/core/common/cloud/dm_token.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/web_contents_tester.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/resource_request_body.h"
#include "services/network/public/cpp/url_loader_factory_builder.h"
#include "services/network/public/mojom/url_loader.mojom.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace enterprise_connectors {

namespace {

using URLLoaderFactoryType =
    content::ContentBrowserClient::URLLoaderFactoryType;

constexpr char kTabUrl[] = "https://foo.com/page";
constexpr char kOtherTabUrl[] = "https://other.com/page";
constexpr char kRequestUrl[] = "https://bar.org/upload";
constexpr char kOtherRequestUrl[] = "https://baz.net/upload";
constexpr std::string_view kRequestBody = "sensitive=data";
constexpr char kDmToken[] = "dm_token";

constexpr char kNetworkRequestPolicy[] = R"({
  "audit": {
    "tab_domain": ["foo.com"],
    "request_domain": ["bar.org"]
  },
  "tags": ["dlp"]
})";

scoped_refptr<network::ResourceRequestBody> CreateBody() {
  return network::ResourceRequestBody::CreateFromCopyOfBytes(
      base::as_byte_span(kRequestBody));
}

class NetworkRequestProxyingURLLoaderFactoryTest
    : public ChromeRenderViewHostTestHarness {
 public:
  NetworkRequestProxyingURLLoaderFactoryTest() {
    scoped_feature_list_.InitAndEnableFeature(
        kEnableAuditOnlyNetworkRequestConnector);
  }

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    identity_test_env_adaptor_ =
        std::make_unique<IdentityTestEnvironmentProfileAdaptor>(profile());
    policy::SetDMTokenForTesting(policy::DMToken::CreateValidToken(kDmToken));
    test::SetAnalysisConnector(profile()->GetPrefs(),
                               AnalysisConnector::NETWORK_REQUEST,
                               kNetworkRequestPolicy);
    NavigateAndCommit(GURL(kTabUrl));
  }

  void TearDown() override {
    loaders_.clear();
    clients_.clear();
    identity_test_env_adaptor_.reset();
    ChromeRenderViewHostTestHarness::TearDown();
    policy::SetDMTokenForTesting(policy::DMToken::CreateEmptyToken());
  }

  TestingProfile::TestingFactories GetTestingFactories() const override {
    TestingProfile::TestingFactories factories =
        IdentityTestEnvironmentProfileAdaptor::
            GetIdentityTestEnvironmentFactories();
    factories.emplace_back(
        safe_browsing::CloudBinaryUploadServiceFactory::GetInstance(),
        base::BindRepeating(&test::PendingBinaryUploadService::Create));
    return factories;
  }

  // Creates a proxy for `frame` that forwards requests to `target_factory_`.
  // `on_proxy_destroyed` is run once the proxy is destroyed, which is observed
  // through the proxy disconnecting from its target.
  mojo::Remote<network::mojom::URLLoaderFactory> CreateProxy(
      content::RenderFrameHost* frame,
      base::OnceClosure on_proxy_destroyed = base::DoNothing()) {
    mojo::PendingRemote<network::mojom::URLLoaderFactory> target_remote;
    auto& target_receiver = target_receivers_.emplace_back(
        std::make_unique<mojo::Receiver<network::mojom::URLLoaderFactory>>(
            &target_factory_, target_remote.InitWithNewPipeAndPassReceiver()));
    target_receiver->set_disconnect_handler(std::move(on_proxy_destroyed));

    mojo::Remote<network::mojom::URLLoaderFactory> proxy;
    base::MakeSelfDeleting<NetworkRequestProxyingURLLoaderFactory>(
        proxy.BindNewPipeAndPassReceiver(), std::move(target_remote),
        frame->GetGlobalId());
    return proxy;
  }

  // Sends a request through `factory` and waits for it to reach
  // `target_factory_`. The loader and client of the request are kept alive
  // until the end of the test.
  void SendRequest(network::mojom::URLLoaderFactory& factory,
                   const std::string& method,
                   const GURL& url,
                   scoped_refptr<network::ResourceRequestBody> body) {
    network::ResourceRequest request;
    request.method = method;
    request.url = url;
    request.request_body = std::move(body);

    mojo::PendingRemote<network::mojom::URLLoaderClient> client;
    clients_.push_back(client.InitWithNewPipeAndPassReceiver());
    loaders_.emplace_back();

    size_t expected_requests = target_factory_.total_requests() + 1;
    factory.CreateLoaderAndStart(loaders_.back().BindNewPipeAndPassReceiver(),
                                 /*request_id=*/0,
                                 /*options=*/0, request, std::move(client),
                                 net::MutableNetworkTrafficAnnotationTag());
    ASSERT_TRUE(base::test::RunUntil([&]() {
      return target_factory_.total_requests() == expected_requests;
    }));
  }

  test::PendingBinaryUploadService* upload_service() {
    return test::PendingBinaryUploadService::GetForProfile(profile());
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
  network::TestURLLoaderFactory target_factory_;

 private:
  std::unique_ptr<IdentityTestEnvironmentProfileAdaptor>
      identity_test_env_adaptor_;
  std::vector<std::unique_ptr<mojo::Receiver<network::mojom::URLLoaderFactory>>>
      target_receivers_;
  std::vector<mojo::Remote<network::mojom::URLLoader>> loaders_;
  std::vector<mojo::PendingReceiver<network::mojom::URLLoaderClient>> clients_;
};

// Same as `NetworkRequestProxyingURLLoaderFactoryTest`, but with the feature
// disabled before the profile is created.
class NetworkRequestProxyingURLLoaderFactoryFeatureDisabledTest
    : public NetworkRequestProxyingURLLoaderFactoryTest {
 public:
  NetworkRequestProxyingURLLoaderFactoryFeatureDisabledTest() {
    scoped_feature_list_.Reset();
    scoped_feature_list_.InitAndDisableFeature(
        kEnableAuditOnlyNetworkRequestConnector);
  }
};

}  // namespace

TEST_F(NetworkRequestProxyingURLLoaderFactoryTest,
       MaybeProxyRequest_FrameFactories) {
  for (auto type : {URLLoaderFactoryType::kNavigation,
                    URLLoaderFactoryType::kDocumentSubResource}) {
    network::URLLoaderFactoryBuilder builder;
    NetworkRequestProxyingURLLoaderFactory::MaybeProxyRequest(main_rfh(), type,
                                                              builder);
    EXPECT_EQ(1u, builder.num_interceptors());
  }
}

TEST_F(NetworkRequestProxyingURLLoaderFactoryTest,
       MaybeProxyRequest_OtherFactories) {
  {
    network::URLLoaderFactoryBuilder builder;
    NetworkRequestProxyingURLLoaderFactory::MaybeProxyRequest(
        /*frame=*/nullptr, URLLoaderFactoryType::kDocumentSubResource, builder);
    EXPECT_EQ(0u, builder.num_interceptors());
  }

  for (auto type : {URLLoaderFactoryType::kDownload,
                    URLLoaderFactoryType::kWorkerMainResource,
                    URLLoaderFactoryType::kWorkerSubResource,
                    URLLoaderFactoryType::kServiceWorkerScript}) {
    network::URLLoaderFactoryBuilder builder;
    NetworkRequestProxyingURLLoaderFactory::MaybeProxyRequest(main_rfh(), type,
                                                              builder);
    EXPECT_EQ(0u, builder.num_interceptors());
  }
}

TEST_F(NetworkRequestProxyingURLLoaderFactoryTest,
       MaybeProxyRequest_FeatureDisabled) {
  base::test::ScopedFeatureList disable_feature;
  disable_feature.InitAndDisableFeature(
      kEnableAuditOnlyNetworkRequestConnector);

  network::URLLoaderFactoryBuilder builder;
  NetworkRequestProxyingURLLoaderFactory::MaybeProxyRequest(
      main_rfh(), URLLoaderFactoryType::kDocumentSubResource, builder);
  EXPECT_EQ(0u, builder.num_interceptors());
}

TEST_F(NetworkRequestProxyingURLLoaderFactoryTest, MaybeProxyRequest_NoPolicy) {
  test::ClearAnalysisConnector(profile()->GetPrefs(),
                               AnalysisConnector::NETWORK_REQUEST);

  network::URLLoaderFactoryBuilder builder;
  NetworkRequestProxyingURLLoaderFactory::MaybeProxyRequest(
      main_rfh(), URLLoaderFactoryType::kDocumentSubResource, builder);
  EXPECT_EQ(0u, builder.num_interceptors());
}

TEST_F(NetworkRequestProxyingURLLoaderFactoryTest, ScansPostRequest) {
  base::test::TestFuture<RequestHandlerResult> future;
  auto scan_completed_callback = NetworkRequestProxyingURLLoaderFactory::
      SetScanCompletedCallbackForTesting(
          future.GetRepeatingCallback<const RequestHandlerResult&>());

  auto proxy = CreateProxy(main_rfh());
  SendRequest(*proxy, "POST", GURL(kRequestUrl), CreateBody());

  // The request is forwarded right away, without waiting for a verdict.
  ASSERT_EQ(1u, target_factory_.pending_requests()->size());
  const network::ResourceRequest& forwarded_request =
      target_factory_.pending_requests()->at(0).request;
  EXPECT_EQ("POST", forwarded_request.method);
  EXPECT_EQ(GURL(kRequestUrl), forwarded_request.url);
  ASSERT_TRUE(forwarded_request.request_body);
  EXPECT_EQ(1u, forwarded_request.request_body->elements()->size());
  EXPECT_FALSE(future.IsReady());

  ASSERT_EQ(1u, upload_service()->requests().size());
  BinaryUploadRequest* scan_request = upload_service()->requests()[0].get();
  const ContentAnalysisRequest& analysis_request =
      scan_request->content_analysis_request();
  EXPECT_EQ(AnalysisConnector::NETWORK_REQUEST,
            analysis_request.analysis_connector());
  EXPECT_EQ(kDmToken, analysis_request.device_token());
  EXPECT_FALSE(analysis_request.blocking());
  ASSERT_EQ(1, analysis_request.tags_size());
  EXPECT_EQ("dlp", analysis_request.tags(0));
  EXPECT_EQ(kRequestUrl, analysis_request.request_data().url());
  EXPECT_EQ(kTabUrl, analysis_request.request_data().tab_url());
  EXPECT_EQ(kRequestUrl, analysis_request.request_data().destination());

  base::test::TestFuture<ScanRequestUploadResult, BinaryUploadRequest::Data>
      data_future;
  scan_request->GetRequestData(data_future.GetCallback());
  EXPECT_EQ(ScanRequestUploadResult::kSuccess,
            data_future.Get<ScanRequestUploadResult>());
  EXPECT_EQ(kRequestBody.size(),
            data_future.Get<BinaryUploadRequest::Data>().size);

  ContentAnalysisResponse response;
  response.set_request_token("request_token");
  scan_request->FinishRequest(ScanRequestUploadResult::kSuccess, response);

  // Scans complete synchronously with their request.
  ASSERT_TRUE(future.IsReady());
  RequestHandlerResult result = future.Take();
  EXPECT_TRUE(result.complies);
  EXPECT_EQ(FinalContentAnalysisResult::SUCCESS, result.final_result);
  EXPECT_EQ("request_token", result.request_token);
}

TEST_F(NetworkRequestProxyingURLLoaderFactoryTest, DoesNotScanOtherRequests) {
  auto proxy = CreateProxy(main_rfh());

  // Only POST requests are scanned.
  SendRequest(*proxy, "GET", GURL(kRequestUrl), nullptr);
  SendRequest(*proxy, "PUT", GURL(kRequestUrl), CreateBody());

  // POST requests without a body have nothing to scan.
  SendRequest(*proxy, "POST", GURL(kRequestUrl), nullptr);
  SendRequest(*proxy, "POST", GURL(kRequestUrl),
              base::MakeRefCounted<network::ResourceRequestBody>());

  // The request URL doesn't match the policy.
  SendRequest(*proxy, "POST", GURL(kOtherRequestUrl), CreateBody());

  // Every request is still forwarded.
  EXPECT_EQ(5u, target_factory_.total_requests());
  EXPECT_TRUE(upload_service()->requests().empty());
}

TEST_F(NetworkRequestProxyingURLLoaderFactoryTest, DoesNotScanNonMatchingTab) {
  NavigateAndCommit(GURL(kOtherTabUrl));

  auto proxy = CreateProxy(main_rfh());
  SendRequest(*proxy, "POST", GURL(kRequestUrl), CreateBody());

  EXPECT_EQ(1u, target_factory_.total_requests());
  EXPECT_TRUE(upload_service()->requests().empty());
}

// The policy is checked for every request, so removing it applies to existing
// proxies. The first scan is left pending to also cover the destruction of a
// profile with a pending scan.
TEST_F(NetworkRequestProxyingURLLoaderFactoryTest, PolicyRemoved) {
  auto proxy = CreateProxy(main_rfh());
  SendRequest(*proxy, "POST", GURL(kRequestUrl), CreateBody());
  EXPECT_EQ(1u, upload_service()->requests().size());

  test::ClearAnalysisConnector(profile()->GetPrefs(),
                               AnalysisConnector::NETWORK_REQUEST);

  SendRequest(*proxy, "POST", GURL(kRequestUrl), CreateBody());
  EXPECT_EQ(2u, target_factory_.total_requests());
  EXPECT_EQ(1u, upload_service()->requests().size());
}

// Scans should complete even if the tab that made the request, and the proxy
// that started the scan, are destroyed before the verdict is obtained.
TEST_F(NetworkRequestProxyingURLLoaderFactoryTest, ScanOutlivesTab) {
  base::test::TestFuture<RequestHandlerResult> future;
  auto scan_completed_callback = NetworkRequestProxyingURLLoaderFactory::
      SetScanCompletedCallbackForTesting(
          future.GetRepeatingCallback<const RequestHandlerResult&>());

  base::test::TestFuture<void> proxy_destroyed;
  {
    auto proxy = CreateProxy(main_rfh(), proxy_destroyed.GetCallback());
    SendRequest(*proxy, "POST", GURL(kRequestUrl), CreateBody());
  }
  DeleteContents();
  ASSERT_TRUE(proxy_destroyed.Wait());

  ASSERT_EQ(1u, upload_service()->requests().size());
  upload_service()->requests()[0]->FinishRequest(
      ScanRequestUploadResult::kSuccess, ContentAnalysisResponse());

  RequestHandlerResult result = future.Take();
  EXPECT_TRUE(result.complies);
  EXPECT_EQ(FinalContentAnalysisResult::SUCCESS, result.final_result);
}

// Scans depend on keyed services of their profile, so pending scans should be
// deleted when that profile is destroyed.
TEST_F(NetworkRequestProxyingURLLoaderFactoryTest,
       ProfileDestroyedWithPendingScan) {
  TestingProfile::Builder profile_builder;
  profile_builder.AddTestingFactories(GetTestingFactories());
  std::unique_ptr<TestingProfile> other_profile = profile_builder.Build();
  test::SetAnalysisConnector(other_profile->GetPrefs(),
                             AnalysisConnector::NETWORK_REQUEST,
                             kNetworkRequestPolicy);

  std::unique_ptr<content::WebContents> other_web_contents =
      content::WebContentsTester::CreateTestWebContents(other_profile.get(),
                                                        nullptr);
  content::NavigationSimulator::NavigateAndCommitFromBrowser(
      other_web_contents.get(), GURL(kTabUrl));

  base::test::TestFuture<RequestHandlerResult> future;
  auto scan_completed_callback = NetworkRequestProxyingURLLoaderFactory::
      SetScanCompletedCallbackForTesting(
          future.GetRepeatingCallback<const RequestHandlerResult&>());

  {
    auto proxy = CreateProxy(other_web_contents->GetPrimaryMainFrame());
    SendRequest(*proxy, "POST", GURL(kRequestUrl), CreateBody());
  }

  // Take ownership of the pending request so it outlives the profile's upload
  // service.
  test::PendingBinaryUploadService* other_upload_service =
      test::PendingBinaryUploadService::GetForProfile(other_profile.get());
  ASSERT_EQ(1u, other_upload_service->requests().size());
  std::unique_ptr<BinaryUploadRequest> request =
      std::move(other_upload_service->requests()[0]);
  other_upload_service->requests().clear();

  other_web_contents.reset();
  other_profile.reset();

  // The scan was deleted with the profile, so completing its request is a
  // no-op. Scans complete synchronously with their request, so there is no need
  // to wait before checking that the scan didn't complete.
  request->FinishRequest(ScanRequestUploadResult::kSuccess,
                         ContentAnalysisResponse());
  EXPECT_FALSE(future.IsReady());
}

// Requests matching the policy shouldn't be scanned if the feature is disabled,
// but they should still be forwarded.
TEST_F(NetworkRequestProxyingURLLoaderFactoryFeatureDisabledTest,
       DoesNotScanRequests) {
  auto proxy = CreateProxy(main_rfh());
  SendRequest(*proxy, "POST", GURL(kRequestUrl), CreateBody());

  EXPECT_EQ(1u, target_factory_.total_requests());
  EXPECT_TRUE(upload_service()->requests().empty());
}

}  // namespace enterprise_connectors
