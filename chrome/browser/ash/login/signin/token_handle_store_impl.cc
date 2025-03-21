// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/signin/token_handle_store_impl.h"

#include "base/json/values_util.h"
#include "base/no_destructor.h"
#include "chrome/browser/browser_process.h"
#include "components/account_id/account_id.h"

namespace ash {

namespace {

constexpr char kTokenHandlePref[] = "PasswordTokenHandle";
constexpr char kTokenHandleStatusPref[] = "TokenHandleStatus";
constexpr char kTokenHandleStatusInvalid[] = "invalid";

}  // namespace

TokenHandleStoreImpl::TokenHandleStoreImpl(
    std::unique_ptr<user_manager::KnownUser> known_user)
    : known_user_(std::move(known_user)) {}

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
  // TODO(emaamari): implement.
  return true;
}

void TokenHandleStoreImpl::IsReauthRequired(
    const AccountId& account_id,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    TokenValidationCallback callback) {}

void TokenHandleStoreImpl::StoreTokenHandle(const AccountId& account_id,
                                            const std::string& handle) {}

bool TokenHandleStoreImpl::HasTokenStatusInvalid(
    const AccountId& account_id) const {
  const std::string* status =
      known_user_->FindStringPath(account_id, kTokenHandleStatusPref);

  return status && *status == kTokenHandleStatusInvalid;
}

void TokenHandleStoreImpl::SetInvalidTokenForTesting(const char* token) {}

void TokenHandleStoreImpl::SetLastCheckedPrefForTesting(
    const AccountId& account_id,
    base::Time time) {}

}  // namespace ash
