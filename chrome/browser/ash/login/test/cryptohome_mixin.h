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

// Mixin that acts as a broker between tests and FakeUserDataAuthClient,
// handling all interactions and transformations.
class CryptohomeMixin : public InProcessBrowserTestMixin,
                        public FakeUserDataAuthClient::TestApi {
 public:
  explicit CryptohomeMixin(InProcessBrowserTestMixinHost* host);
  CryptohomeMixin(const CryptohomeMixin&) = delete;
  CryptohomeMixin& operator=(const CryptohomeMixin&) = delete;
  ~CryptohomeMixin() override;

  void SetUpOnMainThread() override;

  void MarkUserAsExisting(const AccountId& user);
  std::string AddSession(const AccountId& user, bool authenticated);
  void AddGaiaPassword(const AccountId& user, std::string password);
  void SetPinLocked(const AccountId& user, bool locked);
  bool HasPinFactor(const AccountId& user);
  void AddRecoveryFactor(const AccountId& user);
  bool HasRecoveryFactor(const AccountId& user);

  void SendLegacyFingerprintSuccessScan();
  void SendLegacyFingerprintFailureScan();
  void SendLegacyFingerprintFailureLockoutScan();
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_TEST_CRYPTOHOME_MIXIN_H_
