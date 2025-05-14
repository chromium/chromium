// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/signin/token_handle_store_impl.h"

#include <algorithm>

#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/json/values_util.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "components/account_id/account_id.h"
#include "components/user_manager/known_user.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"

namespace ash {

namespace {

constexpr char kTokenHandlePref[] = "PasswordTokenHandle";
constexpr char kTokenHandleStatusPref[] = "TokenHandleStatus";
constexpr char kTokenHandleLastCheckedPref[] = "TokenHandleLastChecked";
constexpr char kTokenHandleStatusInvalid[] = "invalid";
constexpr char kTokenHandleStatusValid[] = "valid";
constexpr char kTokenHandleStatusStale[] = "stale";
constexpr base::TimeDelta kCacheStatusTime = base::Hours(1);

bool IsReauthRequired(const TokenHandleChecker::Status& status,
                      bool user_has_gaia_password) {
  switch (status) {
    case TokenHandleChecker::Status::kUnknown:
    case TokenHandleChecker::Status::kValid:
      return false;
    case TokenHandleChecker::Status::kInvalid:
      return true;
    case TokenHandleChecker::Status::kExpired:
      // When the status of the token is `kExpired`, enforce re-authentication
      // only if the user is using their Gaia password for logging in.
      return user_has_gaia_password;
  }
  NOTREACHED();
}

}  // namespace

TokenHandleStoreImpl::TokenHandleStoreImpl(
    std::unique_ptr<user_manager::KnownUser> known_user,
    DoesUserHaveGaiaPasswordCallback does_user_have_gaia_password)
    : known_user_(std::move(known_user)),
      does_user_have_gaia_password_(std::move(does_user_have_gaia_password)) {}

TokenHandleStoreImpl::~TokenHandleStoreImpl() = default;

bool TokenHandleStoreImpl::HasToken(const AccountId& account_id) const {
  const std::string* token =
      known_user_->FindStringPath(account_id, kTokenHandlePref);
  return token && !token->empty();
}

bool TokenHandleStoreImpl::ShouldObtainHandle(
    const AccountId& account_id) const {
  return !HasToken(account_id) || HasTokenStatusInvalid(account_id);
}

bool TokenHandleStoreImpl::IsRecentlyChecked(
    const AccountId& account_id) const {
  const base::Value* value =
      known_user_->FindPath(account_id, kTokenHandleLastCheckedPref);
  if (!value) {
    return false;
  }

  std::optional<base::Time> last_checked = base::ValueToTime(value);
  if (!last_checked.has_value()) {
    return false;
  }

  return base::Time::Now() - last_checked.value() < kCacheStatusTime;
}

void TokenHandleStoreImpl::IsReauthRequired(
    const AccountId& account_id,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    TokenValidationCallback callback) {
  if (const user_manager::User* user =
          user_manager::UserManager::Get()->FindUser(account_id);
      !user) {
    DUMP_WILL_BE_NOTREACHED() << "Invalid user";
    std::move(callback).Run(account_id, /*token=*/std::string(),
                            /*reauth_required=*/false);
    return;
  }

  const std::string* token =
      known_user_->FindStringPath(account_id, kTokenHandlePref);
  if (!token) {
    // A token fetch is still in flight and we do not have a token yet. This is
    // not an error case, so no error handling or logging is required.
    std::move(callback).Run(account_id, /*token=*/std::string(),
                            /*reauth_required=*/false);
    return;
  }

  if (invalid_token_for_testing_ == *token) {
    std::move(callback).Run(account_id, *token, /*reauth_required=*/true);
    return;
  }

  pending_callbacks_[account_id].push_back(std::move(callback));

  // Overwriting the `TokenHandleChecker` for `account_id` while the check is
  // pending will effectively cancel the previous check, and issue and newer
  // one. Destroying the previous instance of `TokenHandleChecker` will also
  // destroy the owned `GaiaOAuthClient`, therefore invalidating the weak_ptrs
  // referencing it.
  pending_checks_[account_id] = std::make_unique<TokenHandleChecker>(
      account_id, *token, url_loader_factory);

  pending_checks_[account_id]->StartCheck(base::BindOnce(
      &TokenHandleStoreImpl::OnCheckToken, weak_factory_.GetWeakPtr()));
}

void TokenHandleStoreImpl::StoreTokenHandle(const AccountId& account_id,
                                            const std::string& handle) {
  known_user_->SetStringPref(account_id, kTokenHandlePref, handle);
  known_user_->SetStringPref(account_id, kTokenHandleStatusPref,
                             kTokenHandleStatusValid);
  known_user_->SetPath(account_id, kTokenHandleLastCheckedPref,
                       base::TimeToValue(base::Time::Now()));
}

void TokenHandleStoreImpl::MaybeFetchTokenHandle(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    const AccountId& account_id,
    const std::string& access_token,
    const std::string& refresh_token_hash) {
  // If the user doesn't have a token handle (new user), or the existing token
  // handle is stale, fetch a new token.
  if (!HasToken(account_id) || IsTokenHandleStale(account_id)) {
    FetchTokenHandle(url_loader_factory, account_id, access_token,
                     refresh_token_hash);
  }
}

void TokenHandleStoreImpl::FetchTokenHandle(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    const AccountId& account_id,
    const std::string& access_token,
    const std::string& refresh_token_hash) {
  CHECK_NE(access_token, std::string());
  // Overwriting the `TokenHandleFetcher` for `account_id` while the fetch is
  // pending will effectively cancel the previous check, and issue a newer
  // one. Destroying the previous instance of `TokenHandleFetcher` will also
  // destroy the owned `GaiaOAuthClient`, therefore invalidating the weak_ptrs
  // referencing it.
  pending_fetches_[account_id] =
      std::make_unique<TokenHandleFetcher>(url_loader_factory, account_id);
  pending_fetches_[account_id]->Fetch(
      access_token, refresh_token_hash,
      base::BindOnce(&TokenHandleStoreImpl::OnFetchToken,
                     weak_factory_.GetWeakPtr()));
}

void TokenHandleStoreImpl::OnFetchToken(const AccountId& account_id,
                                        bool success,
                                        const std::string& token) {
  if (!success) {
    LOG(ERROR) << "OAuth2 token handle fetch failed.";
    return;
  }

  known_user_->SetStringPref(account_id, kTokenHandleStatusPref,
                             kTokenHandleStatusValid);
  StoreTokenHandle(account_id, token);

  // Reply to pending checks that were waiting on a new token handle to be
  // fetched, if any.
  if (pending_checks_.find(account_id) != pending_checks_.end()) {
    OnCheckToken(account_id, token,
                 /*status=*/TokenHandleChecker::Status::kValid);
  }

  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&TokenHandleStoreImpl::ScheduleFetcherDelete,
                                weak_factory_.GetWeakPtr(), account_id));
}

