// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_SIGNIN_OAUTH2_TOKEN_INITIALIZER_H_
#define CHROME_BROWSER_ASH_LOGIN_SIGNIN_OAUTH2_TOKEN_INITIALIZER_H_

#include <memory>

#include "base/functional/callback.h"
#include "chrome/browser/ash/login/signin/oauth2_token_fetcher.h"
#include "chromeos/ash/components/login/auth/public/user_context.h"

namespace ash {

// Performs initial fetch of OAuth2 Tokens.
class OAuth2TokenInitializer final : public OAuth2TokenFetcher::Delegate {
 public:
  // Callback to be invoked after initialization is done.
  using FetchOAuth2TokensCallback =
      base::OnceCallback<void(bool success, const UserContext& user_context)>;

  OAuth2TokenInitializer();

  OAuth2TokenInitializer(const OAuth2TokenInitializer&) = delete;
  OAuth2TokenInitializer& operator=(const OAuth2TokenInitializer&) = delete;

  ~OAuth2TokenInitializer() override;

  // Fetch OAuth2 tokens.
  void Start(const UserContext& context, FetchOAuth2TokensCallback callback);

 private:
  // OAuth2TokenFetcher::Delegate overrides.
  void OnOAuth2TokensAvailable(
      const GaiaAuthConsumer::ClientOAuthResult& oauth2_tokens) override;
  void OnOAuth2TokensFetchFailed() override;

  UserContext user_context_;
  FetchOAuth2TokensCallback callback_;
  std::unique_ptr<OAuth2TokenFetcher> oauth2_token_fetcher_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_SIGNIN_OAUTH2_TOKEN_INITIALIZER_H_
