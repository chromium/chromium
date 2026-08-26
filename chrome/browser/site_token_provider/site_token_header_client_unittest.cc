// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/site_token_provider/site_token_header_client.h"

#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "base/functional/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "base/values.h"
#include "chrome/browser/site_token_provider/site_token_provider_service_factory.h"
#include "chrome/test/base/testing_profile.h"
#include "components/site_token_provider/features.h"
#include "components/site_token_provider/site_token_constants.h"
#include "components/site_token_provider/site_token_provider_service.h"
#include "content/public/test/browser_task_environment.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/base/ip_address.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_errors.h"
#include "net/http/http_request_headers.h"
#include "net/ssl/ssl_info.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace site_token_provider {

namespace {

using testing::_;
using testing::Invoke;
using testing::StrictMock;

constexpr char kTestSiteTokenExample[] = "token-for-example";
constexpr char kTestSiteTokenExample2[] = "token-for-example2";
constexpr char kTestSiteTokenLocalhost[] = "token-for-localhost";
constexpr char kTestSiteToken127[] = "token-for-127-0-0-1";
constexpr char kTestSiteTokenSubExample[] = "token-for-sub-example";
constexpr char kOriginalHeaderName[] = "X-Original";
constexpr char kOriginalHeaderValue[] = "Value";
constexpr char kInitialHeaderName[] = "X-Initial";
constexpr char kInitialHeaderValue[] = "InitialValue";
constexpr char kEnterpriseHeaderName[] = "X-Enterprise-Header";
constexpr char kEnterpriseHeaderValue[] = "EnterpriseValue";

class MockTrustedHeaderClient : public network::mojom::TrustedHeaderClient {
 public:
  MockTrustedHeaderClient() = default;
  ~MockTrustedHeaderClient() override = default;

  mojo::PendingRemote<network::mojom::TrustedHeaderClient> BindAndGetRemote() {
    return receiver_.BindNewPipeAndPassRemote();
  }

  MOCK_METHOD(void,
              OnBeforeSendHeaders,
              (const GURL& request_url,
               const net::HttpRequestHeaders& headers,
               OnBeforeSendHeadersCallback callback),
              (override));

  MOCK_METHOD(void,
              OnHeadersReceived,
              (const std::string& headers,
               const net::IPEndPoint& remote_endpoint,
               const std::optional<net::SSLInfo>& ssl_info,
               OnHeadersReceivedCallback callback),
              (override));

 private:
  mojo::Receiver<network::mojom::TrustedHeaderClient> receiver_{this};
};

struct BeforeSendHeadersResult {
  int32_t net_error = net::OK;
  std::optional<net::HttpRequestHeaders> headers;
  std::optional<base::DictValue> extended_net_log_events;
};

struct HeadersReceivedResult {
  int32_t net_error = net::OK;
  std::optional<std::string> headers;
  std::optional<GURL> allowed_unsafe_redirect_url;
};

}  // namespace

class SiteTokenHeaderClientTest : public testing::Test {
 public:
  SiteTokenHeaderClientTest() = default;

  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        features::kSiteTokenProviderEnabled,
        {{"site_token_allowlist",
          "example.com,example2.com,localhost,127.0.0.1"}});
    TestingProfile::Builder builder;
    profile_ = builder.Build();

    auto* service =
        SiteTokenProviderServiceFactory::GetForProfile(profile_.get());
    ASSERT_TRUE(service);
    service->SetTokenForTesting("example.com", kTestSiteTokenExample);
    service->SetTokenForTesting("example2.com", kTestSiteTokenExample2);
    service->SetTokenForTesting("localhost", kTestSiteTokenLocalhost);
    service->SetTokenForTesting("127.0.0.1", kTestSiteToken127);
    service->SetTokenForTesting("sub.example.com", kTestSiteTokenSubExample);
  }

 protected:
  BeforeSendHeadersResult SendHeaders(
      network::mojom::TrustedHeaderClient* client,
      const GURL& url,
      const net::HttpRequestHeaders& initial_headers) {
    base::test::TestFuture<int32_t,
                           const std::optional<net::HttpRequestHeaders>&,
                           std::optional<base::DictValue>>
        future;
    client->OnBeforeSendHeaders(url, initial_headers, future.GetCallback());
    auto [net_error, headers, log_events] = future.Take();
    return BeforeSendHeadersResult{net_error, headers, std::move(log_events)};
  }

  HeadersReceivedResult ReceiveHeaders(
      network::mojom::TrustedHeaderClient* client,
      const std::string& headers) {
    base::test::TestFuture<int32_t, const std::optional<std::string>&,
                           const std::optional<GURL>&>
        future;
    net::IPEndPoint endpoint(net::IPAddress::IPv4Localhost(), 443);
    client->OnHeadersReceived(headers, endpoint, /*ssl_info=*/std::nullopt,
                              future.GetCallback());
    auto [net_error, out_headers, redirect_url] = future.Take();
    return HeadersReceivedResult{net_error, out_headers, redirect_url};
  }

  content::BrowserTaskEnvironment task_environment_;
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<TestingProfile> profile_;
};

