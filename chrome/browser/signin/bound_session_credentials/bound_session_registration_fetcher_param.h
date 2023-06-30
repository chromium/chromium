// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SIGNIN_BOUND_SESSION_CREDENTIALS_BOUND_SESSION_REGISTRATION_FETCHER_PARAM_H_
#define CHROME_BROWSER_SIGNIN_BOUND_SESSION_CREDENTIALS_BOUND_SESSION_REGISTRATION_FETCHER_PARAM_H_

#include <vector>

#include "base/containers/span.h"
#include "crypto/signature_verifier.h"
#include "net/http/http_response_headers.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"

class BoundSessionRegistrationFetcherParam {
 public:
  BoundSessionRegistrationFetcherParam(
      BoundSessionRegistrationFetcherParam&& other);
  BoundSessionRegistrationFetcherParam& operator=(
      BoundSessionRegistrationFetcherParam&& other) noexcept;

  BoundSessionRegistrationFetcherParam(
      const BoundSessionRegistrationFetcherParam& other) = delete;
  BoundSessionRegistrationFetcherParam& operator=(
      const BoundSessionRegistrationFetcherParam&) = delete;
  ~BoundSessionRegistrationFetcherParam();

  // Will return a valid instance or return absl::nullopt;
  static absl::optional<BoundSessionRegistrationFetcherParam>
  MaybeCreateInstance(const GURL& request_url,
                      const net::HttpResponseHeaders* headers);

  // Convenience constructor for testing.
  static BoundSessionRegistrationFetcherParam CreateInstanceForTesting(
      GURL registration_endpoint,
      std::vector<crypto::SignatureVerifier::SignatureAlgorithm>
          supported_algos);

  const GURL& RegistrationEndpoint() const;
  base::span<const crypto::SignatureVerifier::SignatureAlgorithm>
  SupportedAlgos() const;

 private:
  BoundSessionRegistrationFetcherParam(
      GURL registration_endpoint,
      std::vector<crypto::SignatureVerifier::SignatureAlgorithm>
          supported_algos);

  GURL registration_endpoint_;
  std::vector<crypto::SignatureVerifier::SignatureAlgorithm> supported_algos_;
};

#endif  // CHROME_BROWSER_SIGNIN_BOUND_SESSION_CREDENTIALS_BOUND_SESSION_REGISTRATION_FETCHER_PARAM_H_
