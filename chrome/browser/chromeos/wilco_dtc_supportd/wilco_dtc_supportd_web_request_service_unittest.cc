// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/shared_memory_mapping.h"
#include "base/run_loop.h"
#include "base/strings/string_piece.h"
#include "base/strings/stringprintf.h"
#include "base/test/task_environment.h"
#include "chrome/browser/chromeos/wilco_dtc_supportd/mojo_utils.h"
#include "chrome/services/wilco_dtc_supportd/public/mojom/wilco_dtc_supportd.mojom.h"
#include "net/base/net_errors.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_status_code.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "services/network/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

#include "chrome/browser/chromeos/wilco_dtc_supportd/wilco_dtc_supportd_web_request_service.h"

namespace chromeos {

namespace {

constexpr char kFakeUrl[] = "https://fake.url.com";
constexpr char kLocalhostUrl[] = "https://localhost:8000/";
constexpr char kIncorrectHttpUrl[] = "http://fake.url.com";
constexpr char kInvalidUrl[] = "\0\0\1invalid_url";
constexpr char kFakeRequestBody[] = "Fake\0Request\11Body\n\0";
constexpr char kFakeResponseBody[] = "Fake\11Response\0\0\nBody";

// Tests for the WilcoDtcSupportdWebRequestService class.
class WilcoDtcSupportdWebRequestServiceTest : public testing::Test {
 protected:
  struct WebRequestResult {
    WebRequestResult(
        wilco_dtc_supportd::mojom::WilcoDtcSupportdWebRequestStatus status,
        int http_status,
        mojo::ScopedHandle response_body_handle)
        : status(status), http_status(http_status) {
      if (!response_body_handle.is_valid()) {
        response_body = "";
        return;
      }
      base::ReadOnlySharedMemoryMapping shared_memory;
      response_body = std::string(GetStringPieceFromMojoHandle(
          std::move(response_body_handle), &shared_memory));
      if (!shared_memory.IsValid()) {
        response_body = "";
        return;
      }
    }

    wilco_dtc_supportd::mojom::WilcoDtcSupportdWebRequestStatus status;
    int http_status;
    std::string response_body;
  };
  WilcoDtcSupportdWebRequestServiceTest() {
    web_request_service_ = std::make_unique<WilcoDtcSupportdWebRequestService>(
        base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
            &test_url_loader_factory_));
  }

  ~WilcoDtcSupportdWebRequestServiceTest() override {}

  void TearDown() override { test_url_loader_factory_.ClearResponses(); }

  // Start new web request with the next parameters:
  // * web request parameters:
  //   * |http_method|
  //   * |url|
  //   * |headers|
  //   * |request_body|
  // * |request_result| - once the request is complete, this structure contains
  //                      web response.
  // * |run_loop| - the current run loop.
  void StartWebRequest(
      wilco_dtc_supportd::mojom::WilcoDtcSupportdWebRequestHttpMethod
          http_method,
      const std::string& url,
      const std::vector<base::StringPiece>& headers,
      const std::string& request_body,
      std::unique_ptr<WebRequestResult>* request_result,
      base::RunLoop* run_loop) {
    web_request_service()->PerformRequest(
        http_method, GURL(url), headers, request_body,
        base::BindOnce(
            &WilcoDtcSupportdWebRequestServiceTest::OnRequestComplete,
            base::Unretained(this), request_result, run_loop->QuitClosure()));
  }

  // Injects the web response for |url|.
  void InjectNetworkResponse(
      const std::string& url,
      std::unique_ptr<net::HttpStatusCode> response_status,
      net::Error net_error,
      const std::string& response_body) {
    auto response_head = response_status
                             ? network::CreateURLResponseHead(*response_status)
                             : network::mojom::URLResponseHead::New();
    test_url_loader_factory_.AddResponse(
        GURL(url), std::move(response_head), response_body,
        network::URLLoaderCompletionStatus(net_error));
  }

  void DestroyService() { web_request_service_.reset(); }

  WilcoDtcSupportdWebRequestService* web_request_service() {
    return web_request_service_.get();
  }

