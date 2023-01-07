// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/printing/oauth2/ipp_endpoint_token_fetcher.h"

#include <string>
#include <utility>

#include "base/containers/flat_set.h"
#include "base/functional/bind.h"
#include "chrome/browser/ash/printing/oauth2/http_exchange.h"
#include "chrome/browser/ash/printing/oauth2/status_code.h"
#include "chromeos/printing/uri.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "url/gurl.h"

namespace ash {
namespace printing {
namespace oauth2 {

IppEndpointTokenFetcher::IppEndpointTokenFetcher(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    const GURL& token_endpoint_uri,
    const chromeos::Uri& ipp_endpoint,
    base::flat_set<std::string>&& scope)
    : token_endpoint_uri_(token_endpoint_uri),
      ipp_endpoint_uri_(ipp_endpoint),
      scope_(scope),
      http_exchange_(url_loader_factory) {}

IppEndpointTokenFetcher::~IppEndpointTokenFetcher() = default;

void IppEndpointTokenFetcher::AddToWaitingList(StatusCallback callback) {
  callbacks_.push_back(std::move(callback));
}

std::vector<StatusCallback> IppEndpointTokenFetcher::TakeWaitingList() {
  std::vector<StatusCallback> waitlist;
  waitlist.swap(callbacks_);
  return waitlist;
}

void IppEndpointTokenFetcher::SendTokenExchangeRequest(
    const std::string& access_token,
    StatusCallback callback) {
  net::PartialNetworkTrafficAnnotationTag partial_traffic_annotation =
      net::DefinePartialNetworkTrafficAnnotation(
          "printing_oauth2_token_exchange_request",
          "printing_oauth2_http_exchange", R"(
    semantics {
      description:
        "This request asks Authorization Server for an endpoint access token "
        "to use for communication with a particular printer."
      data:
        "Address of the printer and the access token of the current OAuth 2 "
        "session with the Authorization Server."
    })");
  http_exchange_.Clear();

  // Move query parameters from URL to the content.
  chromeos::Uri uri(token_endpoint_uri_.spec());
  auto query = uri.GetQuery();
  for (const auto& kv : query) {
    http_exchange_.AddParamString(kv.first, kv.second);
  }
  uri.SetQuery({});

  // Prepare the request.
  http_exchange_.AddParamString(
      "grant_type", "urn:ietf:params:oauth:grant-type:token-exchange");
  http_exchange_.AddParamString("resource", ipp_endpoint_uri_.GetNormalized());
  http_exchange_.AddParamString("subject_token", access_token);
  http_exchange_.AddParamString(
      "subject_token_type", "urn:ietf:params:oauth:token-type:access_token");
  http_exchange_.Exchange(
      "POST", GURL(uri.GetNormalized()), ContentFormat::kXWwwFormUrlencoded,
      200, 400, partial_traffic_annotation,
      base::BindOnce(&IppEndpointTokenFetcher::OnTokenExchangeResponse,
                     base::Unretained(this), access_token,
                     std::move(callback)));
}

void IppEndpointTokenFetcher::OnTokenExchangeResponse(
    const std::string& access_token,
    StatusCallback callback,
    StatusCode status) {
  if (status == StatusCode::kInvalidAccessToken) {
    std::move(callback).Run(status, access_token);
    return;
  }

  if (status != StatusCode::kOK) {
    std::move(callback).Run(status, http_exchange_.GetErrorMessage());
    return;
  }

  const bool ok =
      http_exchange_.ParamStringGet("access_token", true,
                                    &endpoint_access_token_) &&
      http_exchange_.ParamStringGet("issued_token_type", true, nullptr) &&
      http_exchange_.ParamStringEquals("token_type", true, "bearer");
  if (!ok) {
    // Error occurred.
    endpoint_access_token_.clear();
    std::move(callback).Run(StatusCode::kInvalidResponse,
                            http_exchange_.GetErrorMessage());
    return;
  }

  // Success!
  std::move(callback).Run(StatusCode::kOK, endpoint_access_token_);
}

}  // namespace oauth2
}  // namespace printing
}  // namespace ash
