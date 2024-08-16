// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/httpclient/http_client.h"

#include <iostream>
#include <ostream>
#include <string>
#include <type_traits>

#include "base/functional/bind.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_number_conversions.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/browser_task_environment.h"
#include "net/base/net_errors.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_status_code.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/url_loader_completion_status.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace httpclient {

using ResponseProduceFlags =
    network::TestURLLoaderFactory::ResponseProduceFlags;

struct TestHttpRequest {
  explicit TestHttpRequest(
      std::string url_input,
      std::string request_type = "POST",
      std::string request_body = "",
      std::map<std::string, std::string> request_headers = {})
      : headers(request_headers),
        url(url_input),
        type(request_type),
        body(request_body) {}
  std::map<std::string, std::string> headers;
  std::string url;
  std::string type;
  std::string body;
};

struct TestHttpResponse {
  explicit TestHttpResponse(
      int32_t http_status_input = net::HTTP_OK,
      int32_t net_error_code_input = 0,
      std::string response_body = "",
      std::map<std::string, std::string> response_headers = {})
      : headers(response_headers),
        http_status(http_status_input),
        net_error_code(net_error_code_input),
        body(response_body) {}

  std::map<std::string, std::string> headers;
  int32_t http_status;
  int32_t net_error_code;
  std::string body;

  bool operator==(const TestHttpResponse& other) const {
    return headers == other.headers && http_status == other.http_status &&
           net_error_code == other.net_error_code && !body.compare(other.body);
  }

  friend std::ostream& operator<<(std::ostream& os,
                                  const TestHttpResponse& response);
};

std::ostream& operator<<(std::ostream& os, const TestHttpResponse& response) {
  os << "http_status=[" << base::NumberToString(response.http_status) << "] ";
  os << "net_error_code=[" << base::NumberToString(response.net_error_code)
     << "] ";
  os << "body=[" << response.body << "] ";
  os << "headers={";
  bool first = true;
  for (auto const& [key, value] : response.headers) {
    if (!first) {
      os << ", ";
    }
    first = false;
    os << key << "=" << value;
  }
  os << "}";
  return os;
}

class MockResponseDoneCallback {
 public:
  MockResponseDoneCallback() = default;

  void Done(int32_t http_status,
            int32_t net_error_code,
            std::vector<uint8_t>&& response_bytes,
            std::map<std::string, std::string>&& input_response_headers) {
    EXPECT_FALSE(has_run);
    has_run = true;
    response = TestHttpResponse(
        http_status, net_error_code,
        std::string(response_bytes.begin(), response_bytes.end()),
        std::move(input_response_headers));
  }

  bool has_run{false};
  TestHttpResponse response;
};

class HttpClientTest : public testing::Test {
 public:
  HttpClientTest(const HttpClientTest&) = delete;
  HttpClientTest& operator=(const HttpClientTest&) = delete;

 protected:
  HttpClientTest() = default;

  ~HttpClientTest() override = default;

  void SetUp() override {
    shared_url_loader_factory_ =
        base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
            &test_url_loader_factory_);