  // Returns a Content-Type header value or empty string if none.
  std::string GetContentTypeFromPendingRequest(const std::string& url) {
    const network::ResourceRequest* request;
    if (!test_url_loader_factory_.IsPending(GURL(url).spec(), &request) ||
        !request) {
      return "";
    }
    std::string content_type_value;
    if (!request->headers.GetHeader(net::HttpRequestHeaders::kContentType,
                                    &content_type_value)) {
      return "";
    }
    return content_type_value;
  }

 private:
  void OnRequestComplete(
      std::unique_ptr<WebRequestResult>* request_result,
      base::RepeatingClosure callback,
      wilco_dtc_supportd::mojom::WilcoDtcSupportdWebRequestStatus status,
      int http_status,
      mojo::ScopedHandle response_body) {
    auto response = std::make_unique<WebRequestResult>(
        status, http_status, std::move(response_body));
    *request_result = std::move(response);
    std::move(callback).Run();
  }

  std::unique_ptr<WilcoDtcSupportdWebRequestService> web_request_service_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  base::test::SingleThreadTaskEnvironment task_environment_;
};

}  // namespace

TEST_F(WilcoDtcSupportdWebRequestServiceTest, HttpMethodInvalid) {
  std::unique_ptr<WebRequestResult> request_result;
  base::RunLoop run_loop;

  const auto kInvalidHttpMethod = static_cast<
      wilco_dtc_supportd::mojom::WilcoDtcSupportdWebRequestHttpMethod>(
      static_cast<int>(wilco_dtc_supportd::mojom::
                           WilcoDtcSupportdWebRequestHttpMethod::kMaxValue) +
      1);

  StartWebRequest(kInvalidHttpMethod, kFakeUrl, {} /* headers */,
                  kFakeRequestBody, &request_result, &run_loop);
  // The test fails with a network error on the same thread.
  ASSERT_TRUE(request_result);
  EXPECT_EQ(request_result->status,
            wilco_dtc_supportd::mojom::WilcoDtcSupportdWebRequestStatus::
                kNetworkError);
  EXPECT_EQ(request_result->http_status, 0);
  EXPECT_EQ(request_result->response_body, "");
}

TEST_F(WilcoDtcSupportdWebRequestServiceTest, HttpMethodGetNonEmptyBody) {
  std::unique_ptr<WebRequestResult> request_result;
  base::RunLoop run_loop;

  StartWebRequest(
      wilco_dtc_supportd::mojom::WilcoDtcSupportdWebRequestHttpMethod::kGet,
      kFakeUrl, {} /* headers */, kFakeRequestBody, &request_result, &run_loop);
  // The test fails with a network error on the same thread.
  ASSERT_TRUE(request_result);
  EXPECT_EQ(request_result->status,
            wilco_dtc_supportd::mojom::WilcoDtcSupportdWebRequestStatus::
                kNetworkError);
  EXPECT_EQ(request_result->http_status, 0);
  EXPECT_EQ(request_result->response_body, "");
}

TEST_F(WilcoDtcSupportdWebRequestServiceTest, HttpMethodHeadEmptyBody) {
  std::unique_ptr<WebRequestResult> request_result;
  base::RunLoop run_loop;

  StartWebRequest(
      wilco_dtc_supportd::mojom::WilcoDtcSupportdWebRequestHttpMethod::kHead,
      kFakeUrl, {} /* headers */, "" /* request_body */, &request_result,
      &run_loop);
  EXPECT_FALSE(request_result);
  InjectNetworkResponse(kFakeUrl,
                        std::make_unique<net::HttpStatusCode>(net::HTTP_OK),
                        net::OK, "" /* response_body */);
  run_loop.Run();
  ASSERT_TRUE(request_result);
  EXPECT_EQ(request_result->status,
            wilco_dtc_supportd::mojom::WilcoDtcSupportdWebRequestStatus::kOk);
  EXPECT_EQ(request_result->http_status, net::HTTP_OK);
  EXPECT_EQ(request_result->response_body, "");
}

