// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_TEST_KIOSK_APP_LOGGED_IN_BROWSER_TEST_MIXIN_H_
#define CHROME_BROWSER_ASH_TEST_KIOSK_APP_LOGGED_IN_BROWSER_TEST_MIXIN_H_

#include <string>

#include "chrome/test/base/mixin_based_in_process_browser_test.h"

namespace ash {

// Registers a new KioskApp User, and then use the user to enter
// into a kiosk app session.
class KioskAppLoggedInBrowserTestMixin : public InProcessBrowserTestMixin {
 public:
  // `account_id` is the ID for the KioskApp account.
  // AccountId (to be used by, e.g., UserManager) is generated from the
  // `account_id`.
  KioskAppLoggedInBrowserTestMixin(InProcessBrowserTestMixinHost* host,
                                   std::string account_id);
  ~KioskAppLoggedInBrowserTestMixin() override;

  // InProcessBrowserTestMixin:
  void SetUpCommandLine(base::CommandLine* command_line) override;
  void SetUpLocalStatePrefService(PrefService* local_state) override;

 private:
  // Account ID passed to the constructor, which is the one used in the policy.
  const std::string account_id_;

  // Email-style ID used as a part of AccountId for the given User,
  // generated from the `account_id` passed to the ctor.
  const std::string user_id_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_TEST_KIOSK_APP_LOGGED_IN_BROWSER_TEST_MIXIN_H_