    http_client_ = std::make_unique<HttpClient>(shared_url_loader_factory_);
  }

  void DestroyService() { http_client_.reset(); }

  HttpClient* service() { return http_client_.get(); }

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
      TestHttpResponse response,
      network::URLLoaderCompletionStatus status =
          network::URLLoaderCompletionStatus(),
      ResponseProduceFlags flag = ResponseProduceFlags::kResponseDefault) {
    auto head = network::mojom::URLResponseHead::New();
    if (response.http_status >= 0) {
      head->headers = base::MakeRefCounted<net::HttpResponseHeaders>("");
      head->headers->ReplaceStatusLine(
          "HTTP/1.1 " + base::NumberToString(response.http_status));

      for (auto const& [key, value] : response.headers) {
        head->headers->SetHeader(key, value);
      }

      status.decoded_body_length = response.body.length();
    }

    test_url_loader_factory_.AddResponse(
        url, std::move(head), response.body, status,
        network::TestURLLoaderFactory::Redirects(), flag);

    task_environment_.FastForwardUntilNoTasksRemain();
  }

  void SendRequest(TestHttpRequest request,
                   MockResponseDoneCallback* done_callback) {
    GURL req_url(request.url);
    std::vector<uint8_t> request_body_bytes(request.body.begin(),
                                            request.body.end());
    std::map<std::string, std::string> headers(request.headers);
    content::GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE,
        base::BindOnce(
            &HttpClient::Send, base::Unretained(service()), req_url,
            request.type, std::move(request_body_bytes), std::move(headers),
            net::NetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS),
            base::BindOnce(&MockResponseDoneCallback::Done,
                           base::Unretained(done_callback))));

    task_environment_.RunUntilIdle();
  }

  void SendRequestAndRespond(TestHttpRequest request,
                             TestHttpResponse response,
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

  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

 private:
  std::unique_ptr<HttpClient> http_client_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  scoped_refptr<network::SharedURLLoaderFactory> shared_url_loader_factory_;
};

TEST_F(HttpClientTest, TestSendEmptyRequest) {
  SendRequestAndValidateResponse(TestHttpRequest("http://foobar.com/survey"),
                                 TestHttpResponse());
}

TEST_F(HttpClientTest, TestSendSimpleRequest) {
  std::map<std::string, std::string> request_headers;
  request_headers["Content-Type"] = "application/x-protobuf";
  TestHttpRequest request("http://foobar.com/survey", "POST", "?bar=baz&foo=1",
                          request_headers);
  TestHttpResponse response(net::HTTP_OK, 0, "{key:'val'}");
  SendRequestAndValidateResponse(request, response);
}

TEST_F(HttpClientTest, TestSendDifferentRequestMethod) {
  std::vector<std::string> request_methods({"POST", "PUT", "PATCH"});
  for (const auto& method : request_methods) {
    MockResponseDoneCallback done_callback;
    std::string content_type = "application/x-protobuf";
    std::map<std::string, std::string> request_headers;
    request_headers["Content-Type"] = content_type;
    request_headers["TestMethod"] = method;
    TestHttpRequest request("http://foobar.com/survey", method, "request_body",
                            request_headers);
    SendRequest(request, &done_callback);

    {
      auto pendingRequest = GetLastPendingRequest()->request;
      EXPECT_EQ(pendingRequest.method, method);

      EXPECT_EQ(pendingRequest.headers.GetHeader("Content-Type"), content_type);
      EXPECT_EQ(pendingRequest.headers.GetHeader("TestMethod"), method);
    }

    Respond(GURL(request.url), TestHttpResponse());

    task_environment_.FastForwardUntilNoTasksRemain();
    EXPECT_TRUE(done_callback.has_run);
    EXPECT_EQ(done_callback.response.http_status, net::HTTP_OK);

    test_url_loader_factory()->ClearResponses();
  }
}

TEST_F(HttpClientTest, TestSendMultipleRequests) {
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
}

TEST_F(HttpClientTest, TestResponseHeader) {
  TestHttpRequest request("http://foobar.com/survey");
  std::map<std::string, std::string> response_headers;
  response_headers["Foo"] = "foo_value";
  response_headers["Bar"] = "bar_value";
  TestHttpResponse response(net::HTTP_OK, 0, /*response_body*/ "",
                            response_headers);
  SendRequestAndValidateResponse(request, response);
}

TEST_F(HttpClientTest, TestCancelRequest) {
  MockResponseDoneCallback done_callback;

  GURL url("http://foobar.com/survey");
  service()->Send(
      url, "GET", /*request_body*/ {}, /*headers*/ {},
      net::NetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS),
      base::BindOnce(&MockResponseDoneCallback::Done,
                     base::Unretained(&done_callback)));

  DestroyService();

  Respond(url, TestHttpResponse());

  EXPECT_FALSE(done_callback.has_run);
}

