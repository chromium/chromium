// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/multidevice_setup/auth_token_validator_impl.h"

#include "chrome/browser/chromeos/login/quick_unlock/auth_token.h"
#include "chrome/browser/chromeos/login/quick_unlock/quick_unlock_factory.h"
#include "chrome/browser/chromeos/login/quick_unlock/quick_unlock_storage.h"

namespace chromeos {

namespace multidevice_setup {

AuthTokenValidatorImpl::AuthTokenValidatorImpl(
    quick_unlock::QuickUnlockStorage* quick_unlock_storage)
    : quick_unlock_storage_(quick_unlock_storage) {}

AuthTokenValidatorImpl::~AuthTokenValidatorImpl() = default;

bool AuthTokenValidatorImpl::IsAuthTokenValid(const std::string& auth_token) {
  return quick_unlock_storage_ && quick_unlock_storage_->GetAuthToken() &&
         auth_token == quick_unlock_storage_->GetAuthToken()->Identifier();
}

void AuthTokenValidatorImpl::Shutdown() {
  quick_unlock_storage_ = nullptr;
}

}  // namespace multidevice_setup

}  // namespace chromeos
