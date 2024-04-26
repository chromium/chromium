// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/test/metrics/histogram_tester.h"
#include "build/buildflag.h"
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
#if !defined(NDEBUG)
// Flaky timeout in debug build crbug.com/321826024.
#define MAYBE_Basic DISABLED_Basic
#else
#define MAYBE_Basic Basic
#endif
IN_PROC_BROWSER_TEST_F(SecurityTokenSamlTest, MAYBE_Basic) {
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
#if !defined(NDEBUG)
// Flaky timeout in debug build crbug.com/321826024.
#define MAYBE_NoGaiaTimeout DISABLED_NoGaiaTimeout
#else
#define MAYBE_NoGaiaTimeout NoGaiaTimeout
#endif
IN_PROC_BROWSER_TEST_F(SecurityTokenSamlTest, MAYBE_NoGaiaTimeout) {
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

}  // namespace ash
