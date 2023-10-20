// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "chrome/browser/ip_protection/ip_protection_config_http.h"

#include "base/strings/strcat.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "chrome/browser/ip_protection/get_proxy_config.pb.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "net/base/features.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/public/mojom/url_response_head.mojom-shared.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

class IpProtectionConfigHttpTest : public testing::Test {
 protected:
  void SetUp() override {
    http_fetcher_ = std::make_unique<IpProtectionConfigHttp>(
        base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
            &test_url_loader_factory_));
    token_server_get_initial_data_url_ = GURL(base::StrCat(
        {net::features::kIpPrivacyTokenServer.Get(),
         net::features::kIpPrivacyTokenServerGetInitialDataPath.Get()}));
    ASSERT_TRUE(token_server_get_initial_data_url_.is_valid());
    token_server_get_tokens_url_ = GURL(base::StrCat(
        {net::features::kIpPrivacyTokenServer.Get(),
         net::features::kIpPrivacyTokenServerGetTokensPath.Get()}));
    ASSERT_TRUE(token_server_get_tokens_url_.is_valid());
    token_server_get_proxy_config_url_ = GURL(base::StrCat(
        {net::features::kIpPrivacyTokenServer.Get(),
         net::features::kIpPrivacyTokenServerGetProxyConfigPath.Get()}));
    ASSERT_TRUE(token_server_get_proxy_config_url_.is_valid());
  }

  base::test::TaskEnvironment task_environment_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  std::unique_ptr<IpProtectionConfigHttp> http_fetcher_;
  std::unique_ptr<IdentityTestEnvironmentProfileAdaptor>
      identity_test_env_adaptor_;
  GURL token_server_get_initial_data_url_;
  GURL token_server_get_tokens_url_;
  GURL token_server_get_proxy_config_url_;
};

TEST_F(IpProtectionConfigHttpTest, DoRequestSendsCorrectRequest) {
  auto request_type = quiche::BlindSignHttpRequestType::kGetInitialData;
  std::string authorization_header = "token";
  std::string body = "body";

  // Set up the response to return from the mock.
  auto head = network::mojom::URLResponseHead::New();
  std::string response_body = "Response body";
  test_url_loader_factory_.AddResponse(
      token_server_get_initial_data_url_, std::move(head), response_body,
      network::URLLoaderCompletionStatus(net::OK));

  base::test::TestFuture<absl::StatusOr<quiche::BlindSignHttpResponse>>
      result_future;
  // Note: We use a lambda expression and `TestFuture::SetValue()` instead of
  // `TestFuture::GetCallback()` to avoid having to convert the
  // `base::OnceCallback` to a `quiche::SignedTokenCallback` (an
  // `absl::AnyInvocable` behind the scenes).
  auto callback =
      [&result_future](absl::StatusOr<quiche::BlindSignHttpResponse> response) {
        result_future.SetValue(std::move(response));
      };

  http_fetcher_->DoRequest(request_type, authorization_header, body,
                           std::move(callback));

  absl::StatusOr<quiche::BlindSignHttpResponse> result = result_future.Get();

  ASSERT_TRUE(result.ok());
  EXPECT_EQ("Response body", result->body());
}

TEST_F(IpProtectionConfigHttpTest,
       DoRequestFailsToConnectReturnsFailureStatus) {
  auto request_type = quiche::BlindSignHttpRequestType::kAuthAndSign;
  std::string authorization_header = "token";
  std::string body = "body";

  // Mock no response from Authentication Server (such as a network error).
  std::string response_body;
  auto head = network::mojom::URLResponseHead::New();
  test_url_loader_factory_.AddResponse(
      token_server_get_tokens_url_, std::move(head), response_body,
      network::URLLoaderCompletionStatus(net::ERR_FAILED));

  base::test::TestFuture<absl::StatusOr<quiche::BlindSignHttpResponse>>
      result_future;
  auto callback =
      [&result_future](absl::StatusOr<quiche::BlindSignHttpResponse> response) {
        result_future.SetValue(std::move(response));
      };

  http_fetcher_->DoRequest(request_type, authorization_header, body,
                           std::move(callback));

  absl::StatusOr<quiche::BlindSignHttpResponse> result = result_future.Get();

  EXPECT_EQ("Failed Request to Authentication Server",
            result.status().message());
  EXPECT_EQ(absl::StatusCode::kInternal, result.status().code());
}

