// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_TEST_USER_AUTH_CONFIG_H_
#define CHROME_BROWSER_ASH_LOGIN_TEST_USER_AUTH_CONFIG_H_

#include <initializer_list>
#include <string>

#include "chromeos/ash/components/osauth/public/common_types.h"
#include "components/user_manager/user_manager.h"

namespace ash {
namespace test {

inline constexpr std::initializer_list<ash::AshAuthFactor> kDefaultAuthSetup = {
    ash::AshAuthFactor::kGaiaPassword};

inline constexpr char kGaiaPassword[] = "gaiaPassword";
// Use this constant in the flows where password have to be updated.
inline constexpr char kNewPassword[] = "newPassword";
inline constexpr char kLocalPassword[] = "localPassword";
inline constexpr char kWrongPassword[] = "wrongPassword";
inline constexpr char kAuthPin[] = "123456";
inline constexpr char kPinStubSalt[] = "pin-salt";

// Data class that defines configuration of the auth factors for the user.
// Used in conjunction with `LoginManagerMixin` / `CryptohomeMixin`.
struct UserAuthConfig {
 public:
  UserAuthConfig();
  UserAuthConfig(const UserAuthConfig& other);
  ~UserAuthConfig();

  static UserAuthConfig Create(
      std::initializer_list<ash::AshAuthFactor> factors);

  UserAuthConfig& WithOnlinePassword(const std::string& online_password);
  UserAuthConfig& WithLocalPassword(const std::string& local_password);
  UserAuthConfig& WithCryptohomePin(const std::string& pin,
                                    const std::string& salt);
  UserAuthConfig& WithLegacyPin(const std::string& pin,
                                const std::string& pin_salt);
  UserAuthConfig& WithRecoveryFactor();
  UserAuthConfig& RequireReauth(bool require_reauth = true);

  AuthFactorsSet factors;
  std::string online_password;
  std::string local_password;
  std::string pin;
  std::string pin_salt;

  user_manager::User::OAuthTokenStatus token_status =
      user_manager::User::OAuthTokenStatus::OAUTH2_TOKEN_STATUS_VALID;
};

}  // namespace test
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_TEST_USER_AUTH_CONFIG_H_