TEST_F(SiteTokenHeaderClientTest, PassThroughWithoutService) {
  mojo::Remote<network::mojom::TrustedHeaderClient> remote;
  SiteTokenHeaderClient::Create(nullptr, remote.BindNewPipeAndPassReceiver(),
                                mojo::NullRemote());

  net::HttpRequestHeaders headers;
  headers.SetHeader(kOriginalHeaderName, kOriginalHeaderValue);

  BeforeSendHeadersResult result =
      SendHeaders(remote.get(), GURL("https://example.com"), headers);

  EXPECT_EQ(net::OK, result.net_error);
  EXPECT_FALSE(result.headers.has_value());
}

TEST_F(SiteTokenHeaderClientTest, InjectsTokenForAllowedDomain) {
  auto* service =
      SiteTokenProviderServiceFactory::GetForProfile(profile_.get());
  ASSERT_TRUE(service);

  mojo::Remote<network::mojom::TrustedHeaderClient> remote;
  SiteTokenHeaderClient::Create(service->GetWeakPtr(),
                                remote.BindNewPipeAndPassReceiver(),
                                mojo::NullRemote());

  net::HttpRequestHeaders headers;
  headers.SetHeader(kOriginalHeaderName, kOriginalHeaderValue);

  BeforeSendHeadersResult result =
      SendHeaders(remote.get(), GURL("https://example.com/article"), headers);

  EXPECT_EQ(net::OK, result.net_error);
  ASSERT_TRUE(result.headers.has_value());
  EXPECT_EQ(kOriginalHeaderValue,
            result.headers->GetHeader(kOriginalHeaderName).value_or(""));
  EXPECT_EQ(kTestSiteTokenExample,
            result.headers->GetHeader(kChromeSiteTokenHeader).value_or(""));
}

TEST_F(SiteTokenHeaderClientTest, DoesNotInjectTokenForDisallowedDomain) {
  auto* service =
      SiteTokenProviderServiceFactory::GetForProfile(profile_.get());
  ASSERT_TRUE(service);

  mojo::Remote<network::mojom::TrustedHeaderClient> remote;
  SiteTokenHeaderClient::Create(service->GetWeakPtr(),
                                remote.BindNewPipeAndPassReceiver(),
                                mojo::NullRemote());

  net::HttpRequestHeaders headers;
  headers.SetHeader(kOriginalHeaderName, kOriginalHeaderValue);

  BeforeSendHeadersResult result =
      SendHeaders(remote.get(), GURL("https://google.com"), headers);

  EXPECT_EQ(net::OK, result.net_error);
  EXPECT_FALSE(result.headers.has_value());
}

