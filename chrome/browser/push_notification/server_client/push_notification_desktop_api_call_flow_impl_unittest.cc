// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/push_notification/server_client/push_notification_desktop_api_call_flow_impl.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/test/task_environment.h"
#include "net/base/net_errors.h"
#include "net/base/url_util.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "services/network/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const char kSerializedRequestProto[] = "serialized_request_proto";
const char kSerializedResponseProto[] = "result_proto";
const char kRequestUrl[] = "https://googleapis.com/push_notification/test";
const char kAccessToken[] = "access_token";
const char kQueryParameterAlternateOutputKey[] = "alt";
const char kQueryParameterAlternateOutputProto[] = "proto";
const char kGet[] = "GET";
const char kPost[] = "POST";
const char kPatch[] = "PATCH";

const push_notification::PushNotificationDesktopApiCallFlow::QueryParameters&
GetTestRequestProtoAsQueryParameters() {
  static const base::NoDestructor<
      push_notification::PushNotificationDesktopApiCallFlow::QueryParameters>
      request_as_query_parameters(
          {{"field1", "value1a"}, {"field1", "value1b"}, {"field2", "value2"}});
  return *request_as_query_parameters;
}

// Adds the "alt=proto" query parameters which specifies that the response
// should be formatted as a serialized proto. Adds the key-value pairs of
// 'request_as_query_parameters' as query parameters.
// 'request_as_query_parameters' is only non-null for GET requests.
GURL UrlWithQueryParameters(
    const std::string& url,
    const std::optional<
        push_notification::PushNotificationDesktopApiCallFlow::QueryParameters>&
        request_as_query_parameters) {
  GURL url_with_qp(url);

  url_with_qp =
      net::AppendQueryParameter(url_with_qp, kQueryParameterAlternateOutputKey,
                                kQueryParameterAlternateOutputProto);

  if (request_as_query_parameters) {
    for (const auto& key_value : *request_as_query_parameters) {
      url_with_qp = net::AppendQueryParameter(url_with_qp, key_value.first,
                                              key_value.second);
    }
  }

  return url_with_qp;
}

}  // namespace

namespace push_notification {

class PushNotificationDesktopApiCallFlowImplTest : public testing::Test {
 protected:
  PushNotificationDesktopApiCallFlowImplTest()
      : shared_factory_(
            base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
                &test_url_loader_factory_)) {
    flow_.SetPartialNetworkTrafficAnnotation(
        PARTIAL_TRAFFIC_ANNOTATION_FOR_TESTS);
  }

  void StartPostRequestApiCallFlow() {
    StartPostRequestApiCallFlowWithSerializedRequest(kSerializedRequestProto);
  }

  void StartPostRequestApiCallFlowWithSerializedRequest(
      const std::string& serialized_request) {
    flow_.StartPostRequest(
        GURL(kRequestUrl), serialized_request, shared_factory_, kAccessToken,
        base::BindOnce(&PushNotificationDesktopApiCallFlowImplTest::OnResult,
                       base::Unretained(this)),
        base::BindOnce(&PushNotificationDesktopApiCallFlowImplTest::OnError,
                       base::Unretained(this)));
    // A pending fetch for the API request should be created.
    CheckPushNotificationHttpPostRequest(serialized_request);
  }

  void StartPatchRequestApiCallFlow() {
    StartPatchRequestApiCallFlowWithSerializedRequest(kSerializedRequestProto);
  }

  void StartPatchRequestApiCallFlowWithSerializedRequest(
      const std::string& serialized_request) {
    flow_.StartPatchRequest(
        GURL(kRequestUrl), serialized_request, shared_factory_, kAccessToken,
        base::BindOnce(&PushNotificationDesktopApiCallFlowImplTest::OnResult,
                       base::Unretained(this)),
        base::BindOnce(&PushNotificationDesktopApiCallFlowImplTest::OnError,
                       base::Unretained(this)));
    // A pending fetch for the API request should be created.
    CheckPushNotificationHttpPatchRequest(serialized_request);
  }

  void StartGetRequestApiCallFlow() {
    StartGetRequestApiCallFlowWithRequestAsQueryParameters(
        GetTestRequestProtoAsQueryParameters());
  }

