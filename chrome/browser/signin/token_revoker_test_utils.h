// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SIGNIN_TOKEN_REVOKER_TEST_UTILS_H_
#define CHROME_BROWSER_SIGNIN_TOKEN_REVOKER_TEST_UTILS_H_

#include "base/memory/ref_counted.h"
#include "google_apis/gaia/gaia_auth_fetcher.h"

namespace content {
class MessageLoopRunner;
}

namespace token_revoker_test_utils {

// A helper class that takes care of asynchronously revoking a refresh token.
class RefreshTokenRevoker : public GaiaAuthConsumer {
 public:
  RefreshTokenRevoker();

  RefreshTokenRevoker(const RefreshTokenRevoker&) = delete;
  RefreshTokenRevoker& operator=(const RefreshTokenRevoker&) = delete;

  ~RefreshTokenRevoker() override;

  // Sends a request to Gaia servers to revoke the refresh token. Blocks until
  // it is revoked, i.e. until OnOAuth2RevokeTokenCompleted is fired.
  void Revoke(const std::string& token);

  // Called when token is revoked.
  void OnOAuth2RevokeTokenCompleted(
      GaiaAuthConsumer::TokenRevocationStatus status) override;

 private:
  GaiaAuthFetcher gaia_fetcher_;
  scoped_refptr<content::MessageLoopRunner> message_loop_runner_;
};

}  // namespace token_revoker_test_utils

#endif  // CHROME_BROWSER_SIGNIN_TOKEN_REVOKER_TEST_UTILS_H_
