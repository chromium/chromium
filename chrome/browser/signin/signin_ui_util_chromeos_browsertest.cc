// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/signin_ui_util.h"

#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/ui/ash/account_manager/scoped_fake_account_manager_dialog.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/account_manager_core/account_manager_metrics.h"
#include "components/account_manager_core/account_upsertion_result.h"
#include "components/signin/public/base/signin_metrics.h"
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace signin_ui_util {

namespace {

constexpr char kReauthEmail[] = "test@example.com";

using ::testing::Optional;
using ::testing::StrEq;

}  // namespace

using SigninUiUtilChromeOSTest = InProcessBrowserTest;

IN_PROC_BROWSER_TEST_F(SigninUiUtilChromeOSTest,
                       ShowReauthForAccountOpensAccountManagerDialog) {
  base::HistogramTester histogram_tester;
  auto* profile = browser()->profile();
  ash::test::ScopedFakeAccountManagerDialog fake_account_manager_dialog(
      profile);

  ShowReauthForAccount(profile, kReauthEmail,
                       signin_metrics::AccessPoint::kWebSignin);

  EXPECT_EQ(1, fake_account_manager_dialog
                   ->show_account_reauthentication_dialog_calls());
  EXPECT_THAT(fake_account_manager_dialog->last_reauth_email(),
              Optional(StrEq(kReauthEmail)));
  EXPECT_EQ(0,
            fake_account_manager_dialog->show_account_addition_dialog_calls());
  histogram_tester.ExpectUniqueSample(
      account_manager::kAccountAdditionSourceHistogramName,
      account_manager::AccountAdditionSource::kContentAreaReauth,
      /*expected_count=*/1);
  histogram_tester.ExpectTotalCount(
      account_manager::kAccountUpsertionResultStatusHistogramName, 0);

  fake_account_manager_dialog->CloseDialog();

  histogram_tester.ExpectUniqueSample(
      account_manager::kAccountUpsertionResultStatusHistogramName,
      account_manager::AccountUpsertionResult::Status::kCancelledByUser,
      /*expected_count=*/1);
}

}  // namespace signin_ui_util
