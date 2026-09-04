// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/bound_session_credentials/bound_session_registration_fetcher_param.h"

#include <optional>
#include <string>
#include <vector>

#include "base/types/expected_macros.h"
#include "chrome/browser/signin/bound_session_credentials/bound_session_params_util.h"
#include "components/signin/public/base/session_binding_utils.h"
#include "net/http/structured_headers.h"
#include "url/gurl.h"

namespace {
constexpr char kRegistrationListHeaderName[] =
    "Sec-Session-Google-Registration-List";
constexpr char kChallengeItemKey[] = "challenge";
constexpr char kPathItemKey[] = "path";
}  // namespace

BoundSessionRegistrationFetcherParam::BoundSessionRegistrationFetcherParam(
    BoundSessionRegistrationFetcherParam&& other) noexcept = default;

BoundSessionRegistrationFetcherParam&
BoundSessionRegistrationFetcherParam::operator=(
    BoundSessionRegistrationFetcherParam&& other) noexcept = default;

BoundSessionRegistrationFetcherParam::~BoundSessionRegistrationFetcherParam() =
    default;

BoundSessionRegistrationFetcherParam::BoundSessionRegistrationFetcherParam(
    GURL registration_endpoint,
    std::vector<crypto::sign::SignatureKind> supported_algos,
    std::string challenge)
    : registration_endpoint_(std::move(registration_endpoint)),
      supported_algos_(std::move(supported_algos)),
      challenge_(std::move(challenge)) {}

// static
std::vector<BoundSessionRegistrationFetcherParam>
BoundSessionRegistrationFetcherParam::CreateFromHeaders(
    const GURL& request_url,
    const net::HttpResponseHeaders* headers) {
  if (!request_url.is_valid() || !headers) {
    return {};
  }

  const std::optional<std::string> list_header_value =
      headers->GetNormalizedHeader(kRegistrationListHeaderName);

  if (!list_header_value.has_value()) {
    return {};
  }

  return MaybeCreateFromListHeader(request_url, *list_header_value);
}

// static
BoundSessionRegistrationFetcherParam
BoundSessionRegistrationFetcherParam::CreateInstanceForTesting(
    GURL registration_endpoint,
    std::vector<crypto::sign::SignatureKind> supported_algos,
    std::string challenge) {
  return BoundSessionRegistrationFetcherParam(std::move(registration_endpoint),
                                              std::move(supported_algos),
                                              std::move(challenge));
}

// static
std::optional<BoundSessionRegistrationFetcherParam>
BoundSessionRegistrationFetcherParam::ParseListItem(
    const GURL& request_url,
    net::structured_headers::ParameterizedMember item) {
  ASSIGN_OR_RETURN((auto [items, params]), item.GetWithParamsIfInnerList());

  std::vector<crypto::sign::SignatureKind> supported_algos;
  for (const auto& algo_token : items) {
    const std::string* token = algo_token.item.GetIfToken();
    if (!token) {
      continue;
    }
    std::optional<crypto::sign::SignatureKind> algo =
        signin::SignatureAlgorithmFromString(*token);
    if (algo) {
      supported_algos.push_back(*algo);
    }
  }
  if (supported_algos.empty()) {
    return std::nullopt;
  }

  GURL registration_endpoint;
  std::string challenge;
  for (auto& [name, value] : params) {
    std::string* str = value.GetIfString();
    if (!str) {
      continue;
    }

    if (name == kPathItemKey) {
      registration_endpoint =
          bound_session_credentials::ResolveEndpointPath(request_url, *str);
    } else if (name == kChallengeItemKey) {
      challenge = std::move(*str);
    }
  }

  if (!registration_endpoint.is_valid() || challenge.empty()) {
    return std::nullopt;
  }

  return BoundSessionRegistrationFetcherParam(std::move(registration_endpoint),
                                              std::move(supported_algos),
                                              std::move(challenge));
}

// static
std::vector<BoundSessionRegistrationFetcherParam>
BoundSessionRegistrationFetcherParam::MaybeCreateFromListHeader(
    const GURL& request_url,
    std::string_view header_value) {
  std::optional<net::structured_headers::List> list =
      net::structured_headers::ParseList(header_value);
  if (!list || list->empty()) {
    return {};
  }

  std::vector<BoundSessionRegistrationFetcherParam> params;
  for (auto& item : *list) {
    std::optional<BoundSessionRegistrationFetcherParam> param =
        ParseListItem(request_url, std::move(item));
    if (param) {
      params.push_back(std::move(param).value());
    }
  }

  return params;
}