  void StartGetRequestApiCallFlowWithRequestAsQueryParameters(
      const PushNotificationDesktopApiCallFlow::QueryParameters&
          request_as_query_parameters) {
    flow_.StartGetRequest(
        GURL(kRequestUrl), request_as_query_parameters, shared_factory_,
        kAccessToken,
        base::BindOnce(&PushNotificationDesktopApiCallFlowImplTest::OnResult,
                       base::Unretained(this)),
        base::BindOnce(&PushNotificationDesktopApiCallFlowImplTest::OnError,
                       base::Unretained(this)));
    // A pending fetch for the API request should be created.
    CheckPushNotificationHttpGetRequest(request_as_query_parameters);
  }

  void OnResult(const std::string& result) {
    EXPECT_FALSE(result_ || network_error_);
    result_ = std::make_unique<std::string>(result);
  }

  void OnError(
      PushNotificationDesktopApiCallFlow::PushNotificationApiCallFlowError
          network_error) {
    EXPECT_FALSE(result_ || network_error_);
    network_error_ = std::make_unique<
        PushNotificationDesktopApiCallFlow::PushNotificationApiCallFlowError>(
        network_error);
  }

  void CheckPushNotificationHttpPostRequest(
      const std::string& serialized_request) {
    const std::vector<network::TestURLLoaderFactory::PendingRequest>& pending =
        *test_url_loader_factory_.pending_requests();
    ASSERT_EQ(1u, pending.size());
    const network::ResourceRequest& request = pending[0].request;

    EXPECT_EQ(UrlWithQueryParameters(
                  kRequestUrl, std::nullopt /* request_as_query_parameters */),
              request.url);

    EXPECT_EQ(kPost, request.method);

    EXPECT_EQ(serialized_request, network::GetUploadData(request));

    EXPECT_EQ("application/x-protobuf",
              request.headers.GetHeader(net::HttpRequestHeaders::kContentType));
  }

  void CheckPushNotificationHttpPatchRequest(
      const std::string& serialized_request) {
    const std::vector<network::TestURLLoaderFactory::PendingRequest>& pending =
        *test_url_loader_factory_.pending_requests();
    ASSERT_EQ(1u, pending.size());
    const network::ResourceRequest& request = pending[0].request;

    EXPECT_EQ(UrlWithQueryParameters(
                  kRequestUrl, std::nullopt /* request_as_query_parameters */),
              request.url);

    EXPECT_EQ(kPatch, request.method);

    EXPECT_EQ(serialized_request, network::GetUploadData(request));

    EXPECT_EQ("application/x-protobuf",
              request.headers.GetHeader(net::HttpRequestHeaders::kContentType));
  }

  void CheckPushNotificationHttpGetRequest(
      const PushNotificationDesktopApiCallFlow::QueryParameters&
          request_as_query_parameters) {
    const std::vector<network::TestURLLoaderFactory::PendingRequest>& pending =
        *test_url_loader_factory_.pending_requests();
    ASSERT_EQ(1u, pending.size());
    const network::ResourceRequest& request = pending[0].request;

    EXPECT_EQ(UrlWithQueryParameters(kRequestUrl, request_as_query_parameters),
              request.url);

    EXPECT_EQ(kGet, request.method);

    // Expect no body.
    EXPECT_TRUE(network::GetUploadData(request).empty());
    EXPECT_FALSE(
        request.headers.HasHeader(net::HttpRequestHeaders::kContentType));
  }

  // Responds to the current HTTP POST request. If the 'error' is not net::OK,
  // then the 'response_code' and 'response_string' are null.
  void CompleteCurrentPostRequest(
      net::Error error,
      std::optional<int> response_code = std::nullopt,
      const std::optional<std::string>& response_string = std::nullopt) {
    network::URLLoaderCompletionStatus completion_status(error);
    auto response_head = network::mojom::URLResponseHead::New();
    std::string content;
    if (error == net::OK) {
      response_head = network::CreateURLResponseHead(
          static_cast<net::HttpStatusCode>(*response_code));
      content = *response_string;
    }

    // Use kUrlMatchPrefix flag to match URL without query parameters.
    EXPECT_TRUE(test_url_loader_factory_.SimulateResponseForPendingRequest(
        GURL(kRequestUrl), completion_status, std::move(response_head), content,
        network::TestURLLoaderFactory::ResponseMatchFlags::kUrlMatchPrefix));

    EXPECT_TRUE(result_ || network_error_);
  }

