// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nearby_sharing/common/nearby_share_http_result.h"

#include "net/base/net_errors.h"
#include "net/http/http_status_code.h"
#include "services/network/public/cpp/header_util.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/url_response_head.mojom.h"

NearbyShareHttpError NearbyShareHttpErrorForHttpResponseCode(
    int response_code) {
  if (response_code == 400)
    return NearbyShareHttpError::kBadRequest;

  if (response_code == 403)
    return NearbyShareHttpError::kAuthenticationError;

  if (response_code == 404)
    return NearbyShareHttpError::kEndpointNotFound;

  if (response_code >= 500 && response_code < 600)
    return NearbyShareHttpError::kInternalServerError;

  return NearbyShareHttpError::kUnknown;
}

NearbyShareHttpResult NearbyShareHttpErrorToResult(NearbyShareHttpError error) {
  switch (error) {
    case NearbyShareHttpError::kOffline:
      return NearbyShareHttpResult::kHttpErrorOffline;
    case NearbyShareHttpError::kEndpointNotFound:
      return NearbyShareHttpResult::kHttpErrorEndpointNotFound;
    case NearbyShareHttpError::kAuthenticationError:
      return NearbyShareHttpResult::kHttpErrorAuthenticationError;
    case NearbyShareHttpError::kBadRequest:
      return NearbyShareHttpResult::kHttpErrorBadRequest;
    case NearbyShareHttpError::kResponseMalformed:
      return NearbyShareHttpResult::kHttpErrorResponseMalformed;
    case NearbyShareHttpError::kInternalServerError:
      return NearbyShareHttpResult::kHttpErrorInternalServerError;
    case NearbyShareHttpError::kUnknown:
      return NearbyShareHttpResult::kHttpErrorUnknown;
  }
}

NearbyShareHttpStatus::NearbyShareHttpStatus(
    const int net_error,
    const network::mojom::URLResponseHead* head)
    : net_error_code_(net_error) {
  if (head && head->headers)
    http_response_code_ = head->headers->response_code();

  bool net_success = (net_error_code_ == net::OK ||
                      net_error_code_ == net::ERR_HTTP_RESPONSE_CODE_FAILURE) &&
                     http_response_code_;
  bool http_success =
      net_success && network::IsSuccessfulStatus(*http_response_code_);

  if (http_success) {
    status_ = Status::kSuccess;
  } else if (net_success) {
    status_ = Status::kHttpFailure;
  } else {
    status_ = Status::kNetworkFailure;
  }
}

NearbyShareHttpStatus::NearbyShareHttpStatus(
    const NearbyShareHttpStatus& status) = default;

NearbyShareHttpStatus::~NearbyShareHttpStatus() = default;

bool NearbyShareHttpStatus::IsSuccess() const {
  return status_ == Status::kSuccess;
}

int NearbyShareHttpStatus::GetResultCodeForMetrics() const {
  switch (status_) {
    case Status::kNetworkFailure:
      return net_error_code_;
    case Status::kSuccess:
    case Status::kHttpFailure:
      return *http_response_code_;
  }
}

std::string NearbyShareHttpStatus::ToString() const {
  std::string status;
  switch (status_) {
    case Status::kSuccess:
      status = "kSuccess";
      break;
    case Status::kNetworkFailure:
      status = "kNetworkFailure";
      break;
    case Status::kHttpFailure:
      status = "kHttpFailure";
      break;
  }
  std::string net_code = net::ErrorToString(net_error_code_);
  std::string response_code =
      http_response_code_.has_value()
          ? net::GetHttpReasonPhrase(
                static_cast<net::HttpStatusCode>(*http_response_code_))
          : "[null]";

  return "status=" + status + ", net_code=" + net_code +
         ", response_code=" + response_code;
}

std::ostream& operator<<(std::ostream& stream,
                         const NearbyShareHttpResult& result) {
  switch (result) {
    case NearbyShareHttpResult::kSuccess:
      stream << "[Success]";
      break;
    case NearbyShareHttpResult::kTimeout:
      stream << "[Timeout]";
      break;
    case NearbyShareHttpResult::kHttpErrorOffline:
      stream << "[HTTP Error: Offline]";
      break;
    case NearbyShareHttpResult::kHttpErrorEndpointNotFound:
      stream << "[HTTP Error: Endpoint not found]";
      break;
    case NearbyShareHttpResult::kHttpErrorAuthenticationError:
      stream << "[HTTP Error: Authentication error]";
      break;
    case NearbyShareHttpResult::kHttpErrorBadRequest:
      stream << "[HTTP Error: Bad request]";
      break;
    case NearbyShareHttpResult::kHttpErrorResponseMalformed:
      stream << "[HTTP Error: Response malformed]";
      break;
    case NearbyShareHttpResult::kHttpErrorInternalServerError:
      stream << "[HTTP Error: Internal server error]";
      break;
    case NearbyShareHttpResult::kHttpErrorUnknown:
      stream << "[HTTP Error: Unknown]";
      break;
  }
  return stream;
}

std::ostream& operator<<(std::ostream& stream,
                         const NearbyShareHttpError& error) {
  stream << NearbyShareHttpErrorToResult(error);
  return stream;
}

std::ostream& operator<<(std::ostream& stream,
                         const NearbyShareHttpStatus& status) {
  stream << status.ToString();
  return stream;
}
