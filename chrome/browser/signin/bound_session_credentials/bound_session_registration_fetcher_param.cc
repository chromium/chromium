// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/bound_session_credentials/bound_session_registration_fetcher_param.h"

#include "base/strings/escape.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "net/base/schemeful_site.h"

namespace {
constexpr char kAlgoItemKey[] = "supported-alg";
constexpr char kRegistrationHeaderName[] = "Sec-Session-Google-Registration";
constexpr char kRegistrationItemKey[] = "registration";

absl::optional<crypto::SignatureVerifier::SignatureAlgorithm> AlgoFromString(
    const std::string algo) {
  if (base::EqualsCaseInsensitiveASCII(algo, "ES256")) {
    return crypto::SignatureVerifier::SignatureAlgorithm::ECDSA_SHA256;
  }

  if (base::EqualsCaseInsensitiveASCII(algo, "RS256")) {
    return crypto::SignatureVerifier::SignatureAlgorithm::RSA_PKCS1_SHA256;
  }

  return absl::nullopt;
}
}  // namespace

BoundSessionRegistrationFetcherParam::BoundSessionRegistrationFetcherParam(
    BoundSessionRegistrationFetcherParam&& other) = default;

BoundSessionRegistrationFetcherParam&
BoundSessionRegistrationFetcherParam::operator=(
    BoundSessionRegistrationFetcherParam&& other) noexcept = default;

BoundSessionRegistrationFetcherParam::~BoundSessionRegistrationFetcherParam() =
    default;

BoundSessionRegistrationFetcherParam::BoundSessionRegistrationFetcherParam(
    GURL registration_endpoint,
    std::vector<crypto::SignatureVerifier::SignatureAlgorithm> supported_algos)
    : registration_endpoint_(std::move(registration_endpoint)),
      supported_algos_(std::move(supported_algos)) {}

absl::optional<BoundSessionRegistrationFetcherParam>
BoundSessionRegistrationFetcherParam::MaybeCreateInstance(
    const GURL& request_url,
    const net::HttpResponseHeaders* headers) {
  if (!request_url.is_valid()) {
    return absl::nullopt;
  }

  std::string header_value;
  if (!headers->GetNormalizedHeader(kRegistrationHeaderName, &header_value)) {
    return absl::nullopt;
  }

  GURL registration_endpoint;
  std::vector<crypto::SignatureVerifier::SignatureAlgorithm> supported_algos;
  base::StringPairs items;
  base::SplitStringIntoKeyValuePairs(header_value, '=', ';', &items);
  for (const auto& [key, value] : items) {
    if (base::EqualsCaseInsensitiveASCII(key, kRegistrationItemKey)) {
      std::string unescaped = base::UnescapeURLComponent(
          value,
          base::UnescapeRule::PATH_SEPARATORS |
              base::UnescapeRule::URL_SPECIAL_CHARS_EXCEPT_PATH_SEPARATORS);
      GURL potential_registration_endpoint =
          request_url.GetWithoutFilename().Resolve(unescaped);
      if (net::SchemefulSite(potential_registration_endpoint) ==
          net::SchemefulSite(request_url)) {
        registration_endpoint = potential_registration_endpoint;
      }
    }

    if (base::EqualsCaseInsensitiveASCII(key, kAlgoItemKey)) {
      auto list = base::SplitString(value, ",",
                                    base::WhitespaceHandling::TRIM_WHITESPACE,
                                    base::SplitResult::SPLIT_WANT_NONEMPTY);
      for (const auto& alg_string : list) {
        absl::optional<crypto::SignatureVerifier::SignatureAlgorithm> alg =
            AlgoFromString(alg_string);
        if (alg.has_value()) {
          supported_algos.push_back(alg.value());
        }
      }
    }
  }

  if (registration_endpoint.is_valid() && !supported_algos.empty()) {
    return BoundSessionRegistrationFetcherParam(
        std::move(registration_endpoint), std::move(supported_algos));
  } else {
    return absl::nullopt;
  }
}

BoundSessionRegistrationFetcherParam
BoundSessionRegistrationFetcherParam::CreateInstanceForTesting(
    GURL registration_endpoint,
    std::vector<crypto::SignatureVerifier::SignatureAlgorithm>
        supported_algos) {
  return BoundSessionRegistrationFetcherParam(std::move(registration_endpoint),
                                              std::move(supported_algos));
}

const GURL& BoundSessionRegistrationFetcherParam::RegistrationEndpoint() const {
  return registration_endpoint_;
}

base::span<const crypto::SignatureVerifier::SignatureAlgorithm>
BoundSessionRegistrationFetcherParam::SupportedAlgos() const {
  return supported_algos_;
}
