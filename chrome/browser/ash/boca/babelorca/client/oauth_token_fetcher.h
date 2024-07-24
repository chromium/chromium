// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_BOCA_BABELORCA_CLIENT_OAUTH_TOKEN_FETCHER_H_
#define CHROME_BROWSER_ASH_BOCA_BABELORCA_CLIENT_OAUTH_TOKEN_FETCHER_H_

#include <memory>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/sequence_checker.h"
#include "base/timer/timer.h"
#include "chrome/browser/ash/boca/babelorca/client/token_fetcher.h"

namespace signin {
class AccessTokenFetcher;
class IdentityManager;
struct AccessTokenInfo;
}  // namespace signin

class GoogleServiceAuthError;

namespace babelorca {

// Tachyon oauth token fetcher.
class OAuthTokenFetcher : public TokenFetcher {
 public:
  explicit OAuthTokenFetcher(signin::IdentityManager* identity_manager);

  OAuthTokenFetcher(const OAuthTokenFetcher&) = delete;
  OAuthTokenFetcher& operator=(const OAuthTokenFetcher&) = delete;

  ~OAuthTokenFetcher() override;

  // TokenFetcher:
  void fetchToken(TokenFetchCallback callback) override;

 private:
  void fetchTokenInternal(TokenFetchCallback callback, int retry_num);

  void OnOAuthTokenRequestCompleted(TokenFetchCallback callback,
                                    int retry_num,
                                    GoogleServiceAuthError error,
                                    signin::AccessTokenInfo access_token_info);

  SEQUENCE_CHECKER(sequence_checker_);
  raw_ptr<signin::IdentityManager> identity_manager_;
  std::unique_ptr<signin::AccessTokenFetcher> access_token_fetcher_
      GUARDED_BY_CONTEXT(sequence_checker_);
  base::OneShotTimer retry_timer_;
};

}  // namespace babelorca

#endif  // CHROME_BROWSER_ASH_BOCA_BABELORCA_CLIENT_OAUTH_TOKEN_FETCHER_H_