  // Responds to the current HTTP PATCH request. If the 'error' is not net::OK,
  // then the 'response_code' and 'response_string' are null.
  void CompleteCurrentPatchRequest(
      net::Error error,
      std::optional<int> response_code = std::nullopt,
      const std::optional<std::string>& response_string = std::nullopt) {
    network::URLLoaderCompletionStatus completion_status(error);
    auto response_head = network::mojom::URLResponseHead::New();
    std::string content;
    if (error == net::OK) {
      response_head = network::CreateURLResponseHead(
          static_cast<net::HttpStatusCode>(*response_code));
      content = *response_string;
    }

    // Use kUrlMatchPrefix flag to match URL without query parameters.
    EXPECT_TRUE(test_url_loader_factory_.SimulateResponseForPendingRequest(
        GURL(kRequestUrl), completion_status, std::move(response_head), content,
        network::TestURLLoaderFactory::ResponseMatchFlags::kUrlMatchPrefix));

    EXPECT_TRUE(result_ || network_error_);
  }

  // Responds to the current HTTP GET request. If the 'error' is not net::OK,
  // then the 'response_code' and 'response_string' are null.
  void CompleteCurrentGetRequest(
      net::Error error,
      std::optional<int> response_code = std::nullopt,
      const std::optional<std::string>& response_string = std::nullopt) {
    network::URLLoaderCompletionStatus completion_status(error);
    auto response_head = network::mojom::URLResponseHead::New();
    std::string content;
    if (error == net::OK) {
      response_head = network::CreateURLResponseHead(
          static_cast<net::HttpStatusCode>(*response_code));
      content = *response_string;
    }

    // Use kUrlMatchPrefix flag to match URL without query parameters.
    EXPECT_TRUE(test_url_loader_factory_.SimulateResponseForPendingRequest(
        GURL(kRequestUrl), completion_status, std::move(response_head), content,
        network::TestURLLoaderFactory::ResponseMatchFlags::kUrlMatchPrefix));

    EXPECT_TRUE(result_ || network_error_);
  }

  std::unique_ptr<std::string> result_;
  std::unique_ptr<
      PushNotificationDesktopApiCallFlow::PushNotificationApiCallFlowError>
      network_error_;

 private:
  base::test::TaskEnvironment task_environment_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  scoped_refptr<network::SharedURLLoaderFactory> shared_factory_;

  PushNotificationDesktopApiCallFlowImpl flow_;
};

TEST_F(PushNotificationDesktopApiCallFlowImplTest, PostRequestSuccess) {
  StartPostRequestApiCallFlow();
  CompleteCurrentPostRequest(net::OK, net::HTTP_OK, kSerializedResponseProto);
  EXPECT_EQ(kSerializedResponseProto, *result_);
  EXPECT_FALSE(network_error_);
}

TEST_F(PushNotificationDesktopApiCallFlowImplTest, PatchRequestSuccess) {
  StartPatchRequestApiCallFlow();
  CompleteCurrentPatchRequest(net::OK, net::HTTP_OK, kSerializedResponseProto);
  EXPECT_EQ(kSerializedResponseProto, *result_);
  EXPECT_FALSE(network_error_);
}

TEST_F(PushNotificationDesktopApiCallFlowImplTest, GetRequestSuccess) {
  StartGetRequestApiCallFlow();
  CompleteCurrentGetRequest(net::OK, net::HTTP_OK, kSerializedResponseProto);
  EXPECT_EQ(kSerializedResponseProto, *result_);
  EXPECT_FALSE(network_error_);
}

TEST_F(PushNotificationDesktopApiCallFlowImplTest, PostRequestFailure) {
  StartPostRequestApiCallFlow();
  CompleteCurrentPostRequest(net::ERR_FAILED);
  EXPECT_FALSE(result_);
  EXPECT_EQ(PushNotificationDesktopApiCallFlow::
                PushNotificationApiCallFlowError::kOffline,
            *network_error_);
}

TEST_F(PushNotificationDesktopApiCallFlowImplTest, PatchRequestFailure) {
  StartPatchRequestApiCallFlow();
  CompleteCurrentPatchRequest(net::ERR_FAILED);
  EXPECT_FALSE(result_);
  EXPECT_EQ(PushNotificationDesktopApiCallFlow::
                PushNotificationApiCallFlowError::kOffline,
            *network_error_);
}

