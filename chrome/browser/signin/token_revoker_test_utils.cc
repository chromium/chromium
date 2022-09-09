// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/token_revoker_test_utils.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/net/system_network_context_manager.h"
#include "content/public/test/test_utils.h"
#include "google_apis/gaia/gaia_auth_fetcher.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace token_revoker_test_utils {

RefreshTokenRevoker::RefreshTokenRevoker()
    : gaia_fetcher_(this,
                    gaia::GaiaSource::kChrome,
                    g_browser_process->system_network_context_manager()
                        ->GetSharedURLLoaderFactory()) {}

RefreshTokenRevoker::~RefreshTokenRevoker() {
}

void RefreshTokenRevoker::Revoke(const std::string& token) {
  DVLOG(1) << "Starting RefreshTokenRevoker for token: " << token;
  gaia_fetcher_.StartRevokeOAuth2Token(token);
  message_loop_runner_ = new content::MessageLoopRunner;
  message_loop_runner_->Run();
}

void RefreshTokenRevoker::OnOAuth2RevokeTokenCompleted(
    GaiaAuthConsumer::TokenRevocationStatus status) {
  DVLOG(1) << "TokenRevoker OnOAuth2RevokeTokenCompleted";
  message_loop_runner_->Quit();
}

}  // namespace token_revoker_test_utils
