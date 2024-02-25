// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_SIGNIN_OAUTH2_TOKEN_FETCHER_H_
#define CHROME_BROWSER_ASH_LOGIN_SIGNIN_OAUTH2_TOKEN_FETCHER_H_

#include <string>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "google_apis/gaia/gaia_auth_consumer.h"
#include "google_apis/gaia/gaia_auth_fetcher.h"

namespace network {
class SharedURLLoaderFactory;
}

namespace ash {

// OAuth2TokenFetcher is used to convert authenticated cookie jar from the
// authentication profile into OAuth2 tokens and GAIA credentials that will be
// used to kick off other token retrieval tasks.
class OAuth2TokenFetcher : public GaiaAuthConsumer {
 public:
  class Delegate {
   public:
    virtual ~Delegate() {}
    virtual void OnOAuth2TokensAvailable(
        const GaiaAuthConsumer::ClientOAuthResult& oauth2_tokens) = 0;
    virtual void OnOAuth2TokensFetchFailed() = 0;
  };

  OAuth2TokenFetcher(
      OAuth2TokenFetcher::Delegate* delegate,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);

  OAuth2TokenFetcher(const OAuth2TokenFetcher&) = delete;
  OAuth2TokenFetcher& operator=(const OAuth2TokenFetcher&) = delete;

  ~OAuth2TokenFetcher() override;

  void StartExchangeFromAuthCode(const std::string& auth_code,
                                 const std::string& signin_scoped_device_id);

  base::WeakPtr<OAuth2TokenFetcher> AsWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  // Decides how to proceed on GAIA `error`. If the error looks temporary,
  // retries `task` until max retry count is reached.
  // If retry count runs out, or error condition is unrecoverable, it runs
  // `error_handler`.
  void RetryOnError(const GoogleServiceAuthError& error,
                    base::OnceClosure task,
                    base::OnceClosure error_handler);

  // GaiaAuthConsumer overrides.
  void OnClientOAuthSuccess(
      const GaiaAuthConsumer::ClientOAuthResult& result) override;
  void OnClientOAuthFailure(const GoogleServiceAuthError& error) override;

  raw_ptr<OAuth2TokenFetcher::Delegate> delegate_;
  GaiaAuthFetcher auth_fetcher_;

  // The retry counter. Increment this only when failure happened.
  int retry_count_;
  std::string session_index_;
  std::string signin_scoped_device_id_;
  std::string auth_code_;
  base::WeakPtrFactory<OAuth2TokenFetcher> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_SIGNIN_OAUTH2_TOKEN_FETCHER_H_