TEST_F(PushNotificationDesktopApiCallFlowImplTest, GetRequestFailure) {
  StartGetRequestApiCallFlow();
  CompleteCurrentPostRequest(net::ERR_FAILED);
  EXPECT_FALSE(result_);
  EXPECT_EQ(PushNotificationDesktopApiCallFlow::
                PushNotificationApiCallFlowError::kOffline,
            *network_error_);
}

TEST_F(PushNotificationDesktopApiCallFlowImplTest, RequestStatus500) {
  StartPostRequestApiCallFlow();
  CompleteCurrentPostRequest(net::OK, net::HTTP_INTERNAL_SERVER_ERROR,
                             "Chime Meltdown.");
  EXPECT_FALSE(result_);
  EXPECT_EQ(PushNotificationDesktopApiCallFlow::
                PushNotificationApiCallFlowError::kInternalServerError,
            *network_error_);
}

TEST_F(PushNotificationDesktopApiCallFlowImplTest, PatchRequestStatus500) {
  StartPatchRequestApiCallFlow();
  CompleteCurrentPatchRequest(net::OK, net::HTTP_INTERNAL_SERVER_ERROR,
                              "Chime Meltdown.");
  EXPECT_FALSE(result_);
  EXPECT_EQ(PushNotificationDesktopApiCallFlow::
                PushNotificationApiCallFlowError::kInternalServerError,
            *network_error_);
}

TEST_F(PushNotificationDesktopApiCallFlowImplTest, GetRequestStatus500) {
  StartGetRequestApiCallFlow();
  CompleteCurrentPostRequest(net::OK, net::HTTP_INTERNAL_SERVER_ERROR,
                             "Chime Meltdown.");
  EXPECT_FALSE(result_);
  EXPECT_EQ(PushNotificationDesktopApiCallFlow::
                PushNotificationApiCallFlowError::kInternalServerError,
            *network_error_);
}

// The empty string is a valid protocol buffer message serialization.
TEST_F(PushNotificationDesktopApiCallFlowImplTest, PostRequestWithNoBody) {
  StartPostRequestApiCallFlowWithSerializedRequest(std::string());
  CompleteCurrentPostRequest(net::OK, net::HTTP_OK, kSerializedResponseProto);
  EXPECT_EQ(kSerializedResponseProto, *result_);
  EXPECT_FALSE(network_error_);
}

// The empty string is a valid protocol buffer message serialization.
TEST_F(PushNotificationDesktopApiCallFlowImplTest, PatchRequestWithNoBody) {
  StartPatchRequestApiCallFlowWithSerializedRequest(std::string());
  CompleteCurrentPatchRequest(net::OK, net::HTTP_OK, kSerializedResponseProto);
  EXPECT_EQ(kSerializedResponseProto, *result_);
  EXPECT_FALSE(network_error_);
}

TEST_F(PushNotificationDesktopApiCallFlowImplTest,
       GetRequestWithNoQueryParameters) {
  StartGetRequestApiCallFlowWithRequestAsQueryParameters(
      {} /* request_as_query_parameters */);
  CompleteCurrentPostRequest(net::OK, net::HTTP_OK, kSerializedResponseProto);
  EXPECT_EQ(kSerializedResponseProto, *result_);
  EXPECT_FALSE(network_error_);
}

// The empty string is a valid protocol buffer message serialization.
TEST_F(PushNotificationDesktopApiCallFlowImplTest, PostResponseWithNoBody) {
  StartPostRequestApiCallFlow();
  CompleteCurrentPostRequest(net::OK, net::HTTP_OK, std::string());
  EXPECT_EQ(std::string(), *result_);
  EXPECT_FALSE(network_error_);
}

// The empty string is a valid protocol buffer message serialization.
TEST_F(PushNotificationDesktopApiCallFlowImplTest, PatchResponseWithNoBody) {
  StartPatchRequestApiCallFlow();
  CompleteCurrentPatchRequest(net::OK, net::HTTP_OK, std::string());
  EXPECT_EQ(std::string(), *result_);
  EXPECT_FALSE(network_error_);
}

// The empty string is a valid protocol buffer message serialization.
TEST_F(PushNotificationDesktopApiCallFlowImplTest, GetResponseWithNoBody) {
  StartGetRequestApiCallFlow();
  CompleteCurrentPostRequest(net::OK, net::HTTP_OK, std::string());
  EXPECT_EQ(std::string(), *result_);
  EXPECT_FALSE(network_error_);
}

}  // namespace push_notification
