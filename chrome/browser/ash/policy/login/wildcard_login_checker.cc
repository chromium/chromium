// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/login/wildcard_login_checker.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/values.h"
#include "chrome/browser/ash/policy/core/policy_oauth2_token_fetcher.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/net/system_network_context_manager.h"
#include "components/policy/core/browser/browser_policy_connector.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace policy {

namespace {

// The oauth token consumer name.
const char kOAuthConsumerName[] = "policy_wildcard_login_checker";

// Presence of this key in the userinfo response indicates whether the user is
// on a hosted domain.
const char kHostedDomainKey[] = "hd";

}  // namespace

WildcardLoginChecker::WildcardLoginChecker() {}

WildcardLoginChecker::~WildcardLoginChecker() {}

void WildcardLoginChecker::StartWithRefreshToken(
    const std::string& refresh_token,
    StatusCallback callback) {
  CHECK(!token_fetcher_);
  CHECK(!user_info_fetcher_);

  callback_ = std::move(callback);

  token_fetcher_ = PolicyOAuth2TokenFetcher::CreateInstance(kOAuthConsumerName);
  token_fetcher_->StartWithRefreshToken(
      refresh_token,
      g_browser_process->system_network_context_manager()
          ->GetSharedURLLoaderFactory(),
      base::BindOnce(&WildcardLoginChecker::OnPolicyTokenFetched,
                     base::Unretained(this)));
}

void WildcardLoginChecker::StartWithAccessToken(const std::string& access_token,
                                                StatusCallback callback) {
  CHECK(!token_fetcher_);
  CHECK(!user_info_fetcher_);

  callback_ = std::move(callback);

  StartUserInfoFetcher(access_token);
}

void WildcardLoginChecker::OnGetUserInfoSuccess(
    const base::Value::Dict& response) {
  OnCheckCompleted(response.Find(kHostedDomainKey) ? RESULT_ALLOWED
                                                   : RESULT_BLOCKED);
}

void WildcardLoginChecker::OnGetUserInfoFailure(
    const GoogleServiceAuthError& error) {
  LOG(ERROR) << "Failed to fetch user info " << error.ToString();
  OnCheckCompleted(RESULT_FAILED);
}

void WildcardLoginChecker::OnPolicyTokenFetched(
    const std::string& access_token,
    const GoogleServiceAuthError& error) {
  if (error.state() != GoogleServiceAuthError::NONE) {
    LOG(ERROR) << "Failed to fetch policy token " << error.ToString();
    OnCheckCompleted(RESULT_FAILED);
    return;
  }

  token_fetcher_.reset();
  StartUserInfoFetcher(access_token);
}

void WildcardLoginChecker::StartUserInfoFetcher(
    const std::string& access_token) {
  user_info_fetcher_ = std::make_unique<UserInfoFetcher>(
      this, g_browser_process->shared_url_loader_factory());
  user_info_fetcher_->Start(access_token);
}

void WildcardLoginChecker::OnCheckCompleted(Result result) {
  if (!callback_.is_null())
    std::move(callback_).Run(result);
}

}  // namespace policy
