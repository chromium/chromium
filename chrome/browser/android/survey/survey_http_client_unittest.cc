// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/survey/survey_http_client.h"

#include <iostream>
#include <ostream>
#include <string>
#include <type_traits>

#include "base/bind.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "chrome/browser/android/survey/http_client_type.h"
#include "net/base/net_errors.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_status_code.h"
#include "net/http/http_util.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/url_loader_completion_status.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace survey {

using ResponseProduceFlags =
    network::TestURLLoaderFactory::ResponseProduceFlags;

struct TestHttpHeaders {
  TestHttpHeaders(std::vector<std::string> header_keys_input = {},
                  std::vector<std::string> header_values_input = {})
      : header_keys(std::move(header_keys_input)),
        header_values(std::move(header_values_input)) {}

  std::vector<std::string> header_keys;
  std::vector<std::string> header_values;

  bool operator==(const TestHttpHeaders& other) const {
    return base::ranges::equal(header_keys, other.header_keys) &&
           base::ranges::equal(header_values, other.header_values);
  }

  friend std::ostream& operator<<(std::ostream& os,
                                  const TestHttpHeaders& header);
};

std::ostream& operator<<(std::ostream& os, const TestHttpHeaders& header) {
  os << "header_keys=[";
  for (auto it = header.header_keys.begin(); it != header.header_keys.end();
       ++it) {
    os << ' ' << *it;
  }
  os << " ] "
     << "header_values=[";
  for (auto it = header.header_values.begin(); it != header.header_values.end();
       ++it) {
    os << ' ' << *it;
  }
  os << " ]";

  return os;
}

struct TestHttpRequest : TestHttpHeaders {
  TestHttpRequest(std::string url_input,
                  std::string request_type = "POST",
                  std::string request_body = "",
                  std::vector<std::string> request_header_keys = {},
                  std::vector<std::string> request_header_values = {})
      : TestHttpHeaders(request_header_keys, request_header_values),
        url(url_input),
        type(request_type),
        body(request_body) {}

  std::string url;
  std::string type;
  std::string body;
};

struct TestHttpResponse : TestHttpHeaders {
  TestHttpResponse(int32_t response_code = net::HTTP_OK,
                   int32_t net_error_code_input = 0,
                   std::string response_body = "",
                   std::vector<std::string> response_header_keys = {},
                   std::vector<std::string> response_header_values = {})
      : TestHttpHeaders(response_header_keys, response_header_values),
        code(response_code),
        net_error_code(net_error_code_input),
        body(response_body) {}

  int32_t code;
  int32_t net_error_code;
  std::string body;

  bool operator==(const TestHttpResponse& other) const {
    return TestHttpHeaders::operator==(other) && code == other.code &&
           net_error_code == other.net_error_code && !body.compare(other.body);
  }

  friend std::ostream& operator<<(std::ostream& os,
                                  const TestHttpResponse& response);
};

std::ostream& operator<<(std::ostream& os, const TestHttpResponse& response) {
  os << "response_code=[" << base::NumberToString(response.code) << "] "
     << "net_error_code=[" << base::NumberToString(response.net_error_code)
     << "] "
     << "body=[" << response.body << "] "
     << static_cast<TestHttpHeaders>(response);

  return os;
}

class MockResponseDoneCallback {
 public:
  MockResponseDoneCallback() : has_run(false) {}

  void Done(int32_t response_code,
            int32_t net_error_code,
            std::vector<uint8_t> response_bytes,
            std::vector<std::string> input_response_header_keys,
            std::vector<std::string> input_response_header_values) {
    EXPECT_FALSE(has_run);
    has_run = true;
    response = TestHttpResponse(
        response_code, net_error_code,
        std::string(response_bytes.begin(), response_bytes.end()),
        std::move(input_response_header_keys),
        std::move(input_response_header_values));
  }

  bool has_run;
  TestHttpResponse response;
};

class SurveyHttpClientTest : public testing::Test {
 public:
  SurveyHttpClientTest(const SurveyHttpClientTest&) = delete;
  SurveyHttpClientTest& operator=(const SurveyHttpClientTest&) = delete;

 protected:
  SurveyHttpClientTest() {}

  ~SurveyHttpClientTest() override {}

  void SetUp() override {
    shared_url_loader_factory_ =
        base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
            &test_url_loader_factory_);

