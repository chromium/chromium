// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/test/cryptohome_mixin.h"

#include "ash/constants/ash_pref_names.h"
#include "ash/shell.h"
#include "chrome/browser/ash/login/test/login_manager_mixin.h"
#include "chrome/browser/ash/login/test/user_auth_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"
#include "chromeos/ash/components/cryptohome/auth_factor.h"
#include "chromeos/ash/components/cryptohome/cryptohome_parameters.h"
#include "chromeos/ash/components/cryptohome/system_salt_getter.h"
#include "chromeos/ash/components/dbus/cryptohome/rpc.pb.h"
#include "chromeos/ash/components/dbus/userdataauth/fake_cryptohome_misc_client.h"
#include "chromeos/ash/components/dbus/userdataauth/fake_userdataauth_client.h"
#include "chromeos/ash/components/login/auth/public/cryptohome_key_constants.h"
#include "components/account_id/account_id.h"
#include "components/user_manager/known_user.h"

namespace ash {

CryptohomeMixin::CryptohomeMixin(InProcessBrowserTestMixinHost* host)
    : InProcessBrowserTestMixin(host) {}

CryptohomeMixin::~CryptohomeMixin() = default;

void CryptohomeMixin::SetUpOnMainThread() {
  FakeUserDataAuthClient::TestApi::Get()->CreatePostponedDirectories();
}

void CryptohomeMixin::MarkUserAsExisting(const AccountId& user) {
  auto account_id = cryptohome::CreateAccountIdentifierFromAccountId(user);
  FakeUserDataAuthClient::TestApi::Get()->AddExistingUser(
      std::move(account_id));
}

std::pair<std::string, std::string> CryptohomeMixin::AddSession(
    const AccountId& user,
    bool authenticated) {
  auto account_id = cryptohome::CreateAccountIdentifierFromAccountId(user);
  return FakeUserDataAuthClient::TestApi::Get()->AddSession(
      std::move(account_id), authenticated);
}

void CryptohomeMixin::ApplyAuthConfig(const AccountId& user,
                                      const test::UserAuthConfig& config) {
  if (config.factors.Has(ash::AshAuthFactor::kGaiaPassword)) {
    AddGaiaPassword(user, config.online_password);
  }
  if (config.factors.Has(ash::AshAuthFactor::kLocalPassword)) {
    AddLocalPassword(user, config.local_password);
  }
  if (config.factors.Has(ash::AshAuthFactor::kCryptohomePin)) {
    AddCryptohomePin(user, config.pin, config.pin_salt);
  }
  if (config.factors.Has(ash::AshAuthFactor::kRecovery)) {
    AddRecoveryFactor(user);
  }
}

void CryptohomeMixin::ApplyAuthConfigIfUserExists(
    const AccountId& user,
    const test::UserAuthConfig& config) {
  user_manager::KnownUser known_user(g_browser_process->local_state());
  if (known_user.UserExists(user)) {
    ApplyAuthConfig(user, config);
  }
}

void CryptohomeMixin::AddGaiaPassword(const AccountId& user,
                                      std::string password) {
  auto account_identifier =
      cryptohome::CreateAccountIdentifierFromAccountId(user);

  // Hash the password, as only hashed passwords appear at the userdataauth
  // level.
  Key key(std::move(password));
  key.Transform(Key::KEY_TYPE_SALTED_SHA256_TOP_HALF,
                SystemSaltGetter::ConvertRawSaltToHexString(
                    FakeCryptohomeMiscClient::GetStubSystemSalt()));

  user_data_auth::AuthFactor auth_factor;
  user_data_auth::AuthInput auth_input;

  auth_factor.set_label(ash::kCryptohomeGaiaKeyLabel);
  auth_factor.set_type(user_data_auth::AUTH_FACTOR_TYPE_PASSWORD);

  auth_input.mutable_password_input()->set_secret(key.GetSecret());

  // Add the password key to the user.
  FakeUserDataAuthClient::TestApi::Get()->AddAuthFactor(
      account_identifier, auth_factor, auth_input);
}

void CryptohomeMixin::AddLocalPassword(const AccountId& user,
                                       std::string password) {
  auto account_identifier =
      cryptohome::CreateAccountIdentifierFromAccountId(user);

  // Hash the password, as only hashed passwords appear at the userdataauth
  // level.
  Key key(std::move(password));
  key.Transform(Key::KEY_TYPE_SALTED_SHA256_TOP_HALF,
                SystemSaltGetter::ConvertRawSaltToHexString(
                    FakeCryptohomeMiscClient::GetStubSystemSalt()));

  user_data_auth::AuthFactor auth_factor;
  user_data_auth::AuthInput auth_input;

  auth_factor.set_label(ash::kCryptohomeLocalPasswordKeyLabel);
  auth_factor.set_type(user_data_auth::AUTH_FACTOR_TYPE_PASSWORD);

  auth_input.mutable_password_input()->set_secret(key.GetSecret());

  // Add the password key to the user.
  FakeUserDataAuthClient::TestApi::Get()->AddAuthFactor(
      account_identifier, auth_factor, auth_input);
}

void CryptohomeMixin::AddCryptohomePin(const AccountId& user,
                                       const std::string& pin,
                                       const std::string& pin_salt) {
  auto account_identifier =
      cryptohome::CreateAccountIdentifierFromAccountId(user);

  user_manager::KnownUser known_user(Shell::Get()->local_state());
  known_user.SetStringPref(user, prefs::kQuickUnlockPinSalt, pin_salt);

  // Hash the pin, as only hashed secrets appear at the userdataauth
  // level.
  Key key(std::move(pin));
  key.Transform(Key::KEY_TYPE_SALTED_SHA256_TOP_HALF, pin_salt);

  // Add the pin key to the user.
  user_data_auth::AuthFactor auth_factor;
  user_data_auth::AuthInput auth_input;

  auth_factor.set_label(ash::kCryptohomePinLabel);
  auth_factor.set_type(user_data_auth::AUTH_FACTOR_TYPE_PIN);

  auth_input.mutable_password_input()->set_secret(key.GetSecret());

  // Add the password key to the user.
  FakeUserDataAuthClient::TestApi::Get()->AddAuthFactor(
      account_identifier, auth_factor, auth_input);
}

void CryptohomeMixin::SetPinLocked(const AccountId& user, bool locked) {
  return TestApi::SetPinLocked(
      cryptohome::CreateAccountIdentifierFromAccountId(user),
      kCryptohomePinLabel, locked);
}

bool CryptohomeMixin::HasPinFactor(const AccountId& user) {
  return TestApi::HasPinFactor(
      cryptohome::CreateAccountIdentifierFromAccountId(user));
}

void CryptohomeMixin::AddRecoveryFactor(const AccountId& user) {
  return TestApi::AddRecoveryFactor(
      cryptohome::CreateAccountIdentifierFromAccountId(user));
}

bool CryptohomeMixin::HasRecoveryFactor(const AccountId& user) {
  return TestApi::HasRecoveryFactor(
      cryptohome::CreateAccountIdentifierFromAccountId(user));
}

void CryptohomeMixin::SendLegacyFingerprintSuccessScan() {
  CHECK(FakeUserDataAuthClient::TestApi::Get());
  FakeUserDataAuthClient::TestApi::Get()->SendLegacyFPAuthSignal(
      user_data_auth::FingerprintScanResult::FINGERPRINT_SCAN_RESULT_SUCCESS);
}

void CryptohomeMixin::SendLegacyFingerprintFailureScan() {
  CHECK(FakeUserDataAuthClient::TestApi::Get());
  FakeUserDataAuthClient::TestApi::Get()->SendLegacyFPAuthSignal(
      user_data_auth::FingerprintScanResult::FINGERPRINT_SCAN_RESULT_RETRY);
}

void CryptohomeMixin::SendLegacyFingerprintFailureLockoutScan() {
  CHECK(FakeUserDataAuthClient::TestApi::Get());
  FakeUserDataAuthClient::TestApi::Get()->SendLegacyFPAuthSignal(
      user_data_auth::FingerprintScanResult::FINGERPRINT_SCAN_RESULT_LOCKOUT);
}

bool CryptohomeMixin::IsAuthenticated(const AccountId& user) {
  CHECK(FakeUserDataAuthClient::TestApi::Get());
  return FakeUserDataAuthClient::TestApi::Get()->IsAuthenticated(
      cryptohome::CreateAccountIdentifierFromAccountId(user));
}

}  // namespace ash
