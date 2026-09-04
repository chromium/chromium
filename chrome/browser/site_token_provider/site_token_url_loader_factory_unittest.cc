// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/site_token_provider/site_token_url_loader_factory.h"

#include <memory>
#include <optional>
#include <string>
#include <string_view>

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/strcat.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/site_token_provider/site_token_provider_service_factory.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/site_token_provider/features.h"
#include "components/site_token_provider/site_token_provider.h"
#include "components/site_token_provider/site_token_provider_service.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/child_process_security_policy.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/test/mock_render_process_host.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/system/data_pipe_utils.h"
#include "net/base/net_errors.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_status_code.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/cors/cors.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/mojom/url_loader.mojom.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "services/network/test/test_url_loader_client.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace site_token_provider {
namespace {

using ::testing::_;
using ::testing::Return;

GURL ValidTokenUrl() {
  return GURL(base::StrCat({chrome::kChromeExperimentalSiteTokenProviderScheme,
                            "://", chrome::kChromeExperimentalSiteTokenHost}));
}

class MockSiteTokenProvider : public SiteTokenProvider {
 public:
  MockSiteTokenProvider() = default;
  ~MockSiteTokenProvider() override = default;

  void SetTokenUpdateCallback(TokenUpdateCallback callback) override {}
  void UpdateState() override {}
};

class MockSiteTokenProviderService : public SiteTokenProviderService {
 public:
  explicit MockSiteTokenProviderService(Profile* profile)
      : SiteTokenProviderService(IdentityManagerFactory::GetForProfile(profile),
                                 std::make_unique<MockSiteTokenProvider>()) {}
  ~MockSiteTokenProviderService() override = default;

  MOCK_METHOD(std::string,
              GetTokenForDomain,
              (std::string_view domain),
              (const, override));
};

std::unique_ptr<KeyedService> BuildMockSiteTokenProviderService(
    content::BrowserContext* context) {
  return std::make_unique<MockSiteTokenProviderService>(
      Profile::FromBrowserContext(context));
}

struct LoadResult {
  int error_code = net::OK;
  network::mojom::URLResponseHeadPtr response_head;
  std::string response_body;
};

class SiteTokenURLLoaderFactoryTest : public ChromeRenderViewHostTestHarness {
 protected:
  SiteTokenURLLoaderFactoryTest()
      : ChromeRenderViewHostTestHarness(
            content::BrowserTaskEnvironment::REAL_IO_THREAD) {
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        features::kSiteTokenProviderEnabled,
        {{"site_token_allowlist",
          "example.com,sub.domain.example.com,legitimate.com,localhost,127.0.0."
          "1"}});
  }
  ~SiteTokenURLLoaderFactoryTest() override = default;

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();

    mock_service_ = static_cast<MockSiteTokenProviderService*>(
        SiteTokenProviderServiceFactory::GetInstance()->SetTestingFactoryAndUse(
            profile(),
            base::BindRepeating(&BuildMockSiteTokenProviderService)));
    ASSERT_TRUE(mock_service_);

    factory_remote_.Bind(
        SiteTokenURLLoaderFactory::Create(process()->GetID().GetUnsafeValue()));
  }

  void TearDown() override {
    mock_service_ = nullptr;
    ChromeRenderViewHostTestHarness::TearDown();
  }

  LoadResult IssueRequest(const network::ResourceRequest& request) {
    network::TestURLLoaderClient client;
    mojo::PendingRemote<network::mojom::URLLoader> loader;
    factory_remote_->CreateLoaderAndStart(
        loader.InitWithNewPipeAndPassReceiver(), /*request_id=*/1,
        /*options=*/0, request, client.CreateRemote(),
        net::MutableNetworkTrafficAnnotationTag());
    client.RunUntilComplete();

    LoadResult result;
    result.error_code = client.completion_status().error_code;
    if (client.has_received_response()) {
      result.response_head = client.response_head().Clone();
    }
    if (client.response_body().is_valid()) {
      mojo::BlockingCopyToString(client.response_body_release(),
                                 &result.response_body);
    }
    return result;
  }

  base::test::ScopedFeatureList scoped_feature_list_;
  raw_ptr<MockSiteTokenProviderService> mock_service_ = nullptr;
  mojo::Remote<network::mojom::URLLoaderFactory> factory_remote_;
};