TEST_F(WilcoDtcSupportdWebRequestServiceTest, HttpMethodPostNonEmptyBody) {
  constexpr char kContentTypeValue[] = "text/xml";
  const std::string kContentTypeHeader = base::StringPrintf(
      "%s:%s", net::HttpRequestHeaders::kContentType, kContentTypeValue);
  std::unique_ptr<WebRequestResult> request_result;
  base::RunLoop run_loop;

  StartWebRequest(
      wilco_dtc_supportd::mojom::WilcoDtcSupportdWebRequestHttpMethod::kPost,
      kFakeUrl, {kContentTypeHeader}, kFakeRequestBody, &request_result,
      &run_loop);
  EXPECT_FALSE(request_result);
  EXPECT_EQ(kContentTypeValue, GetContentTypeFromPendingRequest(kFakeUrl));
  InjectNetworkResponse(kFakeUrl,
                        std::make_unique<net::HttpStatusCode>(net::HTTP_OK),
                        net::OK, kFakeResponseBody);
  run_loop.Run();
  ASSERT_TRUE(request_result);
  EXPECT_EQ(request_result->status,
            wilco_dtc_supportd::mojom::WilcoDtcSupportdWebRequestStatus::kOk);
  EXPECT_EQ(request_result->http_status, net::HTTP_OK);
  EXPECT_EQ(request_result->response_body, kFakeResponseBody);
}

TEST_F(WilcoDtcSupportdWebRequestServiceTest, HttpMethodPutEmptyBody) {
  std::unique_ptr<WebRequestResult> request_result;
  base::RunLoop run_loop;

  StartWebRequest(
      wilco_dtc_supportd::mojom::WilcoDtcSupportdWebRequestHttpMethod::kPut,
      kFakeUrl, {} /* headers */, "" /* request_body */, &request_result,
      &run_loop);
  EXPECT_FALSE(request_result);
  InjectNetworkResponse(kFakeUrl,
                        std::make_unique<net::HttpStatusCode>(net::HTTP_OK),
                        net::OK, "" /* response_body */);
  run_loop.Run();
  ASSERT_TRUE(request_result);
  EXPECT_EQ(request_result->status,
            wilco_dtc_supportd::mojom::WilcoDtcSupportdWebRequestStatus::kOk);
  EXPECT_EQ(request_result->http_status, net::HTTP_OK);
  EXPECT_EQ(request_result->response_body, "");
}

TEST_F(WilcoDtcSupportdWebRequestServiceTest, ResponseCodeParsingError) {
  std::unique_ptr<WebRequestResult> request_result;
  base::RunLoop run_loop;

  StartWebRequest(
      wilco_dtc_supportd::mojom::WilcoDtcSupportdWebRequestHttpMethod::kPost,
      kFakeUrl, {} /* headers */, kFakeRequestBody, &request_result, &run_loop);
  EXPECT_FALSE(request_result);
  InjectNetworkResponse(kFakeUrl, nullptr /* response_status */, net::OK,
                        "" /* response_body */);
  run_loop.Run();
  ASSERT_TRUE(request_result);
  EXPECT_EQ(
      request_result->status,
      wilco_dtc_supportd::mojom::WilcoDtcSupportdWebRequestStatus::kHttpError);
  EXPECT_EQ(request_result->http_status, net::HTTP_INTERNAL_SERVER_ERROR);
  EXPECT_EQ(request_result->response_body, "");
}

TEST_F(WilcoDtcSupportdWebRequestServiceTest,
       ResponseCodeParsingErrorNetError) {
  std::unique_ptr<WebRequestResult> request_result;
  base::RunLoop run_loop;

  StartWebRequest(
      wilco_dtc_supportd::mojom::WilcoDtcSupportdWebRequestHttpMethod::kGet,
      kFakeUrl, {} /* headers */, "" /* request_body */, &request_result,
      &run_loop);
  EXPECT_FALSE(request_result);
  InjectNetworkResponse(kFakeUrl, nullptr /* response_status */,
                        net::ERR_CERT_INVALID, kFakeResponseBody);
  run_loop.Run();
  ASSERT_TRUE(request_result);
  EXPECT_EQ(request_result->status,
            wilco_dtc_supportd::mojom::WilcoDtcSupportdWebRequestStatus::
                kNetworkError);
  EXPECT_EQ(request_result->http_status, 0);
  EXPECT_EQ(request_result->response_body, "");
}

