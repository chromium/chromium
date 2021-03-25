// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/file_system/access_token_fetcher.h"

#include "base/base64.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/enterprise/connectors/connectors_prefs.h"
#include "components/os_crypt/os_crypt.h"
#include "components/prefs/pref_registry_simple.h"
#include "google_apis/gaia/google_service_auth_error.h"

namespace enterprise_connectors {

namespace {

// Templates for the profile preferences paths to store the access and
// refresh tokens, per service provider.
constexpr char kAccessTokenPrefPathTemplate[] =
      "enterprise_connectors.file_system.%s.access_token";
constexpr char kRefreshTokenPrefPathTemplate[] =
      "enterprise_connectors.file_system.%s.refresh_token";
constexpr char kBoxProviderName[] = "box";

// Traffic annotation strings must be fully defined at compile time.  They
// can't be dynamically built at runtime based on the |service_provider|.
// This function hard codes all known chrome partners and return the
// appropriate annotation for each.
net::NetworkTrafficAnnotationTag GetAnnotation(
    const std::string& service_provider) {
  if (service_provider == kBoxProviderName) {
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

// Decrypt and return a profile preference that holds an encrypted access
// or refresh token.
bool DecryptPref(PrefService* prefs,
                 const std::string& path,
                 std::string* value) {
  std::string b64_enc_token = prefs->GetString(path);
  std::string enc_token;
  if (!base::Base64Decode(b64_enc_token, &enc_token) ||
      !OSCrypt::DecryptString(enc_token, value)) {
    return false;
  }

  return true;
}

}  // namespace

AccessTokenFetcher::AccessTokenFetcher(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    const std::string& service_provder,
    const GURL& token_endpoint,
    const std::string& refresh_token,
    const std::string& auth_code,
    TokenCallback callback)
    : OAuth2AccessTokenFetcherImpl(this,
                                   url_loader_factory,
                                   refresh_token,
                                   auth_code),
      token_endpoint_(token_endpoint),
      annotation_(GetAnnotation(service_provder)),
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

void RegisterFileSystemPrefsForServiceProvider(
    PrefRegistrySimple* registry,
    const std::string& service_provider) {
  registry->RegisterStringPref(base::StringPrintf(kAccessTokenPrefPathTemplate,
                                                  service_provider.c_str()),
                               std::string());
  registry->RegisterStringPref(
      base::StringPrintf(kRefreshTokenPrefPathTemplate,
                         service_provider.c_str()),
      std::string());
  // Currently need this caching only for Box, depending on what other 3P APIs
  // look like we may want to do this more generally.
  if (service_provider == kBoxProviderName) {
    registry->RegisterStringPref(kFileSystemUploadFolderIdPref, std::string());
  }
}

bool SetFileSystemToken(PrefService* prefs,
                        const std::string& service_provider,
                        const char token_pref_path_template[],
                        const std::string& token) {
  std::string enc_token;
  if (!OSCrypt::EncryptString(token, &enc_token)) {
    return false;
  }

  std::string b64_enc_token;
  base::Base64Encode(enc_token, &b64_enc_token);
  prefs->SetString(
      base::StringPrintf(token_pref_path_template, service_provider.c_str()),
      b64_enc_token);
  return true;
}

bool SetFileSystemOAuth2Tokens(PrefService* prefs,
                               const std::string& service_provider,
                               const std::string& access_token,
                               const std::string& refresh_token) {
  return SetFileSystemToken(prefs, service_provider,
                            kAccessTokenPrefPathTemplate, access_token) &&
         SetFileSystemToken(prefs, service_provider,
                            kRefreshTokenPrefPathTemplate, refresh_token);
}

bool ClearFileSystemAccessToken(PrefService* prefs,
                                const std::string& service_provider) {
  return SetFileSystemToken(prefs, service_provider,
                            kAccessTokenPrefPathTemplate, std::string());
}

bool ClearFileSystemRefreshToken(PrefService* prefs,
                                 const std::string& service_provider) {
  return SetFileSystemToken(prefs, service_provider,
                            kRefreshTokenPrefPathTemplate, std::string());
}

bool ClearFileSystemOAuth2Tokens(PrefService* prefs,
                                 const std::string& service_provider) {
  return ClearFileSystemAccessToken(prefs, service_provider) &&
         ClearFileSystemRefreshToken(prefs, service_provider);
}

bool GetFileSystemOAuth2Tokens(PrefService* prefs,
                               const std::string& service_provider,
                               std::string* access_token,
                               std::string* refresh_token) {
  if (access_token) {
    if (!DecryptPref(prefs,
                     base::StringPrintf(kAccessTokenPrefPathTemplate,
                                        service_provider.c_str()),
                     access_token)) {
      return false;
    }
  }

  if (refresh_token) {
    if (!DecryptPref(prefs,
                     base::StringPrintf(kRefreshTokenPrefPathTemplate,
                                        service_provider.c_str()),
                     refresh_token)) {
      return false;
    }
  }

  return true;
}

}  // namespace enterprise_connectors
