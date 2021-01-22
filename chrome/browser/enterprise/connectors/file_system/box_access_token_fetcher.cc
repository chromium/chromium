// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/file_system/box_access_token_fetcher.h"

#include "chrome/browser/enterprise/connectors/connectors_prefs.h"
#include "chrome/browser/enterprise/connectors/file_system/box_api_call_endpoints.h"
#include "google_apis/gaia/google_service_auth_error.h"

namespace enterprise_connectors {

BoxAccessTokenFetcher::BoxAccessTokenFetcher(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    const std::string& refresh_token,
    const std::string& auth_code,
    TokenCallback callback)
    : OAuth2AccessTokenFetcherImpl(this,
                                   url_loader_factory,
                                   refresh_token,
                                   auth_code),
      callback_(std::move(callback)) {}

BoxAccessTokenFetcher::~BoxAccessTokenFetcher() = default;

GURL BoxAccessTokenFetcher::GetAccessTokenURL() const {
  return GURL(kFileSystemBoxEndpointOAuth2Token);
}

net::NetworkTrafficAnnotationTag
BoxAccessTokenFetcher::GetTrafficAnnotationTag() const {
  return net::DefineNetworkTrafficAnnotation("box_access_token_fetcher",
                                             R"(
      semantics {
        sender: "OAuth 2.0 Access Token Fetcher"
        description:
          "This request is used by the Box integration to fetch an OAuth 2.0 "
          "access token for a known Box account."
        trigger:
          "This request can be triggered when the user uploads or downloads "
          "a file."
        data:
          "Chrome OAuth 2.0 client id and secret, the set of OAuth 2.0 "
          "scopes and the OAuth 2.0 refresh token."
        destination: OTHER
      }
      policy {
        cookies_allowed: NO
        setting:
          "This feature cannot be disabled in settings. It is disabled by "
          "default, unless the administrator enables it via policy."
        policy_exception_justification: "Not implemented yet."
      })");
  // TODO(https://crbug.com/1167934): Add enterprise policy.
  // e.g., FileSystemEnterpriseConnector {policy_options {mode: MANDATORY}}
}

void BoxAccessTokenFetcher::OnGetTokenSuccess(
    const TokenResponse& token_response) {
  std::move(callback_).Run(true, token_response.access_token,
                           token_response.refresh_token);
}

void BoxAccessTokenFetcher::OnGetTokenFailure(
    const GoogleServiceAuthError& error) {
  // TODO(https://crbug.com/1159179): pop a box about authentication failure?
  DLOG(ERROR) << "[BoxAccessTokenFetcher] Failed: " << error.error_message();
  std::move(callback_).Run(false, std::string(), std::string());
}

void SetFileSystemOAuth2Tokens(PrefService* prefs,
                               const std::string& service_provider,
                               const std::string& access_token,
                               const std::string& refresh_token) {
  std::string pref_path = base::StringPrintf(
      "enterprise_connectors.file_system.%s.", service_provider.c_str());
  prefs->SetString(pref_path + "access_token", access_token);
  prefs->SetString(pref_path + "refresh_token", refresh_token);
}

}  // namespace enterprise_connectors