TEST_F(IpProtectionConfigHttpTest,
       DoRequestInvalidFinchParametersFailsGracefully) {
  std::map<std::string, std::string> parameters;
  parameters["IpPrivacyTokenServer"] = "<(^_^)>";
  parameters["IpPrivacyTokenServerGetInitialDataPath"] = "(>_<)";
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      net::features::kEnableIpProtectionProxy, std::move(parameters));

  // Create a new IpProtectionConfigHttp for this test so that the new
  // FeatureParams get used.
  std::unique_ptr<IpProtectionConfigHttp> http_fetcher =
      std::make_unique<IpProtectionConfigHttp>(
          base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
              &test_url_loader_factory_));

  auto request_type = quiche::BlindSignHttpRequestType::kGetInitialData;
  std::string authorization_header = "token";
  std::string body = "body";

  base::test::TestFuture<absl::StatusOr<quiche::BlindSignHttpResponse>>
      result_future;
  auto callback =
      [&result_future](absl::StatusOr<quiche::BlindSignHttpResponse> response) {
        result_future.SetValue(std::move(response));
      };

  http_fetcher->DoRequest(request_type, authorization_header, body,
                          std::move(callback));

  absl::StatusOr<quiche::BlindSignHttpResponse> result = result_future.Get();

  EXPECT_EQ("Invalid IP Protection Token URL", result.status().message());
  EXPECT_EQ(absl::StatusCode::kInternal, result.status().code());
}

TEST_F(IpProtectionConfigHttpTest, DoRequestHttpFailureStatus) {
  auto request_type = quiche::BlindSignHttpRequestType::kAuthAndSign;
  std::string authorization_header = "token";
  std::string body = "body";

  // Mock a non-200 HTTP response from Authentication Server.
  std::string response_body;
  auto head = network::mojom::URLResponseHead::New();
  test_url_loader_factory_.AddResponse(token_server_get_tokens_url_.spec(),
                                       response_body, net::HTTP_BAD_REQUEST);

  base::test::TestFuture<absl::StatusOr<quiche::BlindSignHttpResponse>>
      result_future;
  auto callback =
      [&result_future](absl::StatusOr<quiche::BlindSignHttpResponse> response) {
        result_future.SetValue(std::move(response));
      };

  http_fetcher_->DoRequest(request_type, authorization_header, body,
                           std::move(callback));

  absl::StatusOr<quiche::BlindSignHttpResponse> result = result_future.Get();

  EXPECT_TRUE(result.ok());
  EXPECT_EQ(net::HTTP_BAD_REQUEST, result.value().status_code());
}

TEST_F(IpProtectionConfigHttpTest, GetProxyConfigSuccess) {
  ip_protection::GetProxyConfigResponse response_proto;
  response_proto.add_first_hop_hostnames("host1");
  response_proto.add_first_hop_hostnames("host2");

  ip_protection::GetProxyConfigResponse_ProxyChain* proxyChain =
      response_proto.add_proxy_chain();
  proxyChain->set_proxy_a("proxyA");
  proxyChain->set_proxy_b("proxyB");
  std::string response_str = response_proto.SerializeAsString();

  auto head = network::mojom::URLResponseHead::New();
  test_url_loader_factory_.AddResponse(
      token_server_get_proxy_config_url_, std::move(head), response_str,
      network::URLLoaderCompletionStatus(net::OK));

  base::test::TestFuture<absl::StatusOr<ip_protection::GetProxyConfigResponse>>
      result_future;
  http_fetcher_->GetProxyConfig(result_future.GetCallback());

  absl::StatusOr<ip_protection::GetProxyConfigResponse> result =
      result_future.Get();

  ASSERT_TRUE(result.ok());
  EXPECT_EQ(2, result->first_hop_hostnames_size());
  EXPECT_EQ("host1", result->first_hop_hostnames(0));
  EXPECT_EQ("host2", result->first_hop_hostnames(1));
  EXPECT_EQ("proxyA", result->proxy_chain().at(0).proxy_a());
  EXPECT_EQ("proxyB", result->proxy_chain().at(0).proxy_b());
}

TEST_F(IpProtectionConfigHttpTest, GetProxyConfigEmpty) {
  ip_protection::GetProxyConfigResponse response_proto;
  std::string response_str = response_proto.SerializeAsString();

  auto head = network::mojom::URLResponseHead::New();
  test_url_loader_factory_.AddResponse(
      token_server_get_proxy_config_url_, std::move(head), response_str,
      network::URLLoaderCompletionStatus(net::OK));

  base::test::TestFuture<absl::StatusOr<ip_protection::GetProxyConfigResponse>>
      result_future;
  http_fetcher_->GetProxyConfig(result_future.GetCallback());

  absl::StatusOr<ip_protection::GetProxyConfigResponse> result =
      result_future.Get();

  ASSERT_TRUE(result.ok());
  EXPECT_EQ(0, result->first_hop_hostnames_size());
  EXPECT_EQ(0, result->proxy_chain_size());
}

TEST_F(IpProtectionConfigHttpTest, GetProxyConfigFails) {
  auto head = network::mojom::URLResponseHead::New();
  test_url_loader_factory_.AddResponse(
      token_server_get_proxy_config_url_, std::move(head), "uhoh",
      network::URLLoaderCompletionStatus(net::HTTP_BAD_REQUEST));

  base::test::TestFuture<absl::StatusOr<ip_protection::GetProxyConfigResponse>>
      result_future;
  http_fetcher_->GetProxyConfig(result_future.GetCallback());

  absl::StatusOr<ip_protection::GetProxyConfigResponse> result =
      result_future.Get();

  ASSERT_FALSE(result.ok());
  ASSERT_TRUE(absl::IsInternal(result.status()));
}