    survey_http_client_ = std::make_unique<SurveyHttpClient>(
        HttpClientType::kSurvey, shared_url_loader_factory_);
  }

  void DestroyService() { survey_http_client_.reset(); }

  SurveyHttpClient* service() { return survey_http_client_.get(); }

  // Helper to check the next request for the network.
  network::TestURLLoaderFactory::PendingRequest* GetLastPendingRequest() {
    EXPECT_GT(test_url_loader_factory_.pending_requests()->size(), 0U)
        << "No pending request!";
    network::TestURLLoaderFactory::PendingRequest* request =
        &(test_url_loader_factory_.pending_requests()->back());
    return request;
  }

  void Respond(
      const GURL& url,
      const TestHttpResponse& response,
      network::URLLoaderCompletionStatus status =
          network::URLLoaderCompletionStatus(),
      ResponseProduceFlags flag = ResponseProduceFlags::kResponseDefault) {
    auto head = network::mojom::URLResponseHead::New();
    if (response.code >= 0) {
      head->headers = base::MakeRefCounted<net::HttpResponseHeaders>("");
      head->headers->ReplaceStatusLine("HTTP/1.1 " +
                                       base::NumberToString(response.code));

      for (size_t i = 0; i < response.header_keys.size(); ++i) {
        head->headers->SetHeader(response.header_keys[i],
                                 response.header_values[i]);
      }

      status.decoded_body_length = response.body.length();
    }

    test_url_loader_factory_.AddResponse(
        url, std::move(head), response.body, status,
        network::TestURLLoaderFactory::Redirects(), flag);

    task_environment_.FastForwardUntilNoTasksRemain();
  }

  void SendRequest(const TestHttpRequest& request,
                   MockResponseDoneCallback* done_callback) {
    GURL req_url(request.url);
    std::vector<uint8_t> request_body_bytes(request.body.begin(),
                                            request.body.end());
    service()->Send(req_url, request.type, request_body_bytes,
                    request.header_keys, request.header_values,
                    base::BindOnce(&MockResponseDoneCallback::Done,
                                   base::Unretained(done_callback)));

    task_environment_.RunUntilIdle();
  }

  void SendRequestAndRespond(const TestHttpRequest& request,
                             const TestHttpResponse& response,
                             network::URLLoaderCompletionStatus status,
                             MockResponseDoneCallback* done_callback) {
    SendRequest(request, done_callback);
    Respond(GURL(request.url), response, status);
  }

  void SendEmptyRequestAndResponseNoHeaders(
      std::string request_url,
      MockResponseDoneCallback* done_callback) {
    SendRequestAndRespond(TestHttpRequest(request_url), TestHttpResponse(),
                          network::URLLoaderCompletionStatus(), done_callback);
  }

  void SendRequestAndValidateResponse(
      const TestHttpRequest& request,
      const TestHttpResponse& response,
      network::URLLoaderCompletionStatus status =
          network::URLLoaderCompletionStatus()) {
    MockResponseDoneCallback done_callback;
    SendRequestAndRespond(request, response, status, &done_callback);

    EXPECT_TRUE(done_callback.has_run);
    EXPECT_EQ(done_callback.response, response);
  }

  network::TestURLLoaderFactory* test_url_loader_factory() {
    return &test_url_loader_factory_;
  }

  base::HistogramTester* histogram_tester() { return &histogram_tester_; }

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

 private:
  std::unique_ptr<SurveyHttpClient> survey_http_client_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  scoped_refptr<network::SharedURLLoaderFactory> shared_url_loader_factory_;
  base::HistogramTester histogram_tester_;
};

TEST_F(SurveyHttpClientTest, TestSendEmptyRequest) {
  SendRequestAndValidateResponse(TestHttpRequest("http://foobar.com/survey"),
                                 TestHttpResponse());
  histogram_tester()->ExpectBucketCount(
      "Net.HttpResponseCode.CustomHttpClient.Survey", net::HTTP_OK, 1);
}

TEST_F(SurveyHttpClientTest, TestSendSimpleRequest) {
  TestHttpRequest request("http://foobar.com/survey", "POST", "?bar=baz&foo=1",
                          {"Content-Type"}, {"application/x-protobuf"});
  TestHttpResponse response(net::HTTP_OK, 0, "{key:'val'}");
  SendRequestAndValidateResponse(request, response);
}

