// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/signin/oauth2_token_initializer.h"

#include <memory>

#include "chrome/browser/browser_process.h"
#include "chrome/browser/net/system_network_context_manager.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace ash {

OAuth2TokenInitializer::OAuth2TokenInitializer() {}

OAuth2TokenInitializer::~OAuth2TokenInitializer() {}

void OAuth2TokenInitializer::Start(const UserContext& user_context,
                                   FetchOAuth2TokensCallback callback) {
  DCHECK(!user_context.GetAuthCode().empty());
  callback_ = std::move(callback);
  user_context_ = user_context;
  oauth2_token_fetcher_ = std::make_unique<OAuth2TokenFetcher>(
      this, g_browser_process->system_network_context_manager()
                ->GetSharedURLLoaderFactory());
  if (user_context.GetDeviceId().empty())
    NOTREACHED_IN_MIGRATION() << "Device ID is not set";
  oauth2_token_fetcher_->StartExchangeFromAuthCode(user_context.GetAuthCode(),
                                                   user_context.GetDeviceId());
}

void OAuth2TokenInitializer::OnOAuth2TokensAvailable(
    const GaiaAuthConsumer::ClientOAuthResult& result) {
  VLOG(1) << "OAuth2 tokens fetched";
  user_context_.SetAuthCode(std::string());
  user_context_.SetRefreshToken(result.refresh_token);
  user_context_.SetAccessToken(result.access_token);
  user_context_.SetIsUnderAdvancedProtection(
      result.is_under_advanced_protection);

  std::move(callback_).Run(true, user_context_);
}

void OAuth2TokenInitializer::OnOAuth2TokensFetchFailed() {
  LOG(WARNING) << "OAuth2TokenInitializer - OAuth2 token fetch failed";
  std::move(callback_).Run(false, user_context_);
}

}  // namespace ash
