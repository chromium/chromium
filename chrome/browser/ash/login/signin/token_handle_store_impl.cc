// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/signin/token_handle_store_impl.h"

#include "base/no_destructor.h"
#include "components/account_id/account_id.h"

namespace ash {

// static
TokenHandleStoreImpl* TokenHandleStoreImpl::Get() {
  static base::NoDestructor<TokenHandleStoreImpl> instance;
  return instance.get();
}

TokenHandleStoreImpl::TokenHandleStoreImpl() = default;
TokenHandleStoreImpl::~TokenHandleStoreImpl() = default;

bool TokenHandleStoreImpl::HasToken(const AccountId& account_id) const {
  // TODO(emaamari): implement.
  return true;
}

bool TokenHandleStoreImpl::ShouldObtainHandle(
    const AccountId& account_id) const {
  // TODO(emaamari): implement.
  return true;
}

bool TokenHandleStoreImpl::IsRecentlyChecked(
    const AccountId& account_id) const {
  // TODO(emaamar): implement.
  return true;
}

void TokenHandleStoreImpl::IsReauthRequired(
    const AccountId& account_id,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    TokenValidationCallback callback) {}

void TokenHandleStoreImpl::StoreTokenHandle(const AccountId& account_id,
                                            const std::string& handle) {}

void TokenHandleStoreImpl::SetInvalidTokenForTesting(const char* token) {}

void TokenHandleStoreImpl::SetLastCheckedPrefForTesting(
    const AccountId& account_id,
    base::Time time) {}

}  // namespace ash