TEST_F(SurveyHttpClientTest, TestSendDifferentRequestMethod) {
  int histogram_counts = 0;

  std::vector<std::string> request_methods({"POST", "PUT", "PATCH"});
  for (const auto& method : request_methods) {
    MockResponseDoneCallback done_callback;
    std::string content_type = "application/x-protobuf";

    TestHttpRequest request("http://foobar.com/survey", method, "request_body",
                            {"Content-Type", "TestMethod"},
                            {content_type, method});
    SendRequest(request, &done_callback);

    {
      auto pendingRequest = GetLastPendingRequest()->request;
      EXPECT_EQ(pendingRequest.method, method);

      std::string content_type_val, method_val;
      pendingRequest.headers.GetHeader("TestMethod", &method_val);
      pendingRequest.headers.GetHeader("Content-Type", &content_type_val);
      EXPECT_EQ(content_type_val, content_type);
      EXPECT_EQ(method_val, method);
    }

    Respond(GURL(request.url), TestHttpResponse());

    task_environment_.FastForwardUntilNoTasksRemain();
    EXPECT_TRUE(done_callback.has_run);
    EXPECT_EQ(done_callback.response.code, net::HTTP_OK);
    histogram_tester()->ExpectBucketCount(
        "Net.HttpResponseCode.CustomHttpClient.Survey", net::HTTP_OK,
        ++histogram_counts);

    test_url_loader_factory()->ClearResponses();
  }
}

TEST_F(SurveyHttpClientTest, TestSendMultipleRequests) {
  MockResponseDoneCallback done_callback1;
  MockResponseDoneCallback done_callback2;
  MockResponseDoneCallback done_callback3;

  SendEmptyRequestAndResponseNoHeaders("http://foobar.com/survey1",
                                       &done_callback1);
  SendEmptyRequestAndResponseNoHeaders("http://foobar.com/survey2",
                                       &done_callback2);
  SendEmptyRequestAndResponseNoHeaders("http://foobar.com/survey3",
                                       &done_callback3);

  task_environment_.FastForwardUntilNoTasksRemain();

  EXPECT_TRUE(done_callback1.has_run);
  EXPECT_TRUE(done_callback2.has_run);
  EXPECT_TRUE(done_callback3.has_run);
  histogram_tester()->ExpectBucketCount(
      "Net.HttpResponseCode.CustomHttpClient.Survey", net::HTTP_OK, 3);
}

TEST_F(SurveyHttpClientTest, TestResponseHeader) {
  TestHttpRequest request("http://foobar.com/survey");
  TestHttpResponse response(
      net::HTTP_OK, 0, /*response_body*/ "",
      /*response_header_keys*/ {"Foo", "Bar"},
      /*response_header_values*/ {"foo_value", "bar_value"});
  SendRequestAndValidateResponse(request, response);
}

TEST_F(SurveyHttpClientTest, TestCancelRequest) {
  MockResponseDoneCallback done_callback;

  GURL url("http://foobar.com/survey");
  service()->Send(url, "GET", /*request_body*/ {}, /*header_keys*/ {},
                  /*header_values*/ {},
                  base::BindOnce(&MockResponseDoneCallback::Done,
                                 base::Unretained(&done_callback)));

  DestroyService();

  Respond(url, TestHttpResponse());

  EXPECT_FALSE(done_callback.has_run);
}

TEST_F(SurveyHttpClientTest, TestRequestTimeout) {
  MockResponseDoneCallback done_callback;
  SendRequest(TestHttpRequest("http://foobar.com/survey"), &done_callback);

  task_environment_.FastForwardUntilNoTasksRemain();

  EXPECT_TRUE(done_callback.has_run);
  EXPECT_EQ(done_callback.response.net_error_code, net::ERR_TIMED_OUT);
  histogram_tester()->ExpectBucketCount(
      "Net.HttpResponseCode.CustomHttpClient.Survey", net::HTTP_OK, 0);
}

