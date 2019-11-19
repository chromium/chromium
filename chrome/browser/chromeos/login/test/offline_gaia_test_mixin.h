// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_TEST_OFFLINE_GAIA_TEST_MIXIN_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_TEST_OFFLINE_GAIA_TEST_MIXIN_H_

#include <memory>
#include <string>

#include "chrome/test/base/mixin_based_in_process_browser_test.h"

class AccountId;

namespace chromeos {

class NetworkStateTestHelper;

// This object sets offline Gaia login mode on the login screen.
class OfflineGaiaTestMixin : public InProcessBrowserTestMixin {
 public:
  explicit OfflineGaiaTestMixin(InProcessBrowserTestMixinHost* host);
  ~OfflineGaiaTestMixin() override;

  // InProcessBrowserTestMixin:
  void SetUpOnMainThread() override;
  void TearDownOnMainThread() override;

  // Initializes DeviceSettingsProvider to start with OfflineGaia on the next
  // start. Should be called from the PRE_ test.
  void PrepareOfflineGaiaLogin();

  // Go Offline/Online forces test to be offline and switches back to online.
  void GoOffline();
  void GoOnline();

  // Loads offline GAIA page and signs-in.
  // Expects user to be already registered (probably via LoginManagerMixin).
  // Expects networking to be disabled (probably by caling GoOffline())
  void SignIn(const AccountId& test_account_id, const std::string& password);

 private:
  // Initializes UserManager for offline Login.
  void InitOfflineLogin(const AccountId& test_account_id,
                        const std::string& password);

  // Triggers Offlige Gaia screen.
  void StartGaiaAuthOffline();

  // Fills in and submits offline GAIA login.
  void SubmitGaiaAuthOfflineForm(const std::string& user_email,
                                 const std::string& password);

  // This is ised to disable networking.
  std::unique_ptr<chromeos::NetworkStateTestHelper> network_state_test_helper_;

  DISALLOW_COPY_AND_ASSIGN(OfflineGaiaTestMixin);
};

}  // namespace chromeos
#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_TEST_OFFLINE_GAIA_TEST_MIXIN_H_
