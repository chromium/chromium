// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/signin/token_handle_util.h"

#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_macros.h"
#include "base/values.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "components/user_manager/known_user.h"
#include "google_apis/gaia/gaia_oauth_client.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace {

const char kTokenHandlePref[] = "PasswordTokenHandle";
const char kTokenHandleStatusPref[] = "TokenHandleStatus";

const char kHandleStatusValid[] = "valid";
const char kHandleStatusInvalid[] = "invalid";
const char* const kDefaultHandleStatus = kHandleStatusValid;

constexpr int kMaxRetries = 3;

}  // namespace

TokenHandleUtil::TokenHandleUtil() : weak_factory_(this) {}

TokenHandleUtil::~TokenHandleUtil() {
  weak_factory_.InvalidateWeakPtrs();
  gaia_client_.reset();
}

bool TokenHandleUtil::HasToken(const AccountId& account_id) {
  const base::DictionaryValue* dict = nullptr;
  std::string token;
  if (!user_manager::known_user::FindPrefs(account_id, &dict))
    return false;
  if (!dict->GetString(kTokenHandlePref, &token))
    return false;
  return !token.empty();
}

bool TokenHandleUtil::ShouldObtainHandle(const AccountId& account_id) {
  const base::DictionaryValue* dict = nullptr;
  std::string token;
  if (!user_manager::known_user::FindPrefs(account_id, &dict))
    return true;
  if (!dict->GetString(kTokenHandlePref, &token))
    return true;
  if (token.empty())
    return true;
  std::string status(kDefaultHandleStatus);
  dict->GetString(kTokenHandleStatusPref, &status);
  return kHandleStatusInvalid == status;
}

void TokenHandleUtil::DeleteHandle(const AccountId& account_id) {
  const base::DictionaryValue* dict = nullptr;
  if (!user_manager::known_user::FindPrefs(account_id, &dict))
    return;
  std::unique_ptr<base::DictionaryValue> dict_copy(dict->DeepCopy());
  dict_copy->Remove(kTokenHandlePref, nullptr);
  dict_copy->Remove(kTokenHandleStatusPref, nullptr);
  user_manager::known_user::UpdatePrefs(account_id, *dict_copy.get(),
                                        /* replace values */ true);
}

void TokenHandleUtil::MarkHandleInvalid(const AccountId& account_id) {
  user_manager::known_user::SetStringPref(account_id, kTokenHandleStatusPref,
                                          kHandleStatusInvalid);
}

void TokenHandleUtil::CheckToken(const AccountId& account_id,
                                 const TokenValidationCallback& callback) {
  const base::DictionaryValue* dict = nullptr;
  std::string token;
  if (!user_manager::known_user::FindPrefs(account_id, &dict)) {
    callback.Run(account_id, UNKNOWN);
    return;
  }
  if (!dict->GetString(kTokenHandlePref, &token)) {
    callback.Run(account_id, UNKNOWN);
    return;
  }

  if (!gaia_client_.get()) {
    auto url_loader_factory = chromeos::ProfileHelper::Get()
                                  ->GetSigninProfile()
                                  ->GetURLLoaderFactory();
    gaia_client_.reset(
        new gaia::GaiaOAuthClient(std::move(url_loader_factory)));
  }

  validation_delegates_[token] =
      std::unique_ptr<TokenDelegate>(new TokenDelegate(
          weak_factory_.GetWeakPtr(), account_id, token, callback));
  gaia_client_->GetTokenHandleInfo(token, kMaxRetries,
                                   validation_delegates_[token].get());
}

void TokenHandleUtil::StoreTokenHandle(const AccountId& account_id,
                                       const std::string& handle) {
  user_manager::known_user::SetStringPref(account_id, kTokenHandlePref, handle);
  user_manager::known_user::SetStringPref(account_id, kTokenHandleStatusPref,
                                          kHandleStatusValid);
}

void TokenHandleUtil::OnValidationComplete(const std::string& token) {
  validation_delegates_.erase(token);
}

TokenHandleUtil::TokenDelegate::TokenDelegate(
    const base::WeakPtr<TokenHandleUtil>& owner,
    const AccountId& account_id,
    const std::string& token,
    const TokenValidationCallback& callback)
    : owner_(owner),
      account_id_(account_id),
      token_(token),
      tokeninfo_response_start_time_(base::TimeTicks::Now()),
      callback_(callback) {}

TokenHandleUtil::TokenDelegate::~TokenDelegate() {}

void TokenHandleUtil::TokenDelegate::OnOAuthError() {
  callback_.Run(account_id_, INVALID);
  NotifyDone();
}

// Warning: NotifyDone() deletes |this|
void TokenHandleUtil::TokenDelegate::NotifyDone() {
  if (owner_)
    owner_->OnValidationComplete(token_);
}

void TokenHandleUtil::TokenDelegate::OnNetworkError(int response_code) {
  callback_.Run(account_id_, UNKNOWN);
  NotifyDone();
}

void TokenHandleUtil::TokenDelegate::OnGetTokenInfoResponse(
    std::unique_ptr<base::DictionaryValue> token_info) {
  TokenHandleStatus outcome = UNKNOWN;
  if (!token_info->HasKey("error")) {
    int expires_in = 0;
    if (token_info->GetInteger("expires_in", &expires_in))
      outcome = (expires_in < 0) ? INVALID : VALID;
  }

  const base::TimeDelta duration =
      base::TimeTicks::Now() - tokeninfo_response_start_time_;
  UMA_HISTOGRAM_TIMES("Login.TokenCheckResponseTime", duration);
  callback_.Run(account_id_, outcome);
  NotifyDone();
}