struct TokenDeliveryTestCase {
  const char* test_name;
  const char* initiator_url;
  const char* mock_token;
  const char* expected_domain;
};

class SiteTokenURLLoaderFactoryTokenDeliveryTest
    : public SiteTokenURLLoaderFactoryTest,
      public testing::WithParamInterface<TokenDeliveryTestCase> {};

TEST_P(SiteTokenURLLoaderFactoryTokenDeliveryTest,
       DeliversTokenForValidRequests) {
  const TokenDeliveryTestCase& param = GetParam();
  NavigateAndCommit(GURL(param.initiator_url));

  EXPECT_CALL(*mock_service_, GetTokenForDomain(param.expected_domain))
      .WillOnce(Return(param.mock_token));

  network::ResourceRequest request;
  request.method = net::HttpRequestHeaders::kGetMethod;
  request.url = ValidTokenUrl();
  request.request_initiator = url::Origin::Create(GURL(param.initiator_url));

  LoadResult result = IssueRequest(request);

  EXPECT_EQ(result.error_code, net::OK);
  ASSERT_TRUE(result.response_head);
  ASSERT_TRUE(result.response_head->headers);
  EXPECT_EQ(result.response_head->headers->response_code(), net::HTTP_OK);
  EXPECT_EQ(result.response_body, param.mock_token);

  EXPECT_EQ(result.response_head->headers->GetNormalizedHeader(
                network::cors::header_names::kAccessControlAllowOrigin),
            url::Origin::Create(GURL(param.initiator_url)).Serialize());
  EXPECT_EQ(result.response_head->headers->GetNormalizedHeader(
                network::cors::header_names::kAccessControlAllowCredentials),
            "true");
}

INSTANTIATE_TEST_SUITE_P(
    All,
    SiteTokenURLLoaderFactoryTokenDeliveryTest,
    testing::Values(
        TokenDeliveryTestCase{
            .test_name = "DomainWithoutPort",
            .initiator_url = "https://example.com",
            .mock_token = "token_abc_123",
            .expected_domain = "example.com",
        },
        TokenDeliveryTestCase{
            .test_name = "Subdomain",
            .initiator_url = "https://sub.domain.example.com",
            .mock_token = "token_subdomain_456",
            .expected_domain = "sub.domain.example.com",
        },
        TokenDeliveryTestCase{
            .test_name = "DomainWithExplicitPort",
            .initiator_url = "https://example.com:8443",
            .mock_token = "token_port_789",
            .expected_domain = "example.com",
        },
        TokenDeliveryTestCase{
            .test_name = "LocalhostInitiator",
            .initiator_url = "http://localhost:8080",
            .mock_token = "token_localhost_123",
            .expected_domain = "localhost",
        },
        TokenDeliveryTestCase{
            .test_name = "IPv4LocalhostInitiator",
            .initiator_url = "http://127.0.0.1:8080",
            .mock_token = "token_127_456",
            .expected_domain = "127.0.0.1",
        },
        TokenDeliveryTestCase{
            .test_name = "EmptyTokenReturnsEmptyBody",
            .initiator_url = "https://example.com",
            .mock_token = "",
            .expected_domain = "example.com",
        }),
    [](const testing::TestParamInfo<TokenDeliveryTestCase>& info) {
      return info.param.test_name;
    });

struct InvalidRequestTestCase {
  const char* test_name;
  const char* method;
  const char* request_url;
  std::optional<url::Origin> initiator;
  int expected_error;
};

