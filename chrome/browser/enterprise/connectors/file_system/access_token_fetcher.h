// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_CONNECTORS_FILE_SYSTEM_ACCESS_TOKEN_FETCHER_H_
#define CHROME_BROWSER_ENTERPRISE_CONNECTORS_FILE_SYSTEM_ACCESS_TOKEN_FETCHER_H_

#include "base/callback.h"
#include "components/prefs/pref_service.h"
#include "google_apis/gaia/oauth2_access_token_consumer.h"
#include "google_apis/gaia/oauth2_access_token_fetcher_impl.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

class PrefRegistrySimple;

namespace enterprise_connectors {

// Helper class to retrieve an access token for a file system service provider.
class AccessTokenFetcher : public OAuth2AccessTokenFetcherImpl,
                           public OAuth2AccessTokenConsumer {
 public:
  // Used in OnGetTokenSuccess/Failure; arguments are access_token and
  // refresh_token.
  using TokenCallback = base::OnceCallback<void(const GoogleServiceAuthError&,
                                                const std::string&,
                                                const std::string&)>;

  AccessTokenFetcher(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      const std::string& service_provder,
      const GURL& token_endpoint,
      const std::string& refresh_token,
      const std::string& auth_code,
      TokenCallback callback);
  ~AccessTokenFetcher() override;

  // The methods are protected for testing purposes only.
 protected:
  // OAuth2AccessTokenFetcherImpl interface.
  GURL GetAccessTokenURL() const override;
  net::NetworkTrafficAnnotationTag GetTrafficAnnotationTag() const override;

  // OAuth2AccessTokenConsumer interface.
  void OnGetTokenSuccess(const TokenResponse& token_response) override;
  void OnGetTokenFailure(const GoogleServiceAuthError& error) override;

 private:
  GURL token_endpoint_;
  net::NetworkTrafficAnnotationTag annotation_;
  TokenCallback callback_;
};

// Registers all the preferences needed to support the given service provider
// for use with the file system connector.
void RegisterFileSystemPrefsForServiceProvider(
    PrefRegistrySimple* registry,
    const std::string& service_provider);

// Stores the OAuth2 tokens for the given service provider.  Returns true
// if both tokens were successfully stored and false if either store fails.
bool SetFileSystemOAuth2Tokens(PrefService* prefs,
                               const std::string& service_provider,
                               const std::string& access_token,
                               const std::string& refresh_token);

// Clears the OAuth2 tokens for the given service provider.
bool ClearFileSystemAccessToken(PrefService* prefs,
                                const std::string& service_provider);
bool ClearFileSystemRefreshToken(PrefService* prefs,
                                 const std::string& service_provider);
bool ClearFileSystemOAuth2Tokens(PrefService* prefs,
                                 const std::string& service_provider);

// Retrieves the OAuth2 tokens for the given service provider.  If a token
// argument is null that token is not retrieved.  Returns true if all requested
// tokens are retrieved and false if any fail.
bool GetFileSystemOAuth2Tokens(PrefService* prefs,
                               const std::string& service_provider,
                               std::string* access_token,
                               std::string* refresh_token);

}  // namespace enterprise_connectors

#endif  // CHROME_BROWSER_ENTERPRISE_CONNECTORS_FILE_SYSTEM_ACCESS_TOKEN_FETCHER_H_
