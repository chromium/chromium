// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>
#include <iterator>
#include <string>

#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/ash/login/saml/security_token_saml_test.h"
#include "chrome/browser/ash/login/test/js_checker.h"
#include "chrome/browser/ash/login/test/session_manager_state_waiter.h"
#include "chrome/browser/ash/login/users/test_users.h"
#include "components/account_id/account_id.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {
namespace {

std::string GetActiveUserEmail() {
  const user_manager::User* user =
      user_manager::UserManager::Get()->GetActiveUser();
  if (!user)
    return std::string();
  return user->GetAccountId().GetUserEmail();
}

}  // namespace

// Tests the successful login scenario with the correct PIN.
IN_PROC_BROWSER_TEST_P(SecurityTokenSamlTest, Basic) {
  StartSignIn();
  WaitForPinDialog();
  test::OobeJS().ExpectVisiblePath({"gaia-signin", "pinDialog"});

  InputPinByClickingKeypad(GetCorrectPin());
  ClickPinDialogSubmit();
  test::WaitForPrimaryUserSessionStart();
  EXPECT_EQ(saml_test_users::kFirstUserCorpExampleComEmail,
            GetActiveUserEmail());
  EXPECT_EQ(1, pin_dialog_shown_count());
}

// Tests that the login doesn't hit the timeout for Chrome waiting on Gaia to
// signal the login completion.
IN_PROC_BROWSER_TEST_P(SecurityTokenSamlTest, NoGaiaTimeout) {
  // Arrange:
  base::HistogramTester histogram_tester;

  // Act:
  StartSignIn();
  WaitForPinDialog();
  InputPinByClickingKeypad(GetCorrectPin());
  ClickPinDialogSubmit();
  test::WaitForPrimaryUserSessionStart();

  // Assert:
  histogram_tester.ExpectBucketCount("ChromeOS.Gaia.Message.Saml.UserInfo", 0,
                                     0);
  histogram_tester.ExpectBucketCount("ChromeOS.Gaia.Message.Saml.UserInfo", 1,
                                     1);

  histogram_tester.ExpectBucketCount("ChromeOS.Gaia.Message.Saml.CloseView", 0,
                                     0);
  histogram_tester.ExpectBucketCount("ChromeOS.Gaia.Message.Saml.CloseView", 1,
                                     1);
}
// Run tests with both implementations of cryptohome API.
INSTANTIATE_TEST_SUITE_P(All, SecurityTokenSamlTest, testing::Bool());

}  // namespace ash
