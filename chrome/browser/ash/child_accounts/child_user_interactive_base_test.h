// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CHILD_ACCOUNTS_CHILD_USER_INTERACTIVE_BASE_TEST_H_
#define CHROME_BROWSER_ASH_CHILD_ACCOUNTS_CHILD_USER_INTERACTIVE_BASE_TEST_H_

#include "chrome/browser/ash/login/test/logged_in_user_mixin.h"
#include "chrome/test/base/ash/interactive/interactive_ash_test.h"

namespace ash {

// Base test class for interactive UI tests for supervised users, which allows
// the Kombucha framework to be used to interact with the UI during the test.
// The test sets up policies that are typically applied to supervised users and
// logs the user in.
class ChildUserInteractiveBaseTest : public InteractiveAshTest {
 public:
  ChildUserInteractiveBaseTest();
  ChildUserInteractiveBaseTest(const ChildUserInteractiveBaseTest&) = delete;
  ChildUserInteractiveBaseTest& operator=(const ChildUserInteractiveBaseTest&) =
      delete;
  ~ChildUserInteractiveBaseTest() override = default;

  // InteractiveAshTest:
  void SetUpOnMainThread() override;

 protected:
  LoggedInUserMixin logged_in_user_mixin_{&mixin_host_, /*test_base=*/this,
                                          embedded_test_server(),
                                          LoggedInUserMixin::LogInType::kChild};
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_CHILD_ACCOUNTS_CHILD_USER_INTERACTIVE_BASE_TEST_H_