class SiteTokenURLLoaderFactoryInvalidRequestTest
    : public SiteTokenURLLoaderFactoryTest,
      public testing::WithParamInterface<InvalidRequestTestCase> {};

TEST_P(SiteTokenURLLoaderFactoryInvalidRequestTest, RejectsInvalidRequests) {
  const InvalidRequestTestCase& param = GetParam();

  EXPECT_CALL(*mock_service_, GetTokenForDomain(testing::_)).Times(0);

  network::ResourceRequest request;
  request.method =
      param.method ? param.method : net::HttpRequestHeaders::kGetMethod;
  request.url = GURL(param.request_url);
  request.request_initiator = param.initiator;

  LoadResult result = IssueRequest(request);

  EXPECT_EQ(result.error_code, param.expected_error);
  EXPECT_TRUE(result.response_body.empty());
  EXPECT_FALSE(result.response_head);
}

INSTANTIATE_TEST_SUITE_P(
    All,
    SiteTokenURLLoaderFactoryInvalidRequestTest,
    testing::Values(
        InvalidRequestTestCase{
            .test_name = "WrongHost",
            .method = net::HttpRequestHeaders::kGetMethod,
            .request_url = "chrome-experimental-site-token-provider://invalid",
            .initiator = url::Origin::Create(GURL("https://example.com")),
            .expected_error = net::ERR_INVALID_URL,
        },
        InvalidRequestTestCase{
            .test_name = "WrongScheme",
            .method = net::HttpRequestHeaders::kGetMethod,
            .request_url = "chrome-site-token://token",
            .initiator = url::Origin::Create(GURL("https://example.com")),
            .expected_error = net::ERR_INVALID_URL,
        },
        InvalidRequestTestCase{
            .test_name = "UrlWithPath",
            .method = net::HttpRequestHeaders::kGetMethod,
            .request_url =
                "chrome-experimental-site-token-provider://token/extra_path",
            .initiator = url::Origin::Create(GURL("https://example.com")),
            .expected_error = net::ERR_INVALID_URL,
        },
        InvalidRequestTestCase{
            .test_name = "UrlWithQuery",
            .method = net::HttpRequestHeaders::kGetMethod,
            .request_url =
                "chrome-experimental-site-token-provider://token?query=1",
            .initiator = url::Origin::Create(GURL("https://example.com")),
            .expected_error = net::ERR_INVALID_URL,
        },
        InvalidRequestTestCase{
            .test_name = "UrlWithRef",
            .method = net::HttpRequestHeaders::kGetMethod,
            .request_url =
                "chrome-experimental-site-token-provider://token#ref",
            .initiator = url::Origin::Create(GURL("https://example.com")),
            .expected_error = net::ERR_INVALID_URL,
        },
        InvalidRequestTestCase{
            .test_name = "PostMethodRejected",
            .method = "POST",
            .request_url = "chrome-experimental-site-token-provider://token",
            .initiator = url::Origin::Create(GURL("https://example.com")),
            .expected_error = net::ERR_METHOD_NOT_SUPPORTED,
        },
        InvalidRequestTestCase{
            .test_name = "PutMethodRejected",
            .method = "PUT",
            .request_url = "chrome-experimental-site-token-provider://token",
            .initiator = url::Origin::Create(GURL("https://example.com")),
            .expected_error = net::ERR_METHOD_NOT_SUPPORTED,
        },
        InvalidRequestTestCase{
            .test_name = "DeleteMethodRejected",
            .method = "DELETE",
            .request_url = "chrome-experimental-site-token-provider://token",
            .initiator = url::Origin::Create(GURL("https://example.com")),
            .expected_error = net::ERR_METHOD_NOT_SUPPORTED,
        },
        InvalidRequestTestCase{
            .test_name = "MissingInitiator",
            .method = net::HttpRequestHeaders::kGetMethod,
            .request_url = "chrome-experimental-site-token-provider://token",
            .initiator = std::nullopt,
            .expected_error = net::ERR_ACCESS_DENIED,
        },
        InvalidRequestTestCase{
            .test_name = "OpaqueInitiator",
            .method = net::HttpRequestHeaders::kGetMethod,
            .request_url = "chrome-experimental-site-token-provider://token",
            .initiator = url::Origin(),
            .expected_error = net::ERR_ACCESS_DENIED,
        },
        InvalidRequestTestCase{
            .test_name = "NonCryptographicHttpInitiator",
            .method = net::HttpRequestHeaders::kGetMethod,
            .request_url = "chrome-experimental-site-token-provider://token",
            .initiator = url::Origin::Create(GURL("http://example.com")),
            .expected_error = net::ERR_ACCESS_DENIED,
        }),
    [](const testing::TestParamInfo<InvalidRequestTestCase>& info) {
      return info.param.test_name;
    });

