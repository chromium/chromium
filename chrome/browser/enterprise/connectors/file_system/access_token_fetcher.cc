// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/file_system/access_token_fetcher.h"

#include "base/base64.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/enterprise/connectors/connectors_prefs.h"
#include "chrome/browser/enterprise/connectors/file_system/account_info_utils.h"
#include "components/os_crypt/os_crypt.h"
#include "components/prefs/pref_registry_simple.h"
#include "google_apis/gaia/google_service_auth_error.h"

namespace enterprise_connectors {

namespace {

// Traffic annotation strings must be fully defined at compile time.  They
// can't be dynamically built at runtime based on the |service_provider|.
// This function hard codes all known chrome partners and return the
// appropriate annotation for each.
net::NetworkTrafficAnnotationTag GetAnnotation(
    const std::string& service_provider) {
  if (service_provider == kFileSystemServiceProviderPrefNameBox) {
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

  return net::NetworkTrafficAnnotationTag::NotReached();
}

}  // namespace

AccessTokenFetcher::AccessTokenFetcher(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    const std::string& service_provder,
    const GURL& token_endpoint,
    const std::string& refresh_token,
    const std::string& auth_code,
    const std::string& consumer_name,
    TokenCallback callback)
    : OAuth2AccessTokenFetcherImpl(this,
                                   url_loader_factory,
                                   refresh_token,
                                   auth_code),
      token_endpoint_(token_endpoint),
      annotation_(GetAnnotation(service_provder)),
      consumer_name_(consumer_name),
      callback_(std::move(callback)) {}

AccessTokenFetcher::~AccessTokenFetcher() = default;

GURL AccessTokenFetcher::GetAccessTokenURL() const {
  return token_endpoint_;
}

net::NetworkTrafficAnnotationTag AccessTokenFetcher::GetTrafficAnnotationTag()
    const {
  return annotation_;
}

void AccessTokenFetcher::OnGetTokenSuccess(
    const TokenResponse& token_response) {
  std::move(callback_).Run(GoogleServiceAuthError::AuthErrorNone(),
                           token_response.access_token,
                           token_response.refresh_token);
}

void AccessTokenFetcher::OnGetTokenFailure(
    const GoogleServiceAuthError& error) {
  // TODO(https://crbug.com/1159179): pop a dialog about authentication failure?
  DLOG(ERROR) << "[AccessTokenFetcher] Failed: " << error.error_message();
  std::move(callback_).Run(error, std::string(), std::string());
}

std::string AccessTokenFetcher::GetConsumerName() const {
  return consumer_name_;
}

}  // namespace enterprise_connectors