void TokenHandleStoreImpl::OnCheckToken(
    const AccountId& account_id,
    const std::string& token,
    const TokenHandleChecker::Status& status) {
  CHECK(pending_checks_.find(account_id) != pending_checks_.end());

  does_user_have_gaia_password_.Run(
      account_id,
      base::BindOnce(&TokenHandleStoreImpl::ReplyToTokenHandleCheck,
                     weak_factory_.GetWeakPtr(), token, account_id, status));
}

// This method maintains the following invariants:
// - A vector of token handle check callbacks, ordered such that the back of the
// vector contains the more recent requests. When the most recent request
// returns, we empty the queue and reply to all of the pending callbacks with
// its result.
// - Setting a token handle's status to stale will defer replying to all pending
// checks until we fetch a new token handle. At which point we'll reply to all
// pending callbacks that the new token is valid.
void TokenHandleStoreImpl::ReplyToTokenHandleCheck(
    const std::string& token,
    const AccountId& account_id,
    const TokenHandleChecker::Status& status,
    std::optional<bool> user_has_gaia_password) {
  if (const std::string* pref_status =
          known_user_->FindStringPath(account_id, kTokenHandleStatusPref);
      pref_status != nullptr && *pref_status == kTokenHandleStatusStale) {
    // Token handle has been marked as stale, we defer replying to checks
    // until we fetch the new token handle.
    return;
  }

  if (status != TokenHandleChecker::Status::kUnknown) {
    // Update last checked timestamp.
    known_user_->SetPath(account_id, kTokenHandleLastCheckedPref,
                         base::TimeToValue(base::Time::Now()));
  }

  bool is_reauth_required =
      ::ash::IsReauthRequired(status, user_has_gaia_password.value_or(true));

  if (is_reauth_required) {
    known_user_->SetStringPref(account_id, kTokenHandleStatusPref,
                               kTokenHandleStatusInvalid);
  }

  std::ranges::for_each(
      pending_callbacks_[account_id], [&](TokenValidationCallback& callback) {
        std::move(callback).Run(account_id, token,
                                /*reauth_required=*/is_reauth_required);
      });

  pending_callbacks_[account_id].clear();

  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&TokenHandleStoreImpl::ScheduleCheckerDelete,
                                weak_factory_.GetWeakPtr(), account_id));
}

void TokenHandleStoreImpl::ScheduleCheckerDelete(const AccountId& account_id) {
  pending_checks_.erase(account_id);
}

void TokenHandleStoreImpl::ScheduleFetcherDelete(const AccountId& account_id) {
  pending_fetches_.erase(account_id);
}

bool TokenHandleStoreImpl::HasTokenStatusInvalid(
    const AccountId& account_id) const {
  const std::string* status =
      known_user_->FindStringPath(account_id, kTokenHandleStatusPref);

  return status && *status == kTokenHandleStatusInvalid;
}

bool TokenHandleStoreImpl::IsTokenHandleStale(
    const AccountId& account_id) const {
  return *known_user_->FindStringPath(account_id, kTokenHandleStatusPref) ==
         kTokenHandleStatusStale;
}

void TokenHandleStoreImpl::SetInvalidTokenForTesting(const char* token) {
  if (!token) {
    invalid_token_for_testing_->clear();
    return;
  }
  invalid_token_for_testing_ = token;
}

void TokenHandleStoreImpl::SetLastCheckedPrefForTesting(
    const AccountId& account_id,
    base::Time time) {}

}  // namespace ash
