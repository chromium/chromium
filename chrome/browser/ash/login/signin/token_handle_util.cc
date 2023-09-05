// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/signin/token_handle_util.h"

#include "base/json/values_util.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "components/user_manager/known_user.h"
#include "google_apis/gaia/gaia_oauth_client.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace ash {
namespace {

const char kTokenHandlePref[] = "PasswordTokenHandle";
const char kTokenHandleStatusPref[] = "TokenHandleStatus";
const char kTokenHandleLastCheckedPref[] = "TokenHandleLastChecked";

const char kHandleStatusValid[] = "valid";
const char kHandleStatusInvalid[] = "invalid";

constexpr int kMaxRetries = 3;

constexpr base::TimeDelta kCacheStatusTime = base::Hours(1);

const char* g_invalid_token_for_testing = nullptr;

bool MaybeReturnCachedStatus(
    const AccountId& account_id,
    const std::string& token,
    TokenHandleUtil::TokenValidationCallback* callback) {
  user_manager::KnownUser known_user(g_browser_process->local_state());
  const std::string* saved_status =
      known_user.FindStringPath(account_id, kTokenHandleStatusPref);
  if (!saved_status)
    return false;

  if (*saved_status == kHandleStatusValid) {
    std::move(*callback).Run(account_id, token,
                             TokenHandleUtil::Status::kValid);
    return true;
  }

  if (*saved_status == kHandleStatusInvalid) {
    std::move(*callback).Run(account_id, token,
                             TokenHandleUtil::Status::kInvalid);
    return true;
  }

  NOTREACHED();
  return false;
}

void OnStatusChecked(TokenHandleUtil::TokenValidationCallback callback,
                     const AccountId& account_id,
                     const std::string& token,
                     const TokenHandleUtil::Status& status) {
  user_manager::KnownUser known_user(g_browser_process->local_state());
  // Check that the token that was checked matches the latest known token.
  // (This may happen if token check took too long, and user went through
  // online sign-in and obtained new token during that time.
  if (const std::string* latest_token =
          known_user.FindStringPath(account_id, kTokenHandlePref)) {
    if (token != *latest_token) {
      LOG(WARNING) << "Outdated token, assuming status is unknown";
      std::move(callback).Run(account_id, token,
                              TokenHandleUtil::Status::kUnknown);
      return;
    }
  }

  if (status != TokenHandleUtil::Status::kUnknown) {
    // Update last checked timestamp.
    known_user.SetPath(account_id, kTokenHandleLastCheckedPref,
                       base::TimeToValue(base::Time::Now()));
  }

  if (status == TokenHandleUtil::Status::kInvalid) {
    known_user.SetStringPref(account_id, kTokenHandleStatusPref,
                             kHandleStatusInvalid);
  }
  std::move(callback).Run(account_id, token, status);
}

// Checks if token handle is explicitly marked as `kValid` for `account_id`.
bool HasTokenStatusInvalid(const AccountId& account_id) {
  user_manager::KnownUser known_user(g_browser_process->local_state());
  const std::string* status =
      known_user.FindStringPath(account_id, kTokenHandleStatusPref);

  return status && *status == kHandleStatusInvalid;
}

}  // namespace

TokenHandleUtil::TokenHandleUtil() = default;

TokenHandleUtil::~TokenHandleUtil() = default;

// static
bool TokenHandleUtil::HasToken(const AccountId& account_id) {
  user_manager::KnownUser known_user(g_browser_process->local_state());
  const std::string* token =
      known_user.FindStringPath(account_id, kTokenHandlePref);
  return token && !token->empty();
}

// static
bool TokenHandleUtil::IsRecentlyChecked(const AccountId& account_id) {
  user_manager::KnownUser known_user(g_browser_process->local_state());
  const base::Value* value =
      known_user.FindPath(account_id, kTokenHandleLastCheckedPref);
  if (!value)
    return false;

  absl::optional<base::Time> last_checked = base::ValueToTime(value);
  if (!last_checked.has_value()) {
    return false;
  }

  return base::Time::Now() - last_checked.value() < kCacheStatusTime;
}

// static
bool TokenHandleUtil::ShouldObtainHandle(const AccountId& account_id) {
  return !HasToken(account_id) || HasTokenStatusInvalid(account_id);
}