TEST_F(SiteTokenHeaderClientTest, ChainsWithTargetClientAndMergesHeaders) {
  auto* service =
      SiteTokenProviderServiceFactory::GetForProfile(profile_.get());
  ASSERT_TRUE(service);

  StrictMock<MockTrustedHeaderClient> mock_target;
  EXPECT_CALL(mock_target,
              OnBeforeSendHeaders(GURL("https://example.com/feed"), _, _))
      .WillOnce(
          [](const GURL&, const net::HttpRequestHeaders& initial_headers,
             network::mojom::TrustedHeaderClient::OnBeforeSendHeadersCallback
                 callback) {
            net::HttpRequestHeaders modified_headers = initial_headers;
            modified_headers.SetHeader(kEnterpriseHeaderName,
                                       kEnterpriseHeaderValue);
            base::DictValue log_events;
            log_events.Set("log_key", "log_val");
            std::move(callback).Run(net::OK, modified_headers,
                                    std::move(log_events));
          });

  mojo::Remote<network::mojom::TrustedHeaderClient> remote;
  SiteTokenHeaderClient::Create(service->GetWeakPtr(),
                                remote.BindNewPipeAndPassReceiver(),
                                mock_target.BindAndGetRemote());

  net::HttpRequestHeaders headers;
  headers.SetHeader(kInitialHeaderName, kInitialHeaderValue);

  BeforeSendHeadersResult result =
      SendHeaders(remote.get(), GURL("https://example.com/feed"), headers);

  EXPECT_EQ(net::OK, result.net_error);
  ASSERT_TRUE(result.headers.has_value());
  EXPECT_EQ(kInitialHeaderValue,
            result.headers->GetHeader(kInitialHeaderName).value_or(""));
  EXPECT_EQ(kEnterpriseHeaderValue,
            result.headers->GetHeader(kEnterpriseHeaderName).value_or(""));
  EXPECT_EQ(kTestSiteTokenExample,
            result.headers->GetHeader(kChromeSiteTokenHeader).value_or(""));
  ASSERT_TRUE(result.extended_net_log_events.has_value());
  EXPECT_EQ("log_val", *result.extended_net_log_events->FindString("log_key"));
}

TEST_F(SiteTokenHeaderClientTest, PropagatesTargetClientError) {
  auto* service =
      SiteTokenProviderServiceFactory::GetForProfile(profile_.get());
  ASSERT_TRUE(service);

  StrictMock<MockTrustedHeaderClient> mock_target;
  EXPECT_CALL(mock_target,
              OnBeforeSendHeaders(GURL("https://example.com/feed"), _, _))
      .WillOnce(
          [](const GURL&, const net::HttpRequestHeaders&,
             network::mojom::TrustedHeaderClient::OnBeforeSendHeadersCallback
                 callback) {
            std::move(callback).Run(net::ERR_ACCESS_DENIED, std::nullopt,
                                    std::nullopt);
          });

  mojo::Remote<network::mojom::TrustedHeaderClient> remote;
  SiteTokenHeaderClient::Create(service->GetWeakPtr(),
                                remote.BindNewPipeAndPassReceiver(),
                                mock_target.BindAndGetRemote());

  net::HttpRequestHeaders headers;
  BeforeSendHeadersResult result =
      SendHeaders(remote.get(), GURL("https://example.com/feed"), headers);

  EXPECT_EQ(net::ERR_ACCESS_DENIED, result.net_error);
  EXPECT_FALSE(result.headers.has_value());
}

TEST_F(SiteTokenHeaderClientTest, DelegatedOnHeadersReceived) {
  StrictMock<MockTrustedHeaderClient> mock_target;
  EXPECT_CALL(mock_target,
              OnHeadersReceived("HTTP/1.1 200 OK\r\n\r\n", _, _, _))
      .WillOnce(
          [](const std::string&, const net::IPEndPoint&,
             const std::optional<net::SSLInfo>&,
             network::mojom::TrustedHeaderClient::OnHeadersReceivedCallback
                 callback) {
            std::move(callback).Run(net::OK,
                                    "HTTP/1.1 200 OK\r\nX-Modified: 1\r\n\r\n",
                                    std::nullopt);
          });

  mojo::Remote<network::mojom::TrustedHeaderClient> remote;
  SiteTokenHeaderClient::Create(nullptr, remote.BindNewPipeAndPassReceiver(),
                                mock_target.BindAndGetRemote());

  HeadersReceivedResult result =
      ReceiveHeaders(remote.get(), "HTTP/1.1 200 OK\r\n\r\n");

  EXPECT_EQ(net::OK, result.net_error);
  ASSERT_TRUE(result.headers.has_value());
  EXPECT_EQ("HTTP/1.1 200 OK\r\nX-Modified: 1\r\n\r\n", *result.headers);
}

