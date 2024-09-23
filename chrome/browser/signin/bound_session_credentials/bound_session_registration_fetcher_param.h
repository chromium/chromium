// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SIGNIN_BOUND_SESSION_CREDENTIALS_BOUND_SESSION_REGISTRATION_FETCHER_PARAM_H_
#define CHROME_BROWSER_SIGNIN_BOUND_SESSION_CREDENTIALS_BOUND_SESSION_REGISTRATION_FETCHER_PARAM_H_

#include <vector>

#include "base/containers/span.h"
#include "base/feature_list.h"
#include "crypto/signature_verifier.h"
#include "net/http/http_response_headers.h"
#include "net/http/structured_headers.h"
#include "url/gurl.h"

BASE_DECLARE_FEATURE(kBoundSessionRegistrationListHeaderSupport);

class BoundSessionRegistrationFetcherParam {
 public:
  BoundSessionRegistrationFetcherParam(
      BoundSessionRegistrationFetcherParam&& other) noexcept;
  BoundSessionRegistrationFetcherParam& operator=(
      BoundSessionRegistrationFetcherParam&& other) noexcept;

  BoundSessionRegistrationFetcherParam(
      const BoundSessionRegistrationFetcherParam& other) = delete;
  BoundSessionRegistrationFetcherParam& operator=(
      const BoundSessionRegistrationFetcherParam& other) = delete;
  ~BoundSessionRegistrationFetcherParam();

  // Returns a vector of valid instances. Return value is empty if `headers`
  // don't contain valid registration headers.
  static std::vector<BoundSessionRegistrationFetcherParam> CreateFromHeaders(
      const GURL& request_url,
      const net::HttpResponseHeaders* headers);

  // Convenience constructor for testing.
  static BoundSessionRegistrationFetcherParam CreateInstanceForTesting(
      GURL registration_endpoint,
      std::vector<crypto::SignatureVerifier::SignatureAlgorithm>
          supported_algos,
      std::string challenge);

  const GURL& registration_endpoint() const { return registration_endpoint_; }

  base::span<const crypto::SignatureVerifier::SignatureAlgorithm>
  supported_algos() const {
    return supported_algos_;
  }

  const std::string& challenge() const { return challenge_; }

 private:
  static std::optional<BoundSessionRegistrationFetcherParam> ParseListItem(
      const GURL& request_url,
      const net::structured_headers::ParameterizedMember& item);
  static std::vector<BoundSessionRegistrationFetcherParam>
  MaybeCreateFromListHeader(const GURL& request_url,
                            std::string_view header_value);
  static std::vector<BoundSessionRegistrationFetcherParam>
  MaybeCreateFromLegacyHeader(const GURL& request_url,
                              std::string_view header_value);

  BoundSessionRegistrationFetcherParam(
      GURL registration_endpoint,
      std::vector<crypto::SignatureVerifier::SignatureAlgorithm>
          supported_algos,
      std::string challenge);

  GURL registration_endpoint_;
  std::vector<crypto::SignatureVerifier::SignatureAlgorithm> supported_algos_;
  std::string challenge_;
};

#endif  // CHROME_BROWSER_SIGNIN_BOUND_SESSION_CREDENTIALS_BOUND_SESSION_REGISTRATION_FETCHER_PARAM_H_