TEST_F(WilcoDtcSupportdWebRequestServiceTest, HttpStatusOkNetError) {
  std::unique_ptr<WebRequestResult> request_result;
  base::RunLoop run_loop;

  StartWebRequest(
      wilco_dtc_supportd::mojom::WilcoDtcSupportdWebRequestHttpMethod::kPost,
      kFakeUrl, {} /* headers */, kFakeRequestBody, &request_result, &run_loop);
  EXPECT_FALSE(request_result);
  InjectNetworkResponse(kFakeUrl,
                        std::make_unique<net::HttpStatusCode>(net::HTTP_OK),
                        net::ERR_CERT_INVALID, kFakeResponseBody);
  run_loop.Run();
  ASSERT_TRUE(request_result);
  EXPECT_EQ(request_result->status,
            wilco_dtc_supportd::mojom::WilcoDtcSupportdWebRequestStatus::
                kNetworkError);
  EXPECT_EQ(request_result->http_status, 0);
  EXPECT_EQ(request_result->response_body, "");
}

TEST_F(WilcoDtcSupportdWebRequestServiceTest, HttpErrorNetError) {
  std::unique_ptr<WebRequestResult> request_result;
  base::RunLoop run_loop;

  StartWebRequest(
      wilco_dtc_supportd::mojom::WilcoDtcSupportdWebRequestHttpMethod::kPost,
      kFakeUrl, {} /* headers */, kFakeRequestBody, &request_result, &run_loop);
  EXPECT_FALSE(request_result);
  InjectNetworkResponse(
      kFakeUrl, std::make_unique<net::HttpStatusCode>(net::HTTP_BAD_REQUEST),
      net::ERR_CERT_INVALID, kFakeResponseBody);
  run_loop.Run();
  ASSERT_TRUE(request_result);
  EXPECT_EQ(request_result->status,
            wilco_dtc_supportd::mojom::WilcoDtcSupportdWebRequestStatus::
                kNetworkError);
  EXPECT_EQ(request_result->http_status, 0);
  EXPECT_EQ(request_result->response_body, "");
}

TEST_F(WilcoDtcSupportdWebRequestServiceTest,
       DestroyServiceWithActiveWebRequest) {
  std::unique_ptr<WebRequestResult> request_result;
  base::RunLoop run_loop;

  StartWebRequest(
      wilco_dtc_supportd::mojom::WilcoDtcSupportdWebRequestHttpMethod::kPost,
      kFakeUrl, {} /* headers */, kFakeRequestBody, &request_result, &run_loop);
  InjectNetworkResponse(kFakeUrl,
                        std::make_unique<net::HttpStatusCode>(net::HTTP_OK),
                        net::OK, kFakeResponseBody);
  EXPECT_FALSE(request_result);
  DestroyService();
  run_loop.Run();

  ASSERT_TRUE(request_result);
  EXPECT_EQ(request_result->status,
            wilco_dtc_supportd::mojom::WilcoDtcSupportdWebRequestStatus::
                kNetworkError);
  EXPECT_EQ(request_result->http_status, 0);
  EXPECT_EQ(request_result->response_body, "");
}

