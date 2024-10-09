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
#include "chromeos/ash/components/cryptohome/auth_factor.h"
#include "chromeos/ash/components/login/auth/public/cryptohome_key_constants.h"
#include "chromeos/ash/components/login/auth/public/user_context.h"
#include "components/user_manager/known_user.h"
#include "components/user_manager/user_manager.h"
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
    std::move(*callback).Run(account_id, token, /*reauth_required=*/false);
    return true;
  }

  if (*saved_status == kHandleStatusInvalid) {
    std::move(*callback).Run(account_id, token, /*reauth_required=*/true);
    return true;
  }

  NOTREACHED_IN_MIGRATION();
  return false;
}

bool IsReauthRequired(const TokenHandleUtil::Status& status,
                      bool user_has_gaia_password) {
  switch (status) {
    case TokenHandleUtil::Status::kUnknown:
    case TokenHandleUtil::Status::kValid:
      return false;
    case TokenHandleUtil::Status::kInvalid:
      return true;
    case TokenHandleUtil::Status::kExpired:
      // When the status of the token is `kExpired`, enforce re-authentication
      // only if the user is using their Gaia password for logging in.
      return user_has_gaia_password;
  }
  NOTREACHED_IN_MIGRATION();
  return false;
}

void FinishWithStatus(TokenHandleUtil::TokenValidationCallback callback,
                      const std::string& token,
                      const AccountId& account_id,
                      const TokenHandleUtil::Status& status,
                      std::optional<bool> user_has_gaia_password) {
  bool has_gaia_pass = user_has_gaia_password.value_or(true);
  user_manager::KnownUser known_user(g_browser_process->local_state());
  // Check that the token that was checked matches the latest known token.
  // This may happen if token check took too long, and user went through
  // online sign-in and obtained new token during that time.
  if (const std::string* latest_token =
          known_user.FindStringPath(account_id, kTokenHandlePref)) {
    if (token != *latest_token) {
      LOG(WARNING) << "Outdated token, assuming status is unknown";
      std::move(callback).Run(account_id, token, /*reauth_required=*/false);
      return;
    }
  }

  if (status != TokenHandleUtil::Status::kUnknown) {
    // Update last checked timestamp.
    known_user.SetPath(account_id, kTokenHandleLastCheckedPref,
                       base::TimeToValue(base::Time::Now()));
  }
  bool is_reauth_required = IsReauthRequired(status, has_gaia_pass);
  if (is_reauth_required) {
    known_user.SetStringPref(account_id, kTokenHandleStatusPref,
                             kHandleStatusInvalid);
  }
  std::move(callback).Run(account_id, token,
                          /*reauth_required=*/is_reauth_required);
}

// Checks if token handle is explicitly marked as `kValid` for `account_id`.
bool HasTokenStatusInvalid(const AccountId& account_id) {
  user_manager::KnownUser known_user(g_browser_process->local_state());
  const std::string* status =
      known_user.FindStringPath(account_id, kTokenHandleStatusPref);

  return status && *status == kHandleStatusInvalid;
}

// Callback used in `AuthFactorEditor::GetAuthFactorsConfiguration()`.
std::optional<bool> DoesUserUseGaiaPassword(
    std::unique_ptr<UserContext> user_context,
    std::optional<AuthenticationError> error) {
  if (error.has_value()) {
    // We don't know what auth factors the user has.
    return std::nullopt;
  }

  auto* factor = user_context->GetAuthFactorsConfiguration().FindFactorByType(
      cryptohome::AuthFactorType::kPassword);
  if (factor && factor->ref().label().value() == ash::kCryptohomeGaiaKeyLabel) {
    return true;
  }

  return false;
}

}  // namespace

TokenHandleUtil::TokenHandleUtil()
    : factor_editor_(UserDataAuthClient::Get()) {}

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

  std::optional<base::Time> last_checked = base::ValueToTime(value);
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
void TokenHandleUtil::IsReauthRequired(
    const AccountId& account_id,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    TokenValidationCallback callback) {
  const user_manager::User* user =
      user_manager::UserManager::Get()->FindUser(account_id);

  if (!user) {
    DUMP_WILL_BE_NOTREACHED() << "Invalid user";
    std::move(callback).Run(account_id, std::string(),
                            /*reauth_required=*/false);
    return;
  }

  user_manager::KnownUser known_user(g_browser_process->local_state());
  const std::string* token =
      known_user.FindStringPath(account_id, kTokenHandlePref);
  if (!token) {
    std::move(callback).Run(account_id, std::string(),
                            /*reauth_required=*/false);
    return;
  }

  if (g_invalid_token_for_testing && g_invalid_token_for_testing == *token) {
    std::move(callback).Run(account_id, *token, /*reauth_required=*/true);
    return;
  }

  if (IsRecentlyChecked(account_id) &&
      MaybeReturnCachedStatus(account_id, *token, &callback)) {
    return;
  }

  // If token is explicitly marked as invalid, it does not make sense to check
  // it again.
  if (HasTokenStatusInvalid(account_id)) {
    std::move(callback).Run(account_id, *token, /*reauth_required=*/true);
    return;
  }

  // Constructor starts validation.
  validation_delegates_[*token] = std::make_unique<TokenDelegate>(
      weak_factory_.GetWeakPtr(), account_id, *token,
      std::move(url_loader_factory),
      base::BindOnce(&TokenHandleUtil::OnStatusChecked,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
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

void TokenHandleUtil::OnStatusChecked(TokenValidationCallback callback,
                                      const AccountId& account_id,
                                      const std::string& token,
                                      const Status& status) {
  const user_manager::User* user =
      user_manager::UserManager::Get()->FindUser(account_id);
  if (!user) {
    DUMP_WILL_BE_NOTREACHED() << "Invalid user";
    FinishWithStatus(std::move(callback), token, account_id, status,
                     /*user_has_gaia_password=*/true);
    return;
  }
  factor_editor_.GetAuthFactorsConfiguration(
      std::make_unique<UserContext>(*user),
      base::BindOnce(&DoesUserUseGaiaPassword)
          .Then(base::BindOnce(&FinishWithStatus, std::move(callback), token,
                               account_id, status)));
}

void TokenHandleUtil::OnValidationComplete(const std::string& token) {
  validation_delegates_.erase(token);
}

TokenHandleUtil::TokenDelegate::TokenDelegate(
    const base::WeakPtr<TokenHandleUtil>& owner,
    const AccountId& account_id,
    const std::string& token,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    TokenDelegateCallback callback)
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
    std::optional<int> expires_in = token_info.FindInt("expires_in");
    if (expires_in) {
      outcome = (*expires_in < 0) ? Status::kExpired : Status::kValid;
    }
  }

  std::move(callback_).Run(account_id_, token_, outcome);
  NotifyDone(/*request_completed=*/true);
}

}  // namespace ash
