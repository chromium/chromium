// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_TEST_CHILD_LOGGED_IN_BROWSER_TEST_MIXIN_H_
#define CHROME_BROWSER_ASH_TEST_CHILD_LOGGED_IN_BROWSER_TEST_MIXIN_H_

#include "chrome/test/base/mixin_based_in_process_browser_test.h"
#include "components/account_id/account_id.h"

namespace ash {

// Sets up child user logged-in situation, similar to browser restart for
// the crash recovery.
class ChildLoggedInBrowserTestMixin : public InProcessBrowserTestMixin {
 public:
  ChildLoggedInBrowserTestMixin(InProcessBrowserTestMixinHost* host,
                                const AccountId& account_id);
  ~ChildLoggedInBrowserTestMixin() override;

  // InProcessBrowserTestMixin:
  void SetUpCommandLine(base::CommandLine* command_line) override;
  void SetUpLocalStatePrefService(PrefService* local_state) override;

 private:
  const AccountId account_id_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_TEST_CHILD_LOGGED_IN_BROWSER_TEST_MIXIN_H_
