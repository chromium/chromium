// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/signin/token_handle_fetcher.h"

#include <memory>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/values.h"
#include "chrome/browser/profiles/profile.h"
#include "components/account_id/account_id.h"
#include "google_apis/gaia/gaia_constants.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace ash {
namespace {

const int kMaxRetries = 3;

}  // namespace

TokenHandleFetcher::TokenHandleFetcher(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    const AccountId& account_id)
    : account_id_(account_id), gaia_client_(std::move(url_loader_factory)) {}

TokenHandleFetcher::~TokenHandleFetcher() = default;

void TokenHandleFetcher::Fetch(const std::string& access_token,
                               const std::string& refresh_token_hash,
                               TokenFetchCallback callback) {
  CHECK(!callback_)
      << "TokenHandleFetcher does not support concurrent requests";
  callback_ = std::move(callback);
  refresh_token_hash_ = refresh_token_hash;
  tokeninfo_response_start_time_ = base::TimeTicks::Now();
  gaia_client_.GetTokenInfo(access_token, kMaxRetries, this);
}

void TokenHandleFetcher::OnOAuthError() {
  SendCallbackResponse(/*success=*/false, /*token=*/std::string());
}

void TokenHandleFetcher::OnNetworkError(int response_code) {
  SendCallbackResponse(/*success=*/false, /*token=*/std::string());
}

void TokenHandleFetcher::OnGetTokenInfoResponse(
    const base::Value::Dict& token_info) {
  const base::TimeDelta duration =
      base::TimeTicks::Now() - tokeninfo_response_start_time_;
  base::UmaHistogramTimes("Login.TokenObtainResponseTime", duration);

  if (token_info.Find("error")) {
    SendCallbackResponse(/*success=*/false, /*token=*/std::string());
    return;
  }

  const std::string* handle = token_info.FindString("token_handle");
  if (!handle) {
    SendCallbackResponse(/*success=*/false, /*token=*/std::string());
    return;
  }

  SendCallbackResponse(/*success=*/true, /*token=*/*handle);
}

void TokenHandleFetcher::SendCallbackResponse(bool success,
                                              const std::string& token) {
  CHECK(callback_) << "Unexpected response received";
  std::move(callback_).Run(account_id_, success, token);
}

}  // namespace ash
