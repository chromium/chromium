// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/enrollment/oauth2_token_revoker.h"

#include <string>

#include "base/task/single_thread_task_runner.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/net/system_network_context_manager.h"
#include "google_apis/gaia/gaia_auth_consumer.h"
#include "google_apis/gaia/gaia_auth_fetcher.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace ash {

// A helper class that takes care of asynchronously revoking a given token.
class TokenRevokerHelper : public GaiaAuthConsumer {
 public:
  TokenRevokerHelper()
      : gaia_fetcher_(this,
                      gaia::GaiaSource::kChromeOS,
                      g_browser_process->system_network_context_manager()
                          ->GetSharedURLLoaderFactory()) {}

  TokenRevokerHelper(const TokenRevokerHelper&) = delete;
  TokenRevokerHelper& operator=(const TokenRevokerHelper&) = delete;

  void Start(const std::string& token) {
    gaia_fetcher_.StartRevokeOAuth2Token(token);
  }

 private:
  // GaiaAuthConsumer:
  void OnOAuth2RevokeTokenCompleted(
      GaiaAuthConsumer::TokenRevocationStatus status) override {
    base::SingleThreadTaskRunner::GetCurrentDefault()->DeleteSoon(FROM_HERE,
                                                                  this);
  }

  GaiaAuthFetcher gaia_fetcher_;
};

void OAuth2TokenRevoker::Start(const std::string& token) {
  (new TokenRevokerHelper())->Start(token);
}

}  // namespace ash