TEST_F(HttpClientTest, TestRequestTimeout) {
  MockResponseDoneCallback done_callback;
  SendRequest(TestHttpRequest("http://foobar.com/survey"), &done_callback);

  task_environment_.FastForwardUntilNoTasksRemain();

  EXPECT_TRUE(done_callback.has_run);
  EXPECT_EQ(done_callback.response.net_error_code, net::ERR_TIMED_OUT);
}

TEST_F(HttpClientTest, TestHttpError) {
  std::vector<net::HttpStatusCode> error_codes(
      {net::HTTP_BAD_REQUEST, net::HTTP_UNAUTHORIZED, net::HTTP_FORBIDDEN,
       net::HTTP_NOT_FOUND, net::HTTP_INTERNAL_SERVER_ERROR,
       net::HTTP_BAD_GATEWAY, net::HTTP_SERVICE_UNAVAILABLE});
  std::map<std::string, std::string> response_headers;
  response_headers["Foo"] = "foo_value";
  response_headers["Bar"] = "bar_value";

  for (const auto& http_status : error_codes) {
    MockResponseDoneCallback done_callback;
    SendRequestAndRespond(
        TestHttpRequest("http://foobar.com/survey"),
        TestHttpResponse(http_status, 0, "error_response_data",
                         response_headers),
        network::URLLoaderCompletionStatus(), &done_callback);

    TestHttpResponse expected_response = TestHttpResponse(
        http_status, 0, "error_response_data", response_headers);

    EXPECT_TRUE(done_callback.has_run);
    EXPECT_EQ(done_callback.response, expected_response);

    test_url_loader_factory()->ClearResponses();
  }
}

TEST_F(HttpClientTest, TestNetworkError) {
  std::vector<int32_t> error_codes(
      {net::ERR_CERT_COMMON_NAME_INVALID, net::ERR_CERT_DATE_INVALID,
       net::ERR_CERT_WEAK_KEY, net::ERR_NAME_RESOLUTION_FAILED});
  std::map<std::string, std::string> response_headers;
  response_headers["Foo"] = "foo_value";
  response_headers["Bar"] = "bar_value";

  for (const auto& code : error_codes) {
    MockResponseDoneCallback done_callback;
    SendRequestAndRespond(
        TestHttpRequest("http://foobar.com/survey"),
        TestHttpResponse(net::HTTP_OK, 0, "success_response_data",
                         response_headers),
        network::URLLoaderCompletionStatus(code), &done_callback);

    // The expected response should have the code override by network error, and
    // empty response body, and empty headers.
    TestHttpResponse expected_response = TestHttpResponse(0, code, "");

    EXPECT_TRUE(done_callback.has_run);
    EXPECT_EQ(done_callback.response, expected_response);

    test_url_loader_factory()->ClearResponses();
  }
}

TEST_F(HttpClientTest, TestNetworkErrorAfterSendHeaders) {
  std::vector<int32_t> error_codes(
      {net::ERR_CERT_COMMON_NAME_INVALID, net::ERR_CERT_DATE_INVALID,
       net::ERR_CERT_WEAK_KEY, net::ERR_NAME_RESOLUTION_FAILED});
  std::map<std::string, std::string> response_headers;
  response_headers["Foo"] = "foo_value";
  response_headers["Bar"] = "bar_value";

  std::string url = "http://foobar.com/survey";
  GURL gurl(url);
  for (const auto& code : error_codes) {
    MockResponseDoneCallback done_callback;
    SendRequest(TestHttpRequest(url), &done_callback);

    Respond(gurl,
            TestHttpResponse(net::HTTP_OK, 0, "success_response_data",
                             response_headers),
            network::URLLoaderCompletionStatus(code),
            ResponseProduceFlags::kSendHeadersOnNetworkError);

    // The expected response should have the code override by network error, and
    // empty response body, and the same headers.
    TestHttpResponse expected_response =
        TestHttpResponse(net::HTTP_OK, code, "", response_headers);

    EXPECT_TRUE(done_callback.has_run);
    EXPECT_EQ(done_callback.response, expected_response);

    test_url_loader_factory()->ClearResponses();
  }
}

}  // namespace httpclient