TEST_F(SiteTokenHeaderClientTest,
       DoesNotInjectTokenWhenCacheEmptyForAllowedDomain) {
  auto* service =
      SiteTokenProviderServiceFactory::GetForProfile(profile_.get());
  ASSERT_TRUE(service);
  service->SetTokenForTesting("example.com", "");

  mojo::Remote<network::mojom::TrustedHeaderClient> remote;
  SiteTokenHeaderClient::Create(service->GetWeakPtr(),
                                remote.BindNewPipeAndPassReceiver(),
                                mojo::NullRemote());

  net::HttpRequestHeaders headers;
  headers.SetHeader(kOriginalHeaderName, kOriginalHeaderValue);

  BeforeSendHeadersResult result =
      SendHeaders(remote.get(), GURL("https://example.com/page"), headers);

  EXPECT_EQ(net::OK, result.net_error);
  EXPECT_FALSE(result.headers.has_value());
}

TEST_F(SiteTokenHeaderClientTest, HandlesTargetClientDisconnectGracefully) {
  auto* service =
      SiteTokenProviderServiceFactory::GetForProfile(profile_.get());
  ASSERT_TRUE(service);

  auto mock_target = std::make_unique<StrictMock<MockTrustedHeaderClient>>();
  mojo::Remote<network::mojom::TrustedHeaderClient> remote;
  SiteTokenHeaderClient::Create(service->GetWeakPtr(),
                                remote.BindNewPipeAndPassReceiver(),
                                mock_target->BindAndGetRemote());

  // Destroy target client to trigger disconnect handler.
  mock_target.reset();

  net::HttpRequestHeaders headers;
  headers.SetHeader(kOriginalHeaderName, kOriginalHeaderValue);

  BeforeSendHeadersResult result =
      SendHeaders(remote.get(), GURL("https://example.com/article"), headers);

  EXPECT_EQ(net::OK, result.net_error);
  ASSERT_TRUE(result.headers.has_value());
  EXPECT_EQ(kOriginalHeaderValue,
            result.headers->GetHeader(kOriginalHeaderName).value_or(""));
  EXPECT_EQ(kTestSiteTokenExample,
            result.headers->GetHeader(kChromeSiteTokenHeader).value_or(""));
}

TEST_F(SiteTokenHeaderClientTest, HandlesServiceDestructionGracefully) {
  mojo::Remote<network::mojom::TrustedHeaderClient> remote;
  {
    TestingProfile::Builder temp_builder;
    std::unique_ptr<TestingProfile> temp_profile = temp_builder.Build();
    auto* service =
        SiteTokenProviderServiceFactory::GetForProfile(temp_profile.get());
    ASSERT_TRUE(service);
    service->SetTokenForTesting("example.com", kTestSiteTokenExample);

    SiteTokenHeaderClient::Create(service->GetWeakPtr(),
                                  remote.BindNewPipeAndPassReceiver(),
                                  mojo::NullRemote());
    // temp_profile goes out of scope and destroys service.
  }

  net::HttpRequestHeaders headers;
  headers.SetHeader(kOriginalHeaderName, kOriginalHeaderValue);

  BeforeSendHeadersResult result =
      SendHeaders(remote.get(), GURL("https://example.com/article"), headers);

  EXPECT_EQ(net::OK, result.net_error);
  EXPECT_FALSE(result.headers.has_value());
}

