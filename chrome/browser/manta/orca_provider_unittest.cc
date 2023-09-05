// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/manta/orca_provider.h"

#include <memory>
#include <string>

#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/endpoint_fetcher/mock_endpoint_fetcher.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "content/public/test/browser_task_environment.h"
#include "net/base/net_errors.h"
#include "net/http/http_status_code.h"
#include "net/http/http_util.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "services/data_decoder/public/cpp/test_support/in_process_data_decoder.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "services/network/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace manta {

namespace {

constexpr base::TimeDelta kMockTimeout = base::Seconds(100);
constexpr char kMockOAuthConsumerName[] = "mock_oauth_consumer_name";
constexpr char kMockScope[] = "mock_scope";
constexpr char kMockEndpoint[] = "https://my-endpoint.com";
constexpr char kHttpMethod[] = "POST";
constexpr char kMockContentType[] = "mock_content_type";
constexpr char kEmail[] = "mock_email@gmail.com";

}  // namespace

// TODO(b:288019728): refactor into some reused test_util
class FakeOrcaProvider : public OrcaProvider {
 public:
  FakeOrcaProvider(
      scoped_refptr<network::SharedURLLoaderFactory> test_url_loader_factory,
      signin::IdentityManager* identity_manager)
      : OrcaProvider(std::move(test_url_loader_factory), identity_manager) {}

 private:
  std::unique_ptr<EndpointFetcher> CreateEndpointFetcher(
      const GURL& url,
      const std::vector<std::string>& scopes,
      const std::string& post_data) override {
    return std::make_unique<EndpointFetcher>(
        /*url_loader_factory=*/url_loader_factory_,
        /*oauth_consumer_name=*/kMockOAuthConsumerName,
        /*url=*/GURL{kMockEndpoint},
        /*http_method=*/kHttpMethod, /*content_type=*/kMockContentType,
        /*scopes=*/std::vector<std::string>{kMockScope},
        /*timeout_ms=*/kMockTimeout.InMilliseconds(), /*post_data=*/post_data,
        /*annotation_tag=*/TRAFFIC_ANNOTATION_FOR_TESTS,
        /*identity_manager=*/identity_manager_,
        /*consent_level=*/signin::ConsentLevel::kSync);
  }
};

class OrcaProviderTest : public testing::Test {
 public:
  OrcaProviderTest()
      : task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME),
        profile_manager_(TestingBrowserProcess::GetGlobal()) {}

  OrcaProviderTest(const OrcaProviderTest&) = delete;
  OrcaProviderTest& operator=(const OrcaProviderTest&) = delete;

  ~OrcaProviderTest() override = default;

  void SetUp() override {
    identity_test_env_.MakePrimaryAccountAvailable(kEmail,
                                                   signin::ConsentLevel::kSync);
    identity_test_env_.SetAutomaticIssueOfAccessTokens(true);
    in_process_data_decoder_ =
        std::make_unique<data_decoder::test::InProcessDataDecoder>();
  }

  void SetEndpointMockResponse(const GURL& request_url,
                               const std::string& response_data,
                               net::HttpStatusCode status_code,
                               net::Error error_code) {
    auto head = network::mojom::URLResponseHead::New();
    std::string headers(base::StringPrintf(
        "HTTP/1.1 %d %s\nContent-type: application/json\n\n",
        static_cast<int>(status_code), GetHttpReasonPhrase(status_code)));
    head->headers = base::MakeRefCounted<net::HttpResponseHeaders>(
        net::HttpUtil::AssembleRawHeaders(headers));
    head->mime_type = "application/json";
    network::URLLoaderCompletionStatus status(error_code);
    status.decoded_body_length = response_data.size();
    test_url_loader_factory_.AddResponse(request_url, std::move(head),
                                         response_data, status);
  }

  std::unique_ptr<FakeOrcaProvider> CreateOrcaProvider() {
    return std::make_unique<FakeOrcaProvider>(
        base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
            &test_url_loader_factory_),
        identity_test_env_.identity_manager());
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;

 private:
  signin::IdentityTestEnvironment identity_test_env_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  TestingProfileManager profile_manager_;
  std::unique_ptr<data_decoder::test::InProcessDataDecoder>
      in_process_data_decoder_;
};

// Test OrcaProvider rejects invalid input data. Currently we require the
// input must contain a valid tone.
TEST_F(OrcaProviderTest, PrepareRequestFailure) {
  std::map<std::string, std::string> input = {{"data", "simple post data"}};
  std::unique_ptr<FakeOrcaProvider> orca_provider = CreateOrcaProvider();

  SetEndpointMockResponse(GURL{kMockEndpoint}, /*response_data=*/"",
                          net::HTTP_OK, net::OK);

  orca_provider->Call(input,
                      base::BindLambdaForTesting(
                          [quit_closure = task_environment_.QuitClosure()](
                              base::Value::Dict response) {
                            EXPECT_TRUE(response.FindBool("error").value());
                            EXPECT_EQ(*response.FindString("error_message"),
                                      "Fail to prepare request");
                            quit_closure.Run();
                          }));

  task_environment_.RunUntilQuit();
}

