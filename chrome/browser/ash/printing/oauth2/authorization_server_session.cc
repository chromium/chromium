// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/printing/oauth2/authorization_server_session.h"

#include <algorithm>
#include <string>
#include <vector>

#include "base/containers/flat_set.h"
#include "base/functional/bind.h"
#include "base/strings/string_split.h"
#include "chrome/browser/ash/printing/oauth2/constants.h"
#include "chrome/browser/ash/printing/oauth2/http_exchange.h"
#include "chromeos/printing/uri.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "url/gurl.h"

namespace ash {
namespace printing {
namespace oauth2 {

base::flat_set<std::string> ParseScope(const std::string& scope) {
  std::vector<std::string> tokens = base::SplitString(
      scope, " ", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  base::flat_set<std::string> output(std::move(tokens));
  return output;
}

AuthorizationServerSession::AuthorizationServerSession(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    const GURL& token_endpoint_uri,
    base::flat_set<std::string>&& scope)
    : token_endpoint_uri_(token_endpoint_uri),
      scope_(scope),
      http_exchange_(url_loader_factory) {}

AuthorizationServerSession::~AuthorizationServerSession() = default;

bool AuthorizationServerSession::ContainsAll(
    const base::flat_set<std::string>& scope) const {
  return std::includes(scope_.begin(), scope_.end(), scope.begin(),
                       scope.end());
}

void AuthorizationServerSession::AddToWaitingList(StatusCallback callback) {
  callbacks_.push_back(std::move(callback));
}

std::vector<StatusCallback> AuthorizationServerSession::TakeWaitingList() {
  std::vector<StatusCallback> waitlist;
  waitlist.swap(callbacks_);
  return waitlist;
}

void AuthorizationServerSession::SendFirstTokenRequest(
    const std::string& client_id,
    const std::string& authorization_code,
    const std::string& code_verifier,
    StatusCallback callback) {
  net::PartialNetworkTrafficAnnotationTag partial_traffic_annotation =
      net::DefinePartialNetworkTrafficAnnotation(
          "printing_oauth2_first_token_request",
          "printing_oauth2_http_exchange", R"(semantics {
    description:
      "This request opens OAuth 2 session with the Authorization Server by "
      "asking it for an access token."
    data:
      "Identifier of the client obtained from the Authorization server during "
      "registration and temporary security codes used during authorization "
      "process."
    })");
  http_exchange_.Clear();
  // Moves query parameters from URL to the content.
  chromeos::Uri uri(token_endpoint_uri_.spec());
  auto query = uri.GetQuery();
  for (const auto& kv : query) {
    http_exchange_.AddParamString(kv.first, kv.second);
  }
  uri.SetQuery({});
  // Prepare the request.
  http_exchange_.AddParamString("grant_type", "authorization_code");
  http_exchange_.AddParamString("code", authorization_code);
  http_exchange_.AddParamString("redirect_uri", kRedirectURI);
  http_exchange_.AddParamString("client_id", client_id);
  http_exchange_.AddParamString("code_verifier", code_verifier);
  http_exchange_.Exchange(
      "POST", GURL(uri.GetNormalized()), ContentFormat::kXWwwFormUrlencoded,
      200, 400, partial_traffic_annotation,
      base::BindOnce(&AuthorizationServerSession::OnFirstTokenResponse,
                     base::Unretained(this), std::move(callback)));
}

void AuthorizationServerSession::SendNextTokenRequest(StatusCallback callback) {
  access_token_.clear();
  if (refresh_token_.empty()) {
    std::move(callback).Run(StatusCode::kAuthorizationNeeded,
                            "No refresh token");
    return;
  }
  net::PartialNetworkTrafficAnnotationTag partial_traffic_annotation =
      net::DefinePartialNetworkTrafficAnnotation(
          "printing_oauth2_next_token_request", "printing_oauth2_http_exchange",
          R"(semantics {
    description:
      "This request refreshes OAuth 2 session with the Authorization Server by "
      "asking it for a new access token."
    data:
      "A refresh token previously issued by the Authorization Server."
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
  http_exchange_.AddParamString("grant_type", "refresh_token");
  http_exchange_.AddParamString("refresh_token", refresh_token_);
  http_exchange_.Exchange(
      "POST", GURL(uri.GetNormalized()), ContentFormat::kXWwwFormUrlencoded,
      200, 400, partial_traffic_annotation,
      base::BindOnce(&AuthorizationServerSession::OnNextTokenResponse,
                     base::Unretained(this), std::move(callback)));
}

void AuthorizationServerSession::OnFirstTokenResponse(StatusCallback callback,
                                                      StatusCode status) {
  if (status != StatusCode::kOK) {
    std::move(callback).Run(status, http_exchange_.GetErrorMessage());
    return;
  }

  // Parses response.
  std::string scope;
  const bool ok =
      http_exchange_.ParamStringGet("access_token", true, &access_token_) &&
      http_exchange_.ParamStringEquals("token_type", true, "bearer") &&
      http_exchange_.ParamStringGet("refresh_token", false, &refresh_token_) &&
      http_exchange_.ParamStringGet("scope", false, &scope);
  if (!ok) {
    // Error occurred.
    access_token_.clear();
    refresh_token_.clear();
    std::move(callback).Run(StatusCode::kInvalidResponse,
                            http_exchange_.GetErrorMessage());
    return;
  }

  // Success!
  auto new_scope = ParseScope(scope);
  scope_.insert(new_scope.begin(), new_scope.end());
  std::move(callback).Run(StatusCode::kOK, access_token_);
}

void AuthorizationServerSession::OnNextTokenResponse(StatusCallback callback,
                                                     StatusCode status) {
  if (status == StatusCode::kInvalidAccessToken) {
    std::move(callback).Run(StatusCode::kAuthorizationNeeded,
                            "Refresh token expired");
    return;
  }

  if (status != StatusCode::kOK) {
    std::move(callback).Run(status, http_exchange_.GetErrorMessage());
    return;
  }

  // Parses response.
  const bool ok =
      http_exchange_.ParamStringGet("access_token", true, &access_token_) &&
      http_exchange_.ParamStringEquals("token_type", true, "bearer") &&
      http_exchange_.ParamStringGet("refresh_token", false, &refresh_token_);
  if (!ok) {
    // Error occurred.
    access_token_.clear();
    refresh_token_.clear();
    std::move(callback).Run(StatusCode::kInvalidResponse,
                            http_exchange_.GetErrorMessage());
    return;
  }

  // Success!
  std::move(callback).Run(StatusCode::kOK, access_token_);
}

}  // namespace oauth2
}  // namespace printing
}  // namespace ash