TEST_F(WilcoDtcSupportdWebRequestServiceTest, TwoWebRequests) {
  constexpr int kNumberOfRequests = 2;

  std::unique_ptr<WebRequestResult> request_results[kNumberOfRequests];
  base::RunLoop run_loops[kNumberOfRequests];

  for (int i = 0; i < kNumberOfRequests; ++i) {
    StartWebRequest(
        wilco_dtc_supportd::mojom::WilcoDtcSupportdWebRequestHttpMethod::kPut,
        kFakeUrl, {} /* headers */, kFakeRequestBody, &request_results[i],
        &run_loops[i]);
    InjectNetworkResponse(kFakeUrl,
                          std::make_unique<net::HttpStatusCode>(net::HTTP_OK),
                          net::OK, kFakeResponseBody);
    EXPECT_FALSE(request_results[i]);
  }

  // The first request is active and the second is in the queue.
  EXPECT_EQ(kNumberOfRequests - 1,
            web_request_service()->request_queue_size_for_testing());
  for (auto& run_loop : run_loops) {
    run_loop.Run();
  }
  EXPECT_EQ(0, web_request_service()->request_queue_size_for_testing());

  for (const auto& request_result : request_results) {
    ASSERT_TRUE(request_result);
    EXPECT_EQ(request_result->status,
              wilco_dtc_supportd::mojom::WilcoDtcSupportdWebRequestStatus::kOk);
    EXPECT_EQ(request_result->http_status, net::HTTP_OK);
    EXPECT_EQ(request_result->response_body, kFakeResponseBody);
  }
}

TEST_F(WilcoDtcSupportdWebRequestServiceTest, RequestQueueOverflow) {
  // The number of requests in the queue is
  // kWilcoDtcSupportdRequestQueueMaxSize. One is already pending.
  std::unique_ptr<WebRequestResult>
      request_results[kWilcoDtcSupportdWebRequestQueueMaxSize + 1];
  base::RunLoop run_loops[kWilcoDtcSupportdWebRequestQueueMaxSize + 1];

  for (int i = 0; i < kWilcoDtcSupportdWebRequestQueueMaxSize + 1; ++i) {
    StartWebRequest(
        wilco_dtc_supportd::mojom::WilcoDtcSupportdWebRequestHttpMethod::kPut,
        kFakeUrl, {} /* headers */, kFakeRequestBody, &request_results[i],
        &run_loops[i]);
    InjectNetworkResponse(kFakeUrl,
                          std::make_unique<net::HttpStatusCode>(net::HTTP_OK),
                          net::OK, kFakeResponseBody);
    EXPECT_FALSE(request_results[i]);
  }
  EXPECT_EQ(kWilcoDtcSupportdWebRequestQueueMaxSize,
            web_request_service()->request_queue_size_for_testing());

  // Try to add one more. Should fail with kNetworkError.
  {
    std::unique_ptr<WebRequestResult> request_result;
    base::RunLoop run_loop;
    StartWebRequest(
        wilco_dtc_supportd::mojom::WilcoDtcSupportdWebRequestHttpMethod::kPut,
        kFakeUrl, {} /* headers */, kFakeRequestBody, &request_result,
        &run_loop);
    InjectNetworkResponse(kFakeUrl,
                          std::make_unique<net::HttpStatusCode>(net::HTTP_OK),
                          net::OK, kFakeResponseBody);
    // The test fails with a network error on the same thread.
    EXPECT_TRUE(request_result);
    EXPECT_EQ(request_result->status,
              wilco_dtc_supportd::mojom::WilcoDtcSupportdWebRequestStatus::
                  kNetworkError);
    EXPECT_EQ(request_result->http_status, 0);
    EXPECT_EQ(request_result->response_body, "");
  }
  for (auto& run_loop : run_loops) {
    run_loop.Run();
  }
  for (const auto& request_result : request_results) {
    EXPECT_TRUE(request_result);
    EXPECT_EQ(request_result->status,
              wilco_dtc_supportd::mojom::WilcoDtcSupportdWebRequestStatus::kOk);
    EXPECT_EQ(request_result->http_status, net::HTTP_OK);
    EXPECT_EQ(request_result->response_body, kFakeResponseBody);
  }
}

TEST_F(WilcoDtcSupportdWebRequestServiceTest, ResponseBodyMaxSize) {
  std::unique_ptr<WebRequestResult> request_result;
  base::RunLoop run_loop;

  StartWebRequest(
      wilco_dtc_supportd::mojom::WilcoDtcSupportdWebRequestHttpMethod::kHead,
      kFakeUrl, {} /* headers */, "" /* request_body */, &request_result,
      &run_loop);
  EXPECT_FALSE(request_result);
  InjectNetworkResponse(
      kFakeUrl, std::make_unique<net::HttpStatusCode>(net::HTTP_OK), net::OK,
      std::string(kWilcoDtcSupportdWebResponseMaxSizeInBytes, 'A'));
  run_loop.Run();
  ASSERT_TRUE(request_result);
  EXPECT_EQ(request_result->status,
            wilco_dtc_supportd::mojom::WilcoDtcSupportdWebRequestStatus::kOk);
  EXPECT_EQ(request_result->http_status, net::HTTP_OK);
  EXPECT_EQ(request_result->response_body,
            std::string(kWilcoDtcSupportdWebResponseMaxSizeInBytes, 'A'));
}

