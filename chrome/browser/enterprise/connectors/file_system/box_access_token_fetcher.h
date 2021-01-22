// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_CONNECTORS_FILE_SYSTEM_BOX_ACCESS_TOKEN_FETCHER_H_
#define CHROME_BROWSER_ENTERPRISE_CONNECTORS_FILE_SYSTEM_BOX_ACCESS_TOKEN_FETCHER_H_

#include "base/callback.h"
#include "components/prefs/pref_service.h"
#include "google_apis/gaia/oauth2_access_token_consumer.h"
#include "google_apis/gaia/oauth2_access_token_fetcher_impl.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace enterprise_connectors {

// Helper class to retrieve a Box access token.
// Notes on lifetime:
// - When used by FileSystemSigninDialogDelegate, ShowDialog() blocks, which
// ensures this class is alive, until the entire authentication (including this
// class) process is completed.
// - In event of termination of browser/tab, the URLLoader in base class is
// safely deletable, even while it's invoking any callback method passed to it.
class BoxAccessTokenFetcher : public OAuth2AccessTokenFetcherImpl,
                              public OAuth2AccessTokenConsumer {
 public:
  // Used in OnGetTokenSuccess/Failure; arguments are access_token and
  // refresh_token.
  using TokenCallback =
      base::OnceCallback<void(bool, const std::string&, const std::string&)>;

  BoxAccessTokenFetcher(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      const std::string& refresh_token,
      const std::string& auth_code,
      TokenCallback callback);
  ~BoxAccessTokenFetcher() override;

  // The methods are protected for testing purposes only.
 protected:
  // OAuth2AccessTokenFetcherImpl interface.
  GURL GetAccessTokenURL() const override;
  net::NetworkTrafficAnnotationTag GetTrafficAnnotationTag() const override;

  // OAuth2AccessTokenConsumer interface.
  void OnGetTokenSuccess(const TokenResponse& token_response) override;
  void OnGetTokenFailure(const GoogleServiceAuthError& error) override;

 private:
  TokenCallback callback_;
};

void SetFileSystemOAuth2Tokens(PrefService* prefs,
                               const std::string& service_provider,
                               const std::string& access_token,
                               const std::string& refresh_token);

}  // namespace enterprise_connectors

#endif  // CHROME_BROWSER_ENTERPRISE_CONNECTORS_FILE_SYSTEM_BOX_ACCESS_TOKEN_FETCHER_H_