// static
void TokenHandleUtil::CheckToken(
    const AccountId& account_id,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    TokenValidationCallback callback) {
  user_manager::KnownUser known_user(g_browser_process->local_state());
  const std::string* token =
      known_user.FindStringPath(account_id, kTokenHandlePref);
  if (!token) {
    std::move(callback).Run(account_id, std::string(), Status::kUnknown);
    return;
  }

  if (g_invalid_token_for_testing && g_invalid_token_for_testing == *token) {
    std::move(callback).Run(account_id, *token, Status::kInvalid);
    return;
  }

  if (IsRecentlyChecked(account_id) &&
      MaybeReturnCachedStatus(account_id, *token, &callback)) {
    return;
  }

  // If token is explicitly marked as invalid, it does not make sense to check
  // it again.
  if (HasTokenStatusInvalid(account_id)) {
    std::move(callback).Run(account_id, *token, Status::kInvalid);
    return;
  }

  // Constructor starts validation.
  validation_delegates_[*token] = std::make_unique<TokenDelegate>(
      weak_factory_.GetWeakPtr(), account_id, *token,
      std::move(url_loader_factory),
      base::BindOnce(&OnStatusChecked, std::move(callback)));
}

// static
void TokenHandleUtil::StoreTokenHandle(const AccountId& account_id,
                                       const std::string& handle) {
  user_manager::KnownUser known_user(g_browser_process->local_state());

  known_user.SetStringPref(account_id, kTokenHandlePref, handle);
  known_user.SetStringPref(account_id, kTokenHandleStatusPref,
                           kHandleStatusValid);
  known_user.SetPath(account_id, kTokenHandleLastCheckedPref,
                     base::TimeToValue(base::Time::Now()));
}

// static
void TokenHandleUtil::SetInvalidTokenForTesting(const char* token) {
  g_invalid_token_for_testing = token;
}

// static
void TokenHandleUtil::SetLastCheckedPrefForTesting(const AccountId& account_id,
                                                   base::Time time) {
  user_manager::KnownUser known_user(g_browser_process->local_state());
  known_user.SetPath(account_id, kTokenHandleLastCheckedPref,
                     base::TimeToValue(time));
}

void TokenHandleUtil::OnValidationComplete(const std::string& token) {
  validation_delegates_.erase(token);
}

TokenHandleUtil::TokenDelegate::TokenDelegate(
    const base::WeakPtr<TokenHandleUtil>& owner,
    const AccountId& account_id,
    const std::string& token,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    TokenValidationCallback callback)
    : owner_(owner),
      account_id_(account_id),
      token_(token),
      tokeninfo_response_start_time_(base::TimeTicks::Now()),
      callback_(std::move(callback)),
      gaia_client_(std::move(url_loader_factory)) {
  gaia_client_.GetTokenHandleInfo(token_, kMaxRetries, this);
}

TokenHandleUtil::TokenDelegate::~TokenDelegate() = default;

void TokenHandleUtil::TokenDelegate::OnOAuthError() {
  std::move(callback_).Run(account_id_, token_, Status::kInvalid);
  NotifyDone(/*request_completed=*/true);
}

// Warning: NotifyDone() deletes `this`
void TokenHandleUtil::TokenDelegate::NotifyDone(bool request_completed) {
  if (request_completed) {
    RecordTokenCheckResponseTime();
  }
  if (owner_)
    owner_->OnValidationComplete(token_);
}

void TokenHandleUtil::TokenDelegate::OnNetworkError(int response_code) {
  std::move(callback_).Run(account_id_, token_, Status::kUnknown);
  NotifyDone(/*request_completed=*/response_code != -1);
}

void TokenHandleUtil::TokenDelegate::RecordTokenCheckResponseTime() {
  const base::TimeDelta duration =
      base::TimeTicks::Now() - tokeninfo_response_start_time_;
  base::UmaHistogramTimes("Login.TokenCheckResponseTime", duration);
}

void TokenHandleUtil::TokenDelegate::OnGetTokenInfoResponse(
    const base::Value::Dict& token_info) {
  Status outcome = Status::kUnknown;
  if (!token_info.Find("error")) {
    absl::optional<int> expires_in = token_info.FindInt("expires_in");
    if (expires_in)
      outcome = (*expires_in < 0) ? Status::kInvalid : Status::kValid;
  }

  std::move(callback_).Run(account_id_, token_, outcome);
  NotifyDone(/*request_completed=*/true);
}

}  // namespace ash
