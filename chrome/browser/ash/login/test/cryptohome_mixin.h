// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_TEST_CRYPTOHOME_MIXIN_H_
#define CHROME_BROWSER_ASH_LOGIN_TEST_CRYPTOHOME_MIXIN_H_

#include <queue>

#include "base/gtest_prod_util.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"
#include "chromeos/ash/components/dbus/cryptohome/rpc.pb.h"
#include "chromeos/ash/components/dbus/userdataauth/fake_userdataauth_client.h"
#include "components/account_id/account_id.h"

namespace ash {

// Mixin that acts as a broker between tests
// and FakeUserDataAuthClient, handling all interactions and transformations
class CryptohomeMixin : public InProcessBrowserTestMixin,
                        public ash::FakeUserDataAuthClient::TestApi {
 public:
  explicit CryptohomeMixin(InProcessBrowserTestMixinHost* host);
  CryptohomeMixin(const CryptohomeMixin&) = delete;
  CryptohomeMixin& operator=(const CryptohomeMixin&) = delete;
  ~CryptohomeMixin() override;

  // InProcessBrowserTestMixin
  void SetUpOnMainThread() override;

  void MarkUserAsExisting(const AccountId& user);
  void AddGaiaPassword(const AccountId& user, std::string password);

 private:
  FRIEND_TEST_ALL_PREFIXES(CryptohomeMixinTest,
                           PoolUsersWhenUserDataAuthClientIsNull);
  FRIEND_TEST_ALL_PREFIXES(CryptohomeMixinTest,
                           UserDataAuthClientCalledWhenAvailable);

  std::queue<cryptohome::AccountIdentifier> pending_users_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_TEST_CRYPTOHOME_MIXIN_H_
