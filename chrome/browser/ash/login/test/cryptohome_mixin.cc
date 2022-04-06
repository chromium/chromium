// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/test/cryptohome_mixin.h"

#include "ash/components/cryptohome/cryptohome_parameters.h"
#include "chrome/browser/ash/login/test/login_manager_mixin.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"
#include "chromeos/dbus/cryptohome/rpc.pb.h"
#include "chromeos/dbus/userdataauth/fake_userdataauth_client.h"
#include "components/account_id/account_id.h"

namespace ash {

CryptohomeMixin::CryptohomeMixin(InProcessBrowserTestMixinHost* host)
    : InProcessBrowserTestMixin(host) {}

CryptohomeMixin::~CryptohomeMixin() = default;

void CryptohomeMixin::MarkUserAsExisting(const AccountId& user) {
  auto account_id = cryptohome::CreateAccountIdentifierFromAccountId(user);
  if (chromeos::FakeUserDataAuthClient::Get() != nullptr) {
    chromeos::FakeUserDataAuthClient::Get()->AddExistingUser(account_id);
  } else {
    pending_users_.emplace(account_id);
  }
}

void CryptohomeMixin::SetUpOnMainThread() {
  while (!pending_users_.empty()) {
    auto user = pending_users_.front();
    chromeos::FakeUserDataAuthClient::Get()->AddExistingUser(user);
    chromeos::FakeUserDataAuthClient::Get()->CreateUserProfileDir(user);
    pending_users_.pop();
  }
}

}  // namespace ash