TEST_F(SiteTokenHeaderClientTest, DoesNotInjectTokenWhenFeatureDisabled) {
  base::test::ScopedFeatureList disabled_feature_list;
  disabled_feature_list.InitAndDisableFeature(
      features::kSiteTokenProviderEnabled);

  auto* service =
      SiteTokenProviderServiceFactory::GetForProfile(profile_.get());
  ASSERT_TRUE(service);

  mojo::Remote<network::mojom::TrustedHeaderClient> remote;
  SiteTokenHeaderClient::Create(service->GetWeakPtr(),
                                remote.BindNewPipeAndPassReceiver(),
                                mojo::NullRemote());

  net::HttpRequestHeaders headers;
  headers.SetHeader(kOriginalHeaderName, kOriginalHeaderValue);

  BeforeSendHeadersResult result =
      SendHeaders(remote.get(), GURL("https://example.com/article"), headers);

  EXPECT_EQ(net::OK, result.net_error);
  EXPECT_FALSE(result.headers.has_value());
}

TEST_F(SiteTokenHeaderClientTest, DoesNotInjectTokenWithInvalidHeaderValue) {
  auto* service =
      SiteTokenProviderServiceFactory::GetForProfile(profile_.get());
  ASSERT_TRUE(service);
  service->SetTokenForTesting("example.com", "invalid\r\ntoken\nvalue");

  mojo::Remote<network::mojom::TrustedHeaderClient> remote;
  SiteTokenHeaderClient::Create(service->GetWeakPtr(),
                                remote.BindNewPipeAndPassReceiver(),
                                mojo::NullRemote());

  net::HttpRequestHeaders headers;
  headers.SetHeader(kOriginalHeaderName, kOriginalHeaderValue);

  BeforeSendHeadersResult result =
      SendHeaders(remote.get(), GURL("https://example.com/article"), headers);

  EXPECT_EQ(net::OK, result.net_error);
  EXPECT_FALSE(result.headers.has_value());
}

struct DomainLookupTestCase {
  std::string_view url_string;
  std::optional<std::string_view> expected_token;
};

class SiteTokenHeaderClientDomainTest
    : public SiteTokenHeaderClientTest,
      public testing::WithParamInterface<DomainLookupTestCase> {};

TEST_P(SiteTokenHeaderClientDomainTest, InjectsTokenForDomainVariations) {
  auto* service =
      SiteTokenProviderServiceFactory::GetForProfile(profile_.get());
  ASSERT_TRUE(service);

  mojo::Remote<network::mojom::TrustedHeaderClient> remote;
  SiteTokenHeaderClient::Create(service->GetWeakPtr(),
                                remote.BindNewPipeAndPassReceiver(),
                                mojo::NullRemote());

  net::HttpRequestHeaders headers;
  BeforeSendHeadersResult result =
      SendHeaders(remote.get(), GURL(GetParam().url_string), headers);

  EXPECT_EQ(net::OK, result.net_error);
  if (GetParam().expected_token.has_value()) {
    ASSERT_TRUE(result.headers.has_value());
    EXPECT_EQ(*GetParam().expected_token,
              result.headers->GetHeader(kChromeSiteTokenHeader).value_or(""));
  } else {
    EXPECT_FALSE(result.headers.has_value());
  }
}

INSTANTIATE_TEST_SUITE_P(
    All,
    SiteTokenHeaderClientDomainTest,
    testing::Values(
        DomainLookupTestCase{"https://example.com/page", kTestSiteTokenExample},
        DomainLookupTestCase{"https://example2.com/page",
                             kTestSiteTokenExample2},
        DomainLookupTestCase{"https://www.example.com/page",
                             kTestSiteTokenExample},
        DomainLookupTestCase{"https://example.com:8443/page",
                             kTestSiteTokenExample},
        DomainLookupTestCase{"http://localhost:8080/page",
                             kTestSiteTokenLocalhost},
        DomainLookupTestCase{"http://127.0.0.1/test", kTestSiteToken127},
        DomainLookupTestCase{"http://example.com/page", std::nullopt},
        DomainLookupTestCase{"https://sub.example.com/page", std::nullopt},
        DomainLookupTestCase{"https://google.com/", std::nullopt},
        DomainLookupTestCase{"about:blank", std::nullopt},
        DomainLookupTestCase{"data:text/html,test", std::nullopt},
        DomainLookupTestCase{"chrome://settings", std::nullopt}));

}  // namespace site_token_provider
