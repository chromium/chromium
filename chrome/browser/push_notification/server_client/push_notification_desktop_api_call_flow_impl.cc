// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/push_notification/server_client/push_notification_desktop_api_call_flow_impl.h"

#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "net/base/net_errors.h"
#include "net/base/url_util.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/mojom/url_response_head.mojom.h"

namespace {

const char kGet[] = "GET";
const char kPatch[] = "PATCH";
const char kPost[] = "POST";
const char kProtobufContentType[] = "application/x-protobuf";
const char kQueryParameterAlternateOutputKey[] = "alt";
const char kQueryParameterAlternateOutputProto[] = "proto";

}  // namespace

namespace push_notification {

PushNotificationDesktopApiCallFlowImpl::
    PushNotificationDesktopApiCallFlowImpl() = default;
PushNotificationDesktopApiCallFlowImpl::
    ~PushNotificationDesktopApiCallFlowImpl() = default;

void PushNotificationDesktopApiCallFlowImpl::StartPostRequest(
    const GURL& request_url,
    const std::string& serialized_request,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    const std::string& access_token,
    ResultCallback&& result_callback,
    ErrorCallback&& error_callback) {
  request_url_ = request_url;
  request_http_method_ = kPost;
  serialized_request_ = serialized_request;
  result_callback_ = std::move(result_callback);
  error_callback_ = std::move(error_callback);
  OAuth2ApiCallFlow::Start(std::move(url_loader_factory), access_token);
}

void PushNotificationDesktopApiCallFlowImpl::StartPatchRequest(
    const GURL& request_url,
    const std::string& serialized_request,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    const std::string& access_token,
    ResultCallback&& result_callback,
    ErrorCallback&& error_callback) {
  request_url_ = request_url;
  request_http_method_ = kPatch;
  serialized_request_ = serialized_request;
  result_callback_ = std::move(result_callback);
  error_callback_ = std::move(error_callback);
  OAuth2ApiCallFlow::Start(std::move(url_loader_factory), access_token);
}

void PushNotificationDesktopApiCallFlowImpl::StartGetRequest(
    const GURL& request_url,
    const QueryParameters& request_as_query_parameters,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    const std::string& access_token,
    ResultCallback&& result_callback,
    ErrorCallback&& error_callback) {
  request_url_ = request_url;
  request_http_method_ = kGet;
  request_as_query_parameters_ = request_as_query_parameters;
  result_callback_ = std::move(result_callback);
  error_callback_ = std::move(error_callback);
  OAuth2ApiCallFlow::Start(std::move(url_loader_factory), access_token);
}

void PushNotificationDesktopApiCallFlowImpl::SetPartialNetworkTrafficAnnotation(
    const net::PartialNetworkTrafficAnnotationTag& partial_traffic_annotation) {
  partial_network_annotation_ =
      std::make_unique<net::PartialNetworkTrafficAnnotationTag>(
          partial_traffic_annotation);
}

GURL PushNotificationDesktopApiCallFlowImpl::CreateApiCallUrl() {
  // Specifies that the server's response body should be formatted as a
  // serialized proto.
  request_url_ =
      net::AppendQueryParameter(request_url_, kQueryParameterAlternateOutputKey,
                                kQueryParameterAlternateOutputProto);

  // GET requests encode the request proto as query parameters.
  if (request_as_query_parameters_) {
    for (const auto& key_value_pair : *request_as_query_parameters_) {
      request_url_ = net::AppendQueryParameter(
          request_url_, key_value_pair.first, key_value_pair.second);
    }
  }

  return request_url_;
}

std::string PushNotificationDesktopApiCallFlowImpl::CreateApiCallBody() {
  return serialized_request_.value_or(std::string());
}

std::string
PushNotificationDesktopApiCallFlowImpl::CreateApiCallBodyContentType() {
  return serialized_request_ ? kProtobufContentType : std::string();
}

// Note: Unlike OAuth2ApiCallFlow, we do *not* determine the request type
// based on whether or not the body is empty.
std::string PushNotificationDesktopApiCallFlowImpl::GetRequestTypeForBody(
    const std::string& body) {
  CHECK(!request_http_method_.empty());
  return request_http_method_;
}

void PushNotificationDesktopApiCallFlowImpl::ProcessApiCallSuccess(
    const network::mojom::URLResponseHead* head,
    std::unique_ptr<std::string> body) {
  if (!body) {
    CHECK(error_callback_);
    std::move(error_callback_)
        .Run(PushNotificationApiCallFlowError::kResponseMalformed);
    return;
  }
  CHECK(result_callback_);
  std::move(result_callback_).Run(std::move(*body));
}

void PushNotificationDesktopApiCallFlowImpl::ProcessApiCallFailure(
    int net_error,
    const network::mojom::URLResponseHead* head,
    std::unique_ptr<std::string> body) {
  std::optional<PushNotificationApiCallFlowError> error;
  std::string error_message;
  if (net_error == net::OK) {
    int response_code = -1;
    if (head && head->headers) {
      response_code = head->headers->response_code();
    }
    error = GetErrorForHttpResponseCode(response_code);
  } else {
    error = PushNotificationApiCallFlowError::kOffline;
  }

  LOG(ERROR) << "API call failed, error code: "
             << net::ErrorToString(net_error);
  if (body) {
    VLOG(1) << "API failure response body: " << *body;
  }

  std::move(error_callback_).Run(*error);
}
net::PartialNetworkTrafficAnnotationTag
PushNotificationDesktopApiCallFlowImpl::GetNetworkTrafficAnnotationTag() {
  CHECK(partial_network_annotation_);
  return *partial_network_annotation_;
}

}  // namespace push_notification
