// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NEARBY_SHARING_COMMON_NEARBY_SHARE_HTTP_RESULT_H_
#define CHROME_BROWSER_NEARBY_SHARING_COMMON_NEARBY_SHARE_HTTP_RESULT_H_

#include <ostream>
#include <string>

#include "services/network/public/mojom/url_response_head.mojom-forward.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

enum class NearbyShareHttpError {
  // Request could not be completed because the device is offline or has issues
  // sending the HTTP request.
  kOffline,

  // Server endpoint could not be found.
  kEndpointNotFound,

  // Authentication error contacting back-end.
  kAuthenticationError,

  // Request was invalid.
  kBadRequest,

  // The server responded, but the response was not formatted correctly.
  kResponseMalformed,

  // Internal server error.
  kInternalServerError,

  // Unknown result.
  kUnknown
};

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class NearbyShareHttpResult {
  kSuccess = 0,
  kTimeout = 1,
  kHttpErrorOffline = 2,
  kHttpErrorEndpointNotFound = 3,
  kHttpErrorAuthenticationError = 4,
  kHttpErrorBadRequest = 5,
  kHttpErrorResponseMalformed = 6,
  kHttpErrorInternalServerError = 7,
  kHttpErrorUnknown = 8,
  kMaxValue = kHttpErrorUnknown
};

class NearbyShareHttpStatus {
 public:
  NearbyShareHttpStatus(const int net_error,
                        const network::mojom::URLResponseHead* head);
  NearbyShareHttpStatus(const NearbyShareHttpStatus& status);
  ~NearbyShareHttpStatus();

  bool IsSuccess() const;
  int GetResultCodeForMetrics() const;
  std::string ToString() const;

 private:
  enum class Status { kSuccess, kNetworkFailure, kHttpFailure } status_;
  int net_error_code_;
  absl::optional<int> http_response_code_;
};

NearbyShareHttpError NearbyShareHttpErrorForHttpResponseCode(int response_code);
NearbyShareHttpResult NearbyShareHttpErrorToResult(NearbyShareHttpError error);

std::ostream& operator<<(std::ostream& stream,
                         const NearbyShareHttpResult& result);
std::ostream& operator<<(std::ostream& stream,
                         const NearbyShareHttpError& error);
std::ostream& operator<<(std::ostream& stream,
                         const NearbyShareHttpStatus& status);

#endif  // CHROME_BROWSER_NEARBY_SHARING_COMMON_NEARBY_SHARE_HTTP_RESULT_H_
