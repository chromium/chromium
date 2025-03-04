// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_TEST_PUBLIC_ACCOUNT_LOGGED_IN_BROWSER_TEST_MIXIN_H_
#define CHROME_BROWSER_ASH_TEST_PUBLIC_ACCOUNT_LOGGED_IN_BROWSER_TEST_MIXIN_H_

#include <string>

#include "chrome/test/base/mixin_based_in_process_browser_test.h"

namespace ash {

// Registers a new PublicAccount User, and then use the user to enter
// into a session.
class PublicAccountLoggedInBrowserTestMixin : public InProcessBrowserTestMixin {
 public:
  // `user_id` is the ID for the PublicAccount user, which must satisfy
  // to be used for a PublicAccount user.
  // See policy::GetDeviceLocalAccountType for details.
  PublicAccountLoggedInBrowserTestMixin(InProcessBrowserTestMixinHost* host,
                                        std::string user_id);
  ~PublicAccountLoggedInBrowserTestMixin() override;

  // InProcessBrowserTestMixin:
  void SetUpCommandLine(base::CommandLine* command_line) override;
  void SetUpLocalStatePrefService(PrefService* local_state) override;

 private:
  const std::string user_id_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_TEST_PUBLIC_ACCOUNT_LOGGED_IN_BROWSER_TEST_MIXIN_H_
