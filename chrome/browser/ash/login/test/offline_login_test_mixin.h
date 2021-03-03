// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_TEST_OFFLINE_LOGIN_TEST_MIXIN_H_
#define CHROME_BROWSER_ASH_LOGIN_TEST_OFFLINE_LOGIN_TEST_MIXIN_H_

#include <memory>
#include <string>

#include "chrome/test/base/mixin_based_in_process_browser_test.h"

class AccountId;

namespace chromeos {

class NetworkStateTestHelper;

// This object sets offline login mode on the login screen.
class OfflineLoginTestMixin : public InProcessBrowserTestMixin {
 public:
  explicit OfflineLoginTestMixin(InProcessBrowserTestMixinHost* host);
  ~OfflineLoginTestMixin() override;

  // InProcessBrowserTestMixin:
  void SetUpOnMainThread() override;
  void TearDownOnMainThread() override;

  // Initializes DeviceSettingsProvider to start with OfflineLogin on the next
  // start. Should be called from the PRE_ test.
  void PrepareOfflineLogin();

  // Go Offline/Online forces test to be offline and switches back to online.
  void GoOffline();
  void GoOnline();

  // Checks if UI displays management notification.
  void CheckManagedStatus(bool expected_is_managed);

  // Initializes UserManager for offline Login.
  // Expects networking to be disabled (probably by calling GoOffline())
  void InitOfflineLogin(const AccountId& test_account_id,
                        const std::string& password);

  // Fills in and submits offline login.
  // Expects user to be already registered (probably via LoginManagerMixin).
  // Optionally waits for sign-in to complete.
  void SubmitLoginAuthOfflineForm(const std::string& user_email,
                                  const std::string& password,
                                  bool wait_for_signin);

 private:
  // Triggers Offline Login screen.
  void StartLoginAuthOffline();

  // This is ised to disable networking.
  std::unique_ptr<chromeos::NetworkStateTestHelper> network_state_test_helper_;

  DISALLOW_COPY_AND_ASSIGN(OfflineLoginTestMixin);
};

}  // namespace chromeos
#endif  // CHROME_BROWSER_ASH_LOGIN_TEST_OFFLINE_LOGIN_TEST_MIXIN_H_