TEST_F(SiteTokenURLLoaderFactoryTest, RejectsUnauthorizedProcess) {
  NavigateAndCommit(GURL("https://legitimate.com"));

  EXPECT_CALL(*mock_service_, GetTokenForDomain(_)).Times(0);

  network::ResourceRequest request;
  request.method = net::HttpRequestHeaders::kGetMethod;
  request.url = ValidTokenUrl();
  request.request_initiator =
      url::Origin::Create(GURL("https://unauthorized.com"));

  LoadResult result = IssueRequest(request);

  EXPECT_EQ(result.error_code, net::ERR_ACCESS_DENIED);
  EXPECT_TRUE(result.response_body.empty());
  EXPECT_FALSE(result.response_head);
}

TEST_F(SiteTokenURLLoaderFactoryTest, RejectsDisallowedDomain) {
  NavigateAndCommit(GURL("https://disallowed.com"));

  EXPECT_CALL(*mock_service_, GetTokenForDomain(_)).Times(0);

  network::ResourceRequest request;
  request.method = net::HttpRequestHeaders::kGetMethod;
  request.url = ValidTokenUrl();
  request.request_initiator =
      url::Origin::Create(GURL("https://disallowed.com"));

  LoadResult result = IssueRequest(request);

  EXPECT_EQ(result.error_code, net::ERR_ACCESS_DENIED);
  EXPECT_TRUE(result.response_body.empty());
  EXPECT_FALSE(result.response_head);
}

TEST_F(SiteTokenURLLoaderFactoryTest, ClonesFactorySuccessfully) {
  mojo::Remote<network::mojom::URLLoaderFactory> cloned_factory;
  factory_remote_->Clone(cloned_factory.BindNewPipeAndPassReceiver());

  NavigateAndCommit(GURL("https://example.com"));

  EXPECT_CALL(*mock_service_, GetTokenForDomain("example.com"))
      .WillOnce(Return("cloned_token"));

  network::ResourceRequest request;
  request.method = net::HttpRequestHeaders::kGetMethod;
  request.url = ValidTokenUrl();
  request.request_initiator = url::Origin::Create(GURL("https://example.com"));

  network::TestURLLoaderClient client;
  mojo::PendingRemote<network::mojom::URLLoader> loader;
  cloned_factory->CreateLoaderAndStart(
      loader.InitWithNewPipeAndPassReceiver(), /*request_id=*/1,
      /*options=*/0, request, client.CreateRemote(),
      net::MutableNetworkTrafficAnnotationTag());
  client.RunUntilComplete();

  EXPECT_EQ(client.completion_status().error_code, net::OK);
  ASSERT_TRUE(client.response_body().is_valid());
  std::string response_body;
  ASSERT_TRUE(mojo::BlockingCopyToString(client.response_body_release(),
                                         &response_body));
  EXPECT_EQ(response_body, "cloned_token");
}

}  // namespace
}  // namespace site_token_provider
