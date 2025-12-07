// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/signin/token_handle_checker.h"

#include <optional>
#include <string>
#include <utility>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/time/time.h"
#include "base/values.h"
#include "components/account_id/account_id.h"
#include "google_apis/gaia/gaia_oauth_client.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace ash {

const char kTokenCheckResponseTime[] = "Login.TokenCheckResponseTime";

namespace {

constexpr int kMaxRetries = 3;

std::string StatusToString(const TokenHandleChecker::Status& status) {
  switch (status) {
    case TokenHandleChecker::Status::kValid:
      return "Valid";
    case TokenHandleChecker::Status::kInvalid:
      return "Invalid";
    case TokenHandleChecker::Status::kExpired:
      return "Expired";
    case TokenHandleChecker::Status::kUnknown:
      return "Unknown";
  }
}

}  // namespace

TokenHandleChecker::TokenHandleChecker(
    const AccountId& account_id,
    const std::string& token,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
    : account_id_(account_id),
      token_(token),
      gaia_client_(std::move(url_loader_factory)) {}

TokenHandleChecker::~TokenHandleChecker() = default;

void TokenHandleChecker::StartCheck(OnTokenChecked callback) {
  CHECK(!callback_) << "This object can only handle one request at a time";
  tokeninfo_response_start_time_ = base::TimeTicks::Now();
  callback_ = std::move(callback);
  gaia_client_.GetTokenHandleInfo(token_, kMaxRetries, this);
}

void TokenHandleChecker::OnOAuthError() {
  SendCallbackResponse(Status::kInvalid, /*should_record_response_time=*/true);
}

void TokenHandleChecker::OnNetworkError(int response_code) {
  SendCallbackResponse(Status::kUnknown,
                       /*should_record_response_time=*/response_code != -1);
}

void TokenHandleChecker::OnGetTokenInfoResponse(
    const base::Value::Dict& token_info) {
  Status outcome = Status::kUnknown;
  if (!token_info.Find("error")) {
    std::optional<int> expires_in = token_info.FindInt("expires_in");
    if (expires_in) {
      outcome = (*expires_in < 0) ? Status::kExpired : Status::kValid;
    }
  }

  SendCallbackResponse(outcome, /*should_record_response_time=*/true);
}

void TokenHandleChecker::SendCallbackResponse(
    const Status& outcome,
    bool should_record_response_time) {
  CHECK(callback_) << "Unexpected response received";
  VLOG(1) << "Token handle check completed, status: "
          << StatusToString(outcome);

  if (should_record_response_time) {
    RecordTokenCheckResponseTime();
  }

  std::move(callback_).Run(account_id_, token_, outcome);
}

void TokenHandleChecker::RecordTokenCheckResponseTime() {
  const base::TimeDelta duration =
      base::TimeTicks::Now() - tokeninfo_response_start_time_;
  base::UmaHistogramTimes(kTokenCheckResponseTime, duration);
}

}  // namespace ash