// Test that responses with http_status_code != net::HTTP_OK are captured.
TEST_F(OrcaProviderTest, CaptureUnexcpetedStatusCode) {
  std::map<std::string, std::string> input = {{"data", "simple post data"},
                                              {"tone", "SHORTEN"}};
  std::unique_ptr<FakeOrcaProvider> orca_provider = CreateOrcaProvider();

  SetEndpointMockResponse(GURL{kMockEndpoint}, /*response_data=*/"",
                          net::HTTP_BAD_REQUEST, net::OK);

  orca_provider->Call(
      input, base::BindLambdaForTesting(
                 [quit_closure = task_environment_.QuitClosure()](
                     base::Value::Dict response) {
                   EXPECT_TRUE(response.FindBool("error").value());
                   EXPECT_EQ(response.FindInt("http_status_code").value(),
                             static_cast<int32_t>(net::HTTP_BAD_REQUEST));
                   quit_closure.Run();
                 }));
  task_environment_.RunUntilQuit();
}

// Test that responses with error_type are captured properly.
TEST_F(OrcaProviderTest, CaptureAuthError) {
  std::map<std::string, std::string> input = {{"data", "simple post data"},
                                              {"tone", "SHORTEN"}};
  std::unique_ptr<FakeOrcaProvider> orca_provider = CreateOrcaProvider();

  SetEndpointMockResponse(GURL{kMockEndpoint}, /*response_data=*/"",
                          net::HTTP_UNAUTHORIZED, net::OK);

  orca_provider->Call(
      input, base::BindLambdaForTesting(
                 [quit_closure = task_environment_.QuitClosure()](
                     base::Value::Dict response) {
                   EXPECT_TRUE(response.FindBool("error").value());
                   EXPECT_TRUE(response.FindInt("error_type").has_value());
                   // kAuthError
                   EXPECT_EQ(response.FindInt("error_type").value(), 0);
                   quit_closure.Run();
                 }));
  task_environment_.RunUntilQuit();
}

TEST_F(OrcaProviderTest, CaptureNetError) {
  std::map<std::string, std::string> input = {{"data", "simple post data"},
                                              {"tone", "SHORTEN"}};
  std::unique_ptr<FakeOrcaProvider> orca_provider = CreateOrcaProvider();

  SetEndpointMockResponse(GURL{kMockEndpoint}, /*response_data=*/"",
                          net::HTTP_OK, net::ERR_FAILED);

  orca_provider->Call(
      input, base::BindLambdaForTesting(
                 [quit_closure = task_environment_.QuitClosure()](
                     base::Value::Dict response) {
                   EXPECT_TRUE(response.FindBool("error").value());
                   EXPECT_TRUE(response.FindInt("error_type").has_value());
                   // kNetError
                   EXPECT_EQ(response.FindInt("error_type").value(), 1);
                   quit_closure.Run();
                 }));
  task_environment_.RunUntilQuit();
}

// Test that malformed response JSON string can be captured with proper
// error_type.
TEST_F(OrcaProviderTest, ParseMalformedJson) {
  std::string post_data = "{invalid json string";

  std::map<std::string, std::string> input = {{"data", "simple post data"},
                                              {"tone", "SHORTEN"}};
  std::unique_ptr<FakeOrcaProvider> orca_provider = CreateOrcaProvider();

  SetEndpointMockResponse(GURL{kMockEndpoint}, /*response_data=*/post_data,
                          net::HTTP_OK, net::OK);

  orca_provider->Call(
      input, base::BindLambdaForTesting(
                 [quit_closure = task_environment_.QuitClosure()](
                     base::Value::Dict response) {
                   EXPECT_TRUE(response.FindBool("error").value());
                   EXPECT_TRUE(response.FindInt("error_type").has_value());
                   // kResultParseError
                   EXPECT_EQ(response.FindInt("error_type").value(), 2);
                   quit_closure.Run();
                 }));
  task_environment_.RunUntilQuit();
}

// Test a successful response can be parsed as base::Value::Dict.
TEST_F(OrcaProviderTest, ParseSuccessfulResponse) {
  std::string post_data = "{\"data\":\"simple post data\"}";

  std::map<std::string, std::string> input = {{"data", "simple post data"},
                                              {"tone", "SHORTEN"}};
  std::unique_ptr<FakeOrcaProvider> orca_provider = CreateOrcaProvider();

  SetEndpointMockResponse(GURL{kMockEndpoint}, /*response_data=*/post_data,
                          net::HTTP_OK, net::OK);

  orca_provider->Call(
      input, base::BindLambdaForTesting(
                 [quit_closure = task_environment_.QuitClosure()](
                     base::Value::Dict response) {
                   EXPECT_FALSE(response.contains("error"));
                   EXPECT_TRUE(response.contains("data"));
                   EXPECT_EQ(*response.FindString("data"), "simple post data");
                   quit_closure.Run();
                 }));
  task_environment_.RunUntilQuit();
}

}  // namespace manta
