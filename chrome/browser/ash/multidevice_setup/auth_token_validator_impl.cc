// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/multidevice_setup/auth_token_validator_impl.h"

#include "chrome/browser/ash/login/quick_unlock/auth_token.h"
#include "chromeos/ash/components/osauth/public/auth_session_storage.h"

namespace ash {
namespace multidevice_setup {

AuthTokenValidatorImpl::AuthTokenValidatorImpl() = default;

AuthTokenValidatorImpl::~AuthTokenValidatorImpl() = default;

bool AuthTokenValidatorImpl::IsAuthTokenValid(const std::string& auth_token) {
  return ash::AuthSessionStorage::Get()->IsValid(auth_token);
}

}  // namespace multidevice_setup
}  // namespace ash