TEST_F(SurveyHttpClientTest, TestHttpError) {
  std::vector<net::HttpStatusCode> error_codes(
      {net::HTTP_BAD_REQUEST, net::HTTP_UNAUTHORIZED, net::HTTP_FORBIDDEN,
       net::HTTP_NOT_FOUND, net::HTTP_INTERNAL_SERVER_ERROR,
       net::HTTP_BAD_GATEWAY, net::HTTP_SERVICE_UNAVAILABLE});

  for (const auto& code : error_codes) {
    MockResponseDoneCallback done_callback;
    SendRequestAndRespond(
        TestHttpRequest("http://foobar.com/survey"),
        TestHttpResponse(code, 0, "error_response_data",
                         /*response_header_keys*/ {"Foo", "Bar"},
                         /*response_header_values*/ {"foo_value", "bar_value"}),
        network::URLLoaderCompletionStatus(), &done_callback);

    // The expected response should have the code override by network error, and
    // empty response body, and the same headers.
    TestHttpResponse expected_response =
        TestHttpResponse(code, net::ERR_HTTP_RESPONSE_CODE_FAILURE, "",
                         /*response_header_keys*/ {"Foo", "Bar"},
                         /*response_header_values*/ {"foo_value", "bar_value"});

    EXPECT_TRUE(done_callback.has_run);
    EXPECT_EQ(done_callback.response, expected_response);
    histogram_tester()->ExpectBucketCount(
        "Net.HttpResponseCode.CustomHttpClient.Survey", code, 1);

    test_url_loader_factory()->ClearResponses();
  }
}

// TODO(crbug.com/1378159): Fix and re-enable this test.
TEST_F(SurveyHttpClientTest, DISABLED_TestNetworkError) {
  std::vector<int32_t> error_codes(
      {net::ERR_CERT_COMMON_NAME_INVALID, net::ERR_CERT_DATE_INVALID,
       net::ERR_CERT_WEAK_KEY, net::ERR_NAME_RESOLUTION_FAILED});

  for (const auto& code : error_codes) {
    MockResponseDoneCallback done_callback;
    SendRequestAndRespond(
        TestHttpRequest("http://foobar.com/survey"),
        TestHttpResponse(net::HTTP_OK, 0, "success_response_data",
                         /*response_header_keys*/ {"Foo", "Bar"},
                         /*response_header_values*/ {"foo_value", "bar_value"}),
        network::URLLoaderCompletionStatus(code), &done_callback);

    // The expected response should have the code override by network error, and
    // empty response body, and the same headers.
    TestHttpResponse expected_response =
        TestHttpResponse(0, code, "",
                         /*response_header_keys*/ {"Foo", "Bar"},
                         /*response_header_values*/ {"foo_value", "bar_value"});

    EXPECT_TRUE(done_callback.has_run);
    EXPECT_EQ(done_callback.response, expected_response);

    test_url_loader_factory()->ClearResponses();
  }

  // Response code should not be recorded when net error occurred.
  histogram_tester()->ExpectTotalCount(
      "Net.HttpResponseCode.CustomHttpClient.Survey", 0);
}

TEST_F(SurveyHttpClientTest, TestNetworkErrorAfterSendHeaders) {
  std::vector<int32_t> error_codes(
      {net::ERR_CERT_COMMON_NAME_INVALID, net::ERR_CERT_DATE_INVALID,
       net::ERR_CERT_WEAK_KEY, net::ERR_NAME_RESOLUTION_FAILED});

  std::string url = "http://foobar.com/survey";
  GURL gurl(url);
  for (const auto& code : error_codes) {
    MockResponseDoneCallback done_callback;
    SendRequest(TestHttpRequest(url), &done_callback);

    Respond(
        gurl,
        TestHttpResponse(net::HTTP_OK, 0, "success_response_data",
                         /*response_header_keys*/ {"Foo", "Bar"},
                         /*response_header_values*/ {"foo_value", "bar_value"}),
        network::URLLoaderCompletionStatus(code),
        ResponseProduceFlags::kSendHeadersOnNetworkError);

    // The expected response should have the code override by network error, and
    // empty response body, and the same headers.
    TestHttpResponse expected_response =
        TestHttpResponse(net::HTTP_OK, code, "",
                         /*response_header_keys*/ {"Foo", "Bar"},
                         /*response_header_values*/ {"foo_value", "bar_value"});

    EXPECT_TRUE(done_callback.has_run);
    EXPECT_EQ(done_callback.response, expected_response);

    test_url_loader_factory()->ClearResponses();
  }
}

}  // namespace survey
