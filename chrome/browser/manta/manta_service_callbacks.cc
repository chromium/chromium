// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/manta/manta_service_callbacks.h"

#include <memory>

#include "net/http/http_status_code.h"

namespace manta {

void OnEndpointFetcherComplete(MantaProtoResponseCallback callback,
                               std::unique_ptr<EndpointFetcher> fetcher,
                               std::unique_ptr<EndpointResponse> responses) {
  // TODO(b/301185733): Log error code to UMA.
  // Tries to parse the response as a Response proto and return to the
  // `callback` together with a OK status, or capture the errors and return a
  // proper error status.

  // TODO(b/288019728): responses->response could contain detailed error message
  // that should be mapped to a specific error code.
  if (!responses || responses->error_type.has_value() ||
      responses->http_status_code != net::HTTP_OK) {
    std::move(callback).Run(
        nullptr, {MantaStatusCode::kBackendFailure, responses->response});
    return;
  }

  auto manta_response = std::make_unique<proto::Response>();
  if (!manta_response->ParseFromString(responses->response)) {
    std::move(callback).Run(
        nullptr, {MantaStatusCode::kMalformedResponse, std::string()});
    return;
  }

  std::move(callback).Run(std::move(manta_response),
                          {MantaStatusCode::kOk, std::string()});
}

}  // namespace manta
