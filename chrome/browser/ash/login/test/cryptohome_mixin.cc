// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/test/cryptohome_mixin.h"

#include "chrome/browser/ash/login/test/login_manager_mixin.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"
#include "chromeos/ash/components/cryptohome/auth_factor.h"
#include "chromeos/ash/components/cryptohome/cryptohome_parameters.h"
#include "chromeos/ash/components/cryptohome/system_salt_getter.h"
#include "chromeos/ash/components/dbus/cryptohome/rpc.pb.h"
#include "chromeos/ash/components/dbus/userdataauth/fake_cryptohome_misc_client.h"
#include "chromeos/ash/components/dbus/userdataauth/fake_userdataauth_client.h"
#include "chromeos/ash/components/login/auth/public/cryptohome_key_constants.h"
#include "components/account_id/account_id.h"

namespace ash {

CryptohomeMixin::CryptohomeMixin(InProcessBrowserTestMixinHost* host)
    : InProcessBrowserTestMixin(host) {}

CryptohomeMixin::~CryptohomeMixin() = default;

void CryptohomeMixin::MarkUserAsExisting(const AccountId& user) {
  auto account_id = cryptohome::CreateAccountIdentifierFromAccountId(user);
  if (FakeUserDataAuthClient::TestApi::Get()) {
    FakeUserDataAuthClient::TestApi::Get()->AddExistingUser(
        std::move(account_id));
  } else {
    pending_users_.emplace(account_id);
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

  // Add the password key to the user.
  cryptohome::Key cryptohome_key;
  cryptohome_key.mutable_data()->set_label(kCryptohomeGaiaKeyLabel);
  cryptohome_key.set_secret(key.GetSecret());
  FakeUserDataAuthClient::TestApi::Get()->AddKey(account_identifier,
                                                 cryptohome_key);
}

void CryptohomeMixin::SetUpOnMainThread() {
  while (!pending_users_.empty()) {
    auto user = pending_users_.front();
    FakeUserDataAuthClient::TestApi::Get()->AddExistingUser(std::move(user));
    pending_users_.pop();
  }
}

}  // namespace ash
