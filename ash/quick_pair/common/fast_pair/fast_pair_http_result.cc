// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/quick_pair/common/fast_pair/fast_pair_http_result.h"

#include "net/base/net_errors.h"
#include "net/http/http_status_code.h"
#include "services/network/public/cpp/header_util.h"
#include "services/network/public/mojom/url_response_head.mojom.h"

namespace ash {
namespace quick_pair {

FastPairHttpResult::FastPairHttpResult(
    const int net_error,
    const network::mojom::URLResponseHead* head) {
  std::optional<int> http_response_code;
  if (head && head->headers)
    http_response_code = head->headers->response_code();
  bool net_success = (net_error == net::OK ||
                      net_error == net::ERR_HTTP_RESPONSE_CODE_FAILURE) &&
                     http_response_code;
  bool http_success =
      net_success && network::IsSuccessfulStatus(*http_response_code);
  if (http_success) {
    type_ = Type::kSuccess;
  } else if (net_success) {
    type_ = Type::kHttpFailure;
    http_response_error_ = *http_response_code;
  } else {
    type_ = Type::kNetworkFailure;
    net_error_ = net_error;
  }
}

FastPairHttpResult::~FastPairHttpResult() = default;

bool FastPairHttpResult::IsSuccess() const {
  return type_ == Type::kSuccess;
}

std::string FastPairHttpResult::ToString() const {
  std::string status;
  switch (type_) {
    case Type::kSuccess:
      status = "kSuccess";
      break;
    case Type::kNetworkFailure:
      status = "kNetworkFailure";
      break;
    case Type::kHttpFailure:
      status = "kHttpFailure";
      break;
  }

  std::string net_error = net_error_.has_value()
                              ? net::ErrorToString(net_error_.value())
                              : "[null]";

  std::string response_error =
      http_response_error_.has_value()
          ? net::GetHttpReasonPhrase(
                static_cast<net::HttpStatusCode>(*http_response_error_))
          : "[null]";

  return "status=" + status + ", net_error=" + net_error +
         ", http_response_error=" + response_error;
}

}  // namespace quick_pair
}  // namespace ash