TEST_F(WilcoDtcSupportdWebRequestServiceTest, ResponseBodyOverflow) {
  std::unique_ptr<WebRequestResult> request_result;
  base::RunLoop run_loop;

  StartWebRequest(
      wilco_dtc_supportd::mojom::WilcoDtcSupportdWebRequestHttpMethod::kHead,
      kFakeUrl, {} /* headers */, "" /* request_body */, &request_result,
      &run_loop);
  EXPECT_FALSE(request_result);
  InjectNetworkResponse(
      kFakeUrl, std::make_unique<net::HttpStatusCode>(net::HTTP_OK), net::OK,
      std::string(kWilcoDtcSupportdWebResponseMaxSizeInBytes + 1, 'A'));
  run_loop.Run();
  ASSERT_TRUE(request_result);
  EXPECT_EQ(request_result->status,
            wilco_dtc_supportd::mojom::WilcoDtcSupportdWebRequestStatus::
                kNetworkError);
  EXPECT_EQ(request_result->http_status, 0);
  EXPECT_EQ(request_result->response_body, "");
}

TEST_F(WilcoDtcSupportdWebRequestServiceTest, LocalhostRequestNetworkError) {
  std::unique_ptr<WebRequestResult> request_result;
  base::RunLoop run_loop;

  StartWebRequest(
      wilco_dtc_supportd::mojom::WilcoDtcSupportdWebRequestHttpMethod::kHead,
      kLocalhostUrl, {} /* headers */, "" /* request_body */, &request_result,
      &run_loop);
  // The test fails with a network error on the same thread.
  run_loop.Run();
  ASSERT_TRUE(request_result);
  EXPECT_EQ(request_result->status,
            wilco_dtc_supportd::mojom::WilcoDtcSupportdWebRequestStatus::
                kNetworkError);
  EXPECT_EQ(request_result->http_status, 0);
  EXPECT_EQ(request_result->response_body, "");
}

TEST_F(WilcoDtcSupportdWebRequestServiceTest, HttpUrlNetworkError) {
  std::unique_ptr<WebRequestResult> request_result;
  base::RunLoop run_loop;

  StartWebRequest(
      wilco_dtc_supportd::mojom::WilcoDtcSupportdWebRequestHttpMethod::kHead,
      kIncorrectHttpUrl, {} /* headers */, "" /* request_body */,
      &request_result, &run_loop);
  // The test fails with a network error on the same thread.
  ASSERT_TRUE(request_result);
  EXPECT_EQ(request_result->status,
            wilco_dtc_supportd::mojom::WilcoDtcSupportdWebRequestStatus::
                kNetworkError);
  EXPECT_EQ(request_result->http_status, 0);
  EXPECT_EQ(request_result->response_body, "");
}

TEST_F(WilcoDtcSupportdWebRequestServiceTest, InvalidUrlNetworkError) {
  std::unique_ptr<WebRequestResult> request_result;
  base::RunLoop run_loop;

  StartWebRequest(
      wilco_dtc_supportd::mojom::WilcoDtcSupportdWebRequestHttpMethod::kHead,
      kInvalidUrl, {} /* headers */, "" /* request_body */, &request_result,
      &run_loop);
  // The test fails with a network error on the same thread.
  ASSERT_TRUE(request_result);
  EXPECT_EQ(request_result->status,
            wilco_dtc_supportd::mojom::WilcoDtcSupportdWebRequestStatus::
                kNetworkError);
  EXPECT_EQ(request_result->http_status, 0);
  EXPECT_EQ(request_result->response_body, "");
}
}  // namespace chromeos
