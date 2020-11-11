// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/wildcard_login_checker.h"

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/metrics/histogram_macros.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/policy/policy_oauth2_token_fetcher.h"
#include "chrome/browser/net/system_network_context_manager.h"
#include "components/policy/core/browser/browser_policy_connector.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace policy {

namespace {

// Presence of this key in the userinfo response indicates whether the user is
// on a hosted domain.
const char kHostedDomainKey[] = "hd";

// UMA histogram names.
const char kUMADelayPolicyTokenFetch[] =
    "Enterprise.WildcardLoginCheck.DelayPolicyTokenFetch";
const char kUMADelayUserInfoFetch[] =
    "Enterprise.WildcardLoginCheck.DelayUserInfoFetch";
const char kUMADelayTotal[] =
    "Enterprise.WildcardLoginCheck.DelayTotal";

}  // namespace

WildcardLoginChecker::WildcardLoginChecker() {}

WildcardLoginChecker::~WildcardLoginChecker() {}

void WildcardLoginChecker::StartWithRefreshToken(
    const std::string& refresh_token,
    StatusCallback callback) {
  CHECK(!token_fetcher_);
  CHECK(!user_info_fetcher_);

  start_timestamp_ = base::Time::Now();
  callback_ = std::move(callback);

  token_fetcher_ = PolicyOAuth2TokenFetcher::CreateInstance();
  token_fetcher_->StartWithRefreshToken(
      refresh_token,
      g_browser_process->system_network_context_manager()
          ->GetSharedURLLoaderFactory(),
      base::Bind(&WildcardLoginChecker::OnPolicyTokenFetched,
                 base::Unretained(this)));
}

void WildcardLoginChecker::StartWithAccessToken(const std::string& access_token,
                                                StatusCallback callback) {
  CHECK(!token_fetcher_);
  CHECK(!user_info_fetcher_);

  start_timestamp_ = base::Time::Now();
  callback_ = std::move(callback);

  StartUserInfoFetcher(access_token);
}

void WildcardLoginChecker::OnGetUserInfoSuccess(
    const base::DictionaryValue* response) {
  if (!start_timestamp_.is_null()) {
    base::Time now = base::Time::Now();
    UMA_HISTOGRAM_MEDIUM_TIMES(kUMADelayUserInfoFetch,
                               now - token_available_timestamp_);
    UMA_HISTOGRAM_MEDIUM_TIMES(kUMADelayTotal,
                               now - start_timestamp_);
  }

  OnCheckCompleted(response->HasKey(kHostedDomainKey) ? RESULT_ALLOWED
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

  if (!start_timestamp_.is_null()) {
    token_available_timestamp_ = base::Time::Now();
    UMA_HISTOGRAM_MEDIUM_TIMES(kUMADelayPolicyTokenFetch,
                               token_available_timestamp_ - start_timestamp_);
  }

  token_fetcher_.reset();
  StartUserInfoFetcher(access_token);
}

void WildcardLoginChecker::StartUserInfoFetcher(
    const std::string& access_token) {
  user_info_fetcher_.reset(new UserInfoFetcher(
      this, g_browser_process->shared_url_loader_factory()));
  user_info_fetcher_->Start(access_token);
}

void WildcardLoginChecker::OnCheckCompleted(Result result) {
  if (!callback_.is_null())
    std::move(callback_).Run(result);
}

}  // namespace policy
