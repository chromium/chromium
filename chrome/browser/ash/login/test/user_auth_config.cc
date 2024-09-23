// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/test/user_auth_config.h"

#include <initializer_list>
#include <string>

#include "base/check_op.h"
#include "base/notreached.h"
#include "chromeos/ash/components/osauth/public/common_types.h"

namespace ash::test {

UserAuthConfig::UserAuthConfig() = default;
UserAuthConfig::UserAuthConfig(const UserAuthConfig& other) = default;
UserAuthConfig::~UserAuthConfig() = default;

// static

UserAuthConfig UserAuthConfig::Create(
    std::initializer_list<ash::AshAuthFactor> factors_list) {
  CHECK_NE(factors_list.size(), 0u)
      << "Existing users should have at least one AuthFactor";
  AuthFactorsSet factors{factors_list};
  UserAuthConfig result;
  if (factors.Has(ash::AshAuthFactor::kGaiaPassword)) {
    CHECK(!factors.Has(ash::AshAuthFactor::kLocalPassword));
    result.WithOnlinePassword(kGaiaPassword);
  }
  if (factors.Has(ash::AshAuthFactor::kLocalPassword)) {
    CHECK(!factors.Has(ash::AshAuthFactor::kGaiaPassword));
    result.WithLocalPassword(kLocalPassword);
  }
  if (factors.Has(ash::AshAuthFactor::kCryptohomePin)) {
    result.WithCryptohomePin(kAuthPin, kPinStubSalt);
  }
  if (factors.Has(ash::AshAuthFactor::kRecovery)) {
    result.WithRecoveryFactor();
  }
  if (factors.Has(ash::AshAuthFactor::kSmartCard) ||
      factors.Has(ash::AshAuthFactor::kSmartUnlock) ||
      factors.Has(ash::AshAuthFactor::kLegacyPin) ||
      factors.Has(ash::AshAuthFactor::kLegacyFingerprint)) {
    NOTREACHED_IN_MIGRATION()
        << "These factors are not supported by test mixins yet";
  }
  return result;
}

UserAuthConfig& UserAuthConfig::WithLocalPassword(const std::string& password) {
  CHECK(!password.empty());
  factors.Put(ash::AshAuthFactor::kLocalPassword);
  local_password = password;
  return *this;
}

UserAuthConfig& UserAuthConfig::WithOnlinePassword(
    const std::string& password) {
  CHECK(!password.empty());
  factors.Put(ash::AshAuthFactor::kGaiaPassword);
  online_password = password;
  return *this;
}

UserAuthConfig& UserAuthConfig::WithCryptohomePin(const std::string& the_pin,
                                                  const std::string& salt) {
  CHECK(!the_pin.empty());
  factors.Put(ash::AshAuthFactor::kCryptohomePin);
  pin = the_pin;
  pin_salt = salt;
  return *this;
}

UserAuthConfig& UserAuthConfig::WithRecoveryFactor() {
  factors.Put(ash::AshAuthFactor::kRecovery);
  return *this;
}

UserAuthConfig& UserAuthConfig::RequireReauth(bool require_reauth /*=true*/) {
  if (require_reauth) {
    token_status =
        user_manager::User::OAuthTokenStatus::OAUTH2_TOKEN_STATUS_INVALID;
  } else {
    token_status =
        user_manager::User::OAuthTokenStatus::OAUTH2_TOKEN_STATUS_VALID;
  }
  return *this;
}

}  // namespace ash::test
