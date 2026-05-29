// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/signin_ui_util.h"

#include <memory>

#include "base/check_deref.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chromeos/ash/components/account_manager/account_manager_factory.h"
#include "components/account_manager_core/account_manager_metrics.h"
#include "components/account_manager_core/account_upsertion_result.h"
#include "components/account_manager_core/chromeos/account_manager_mojo_service.h"
#include "components/account_manager_core/chromeos/fake_account_manager_ui.h"
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
  Profile* profile = browser()->profile();
  crosapi::AccountManagerMojoService& account_manager_mojo_service =
      CHECK_DEREF(
          ash::AccountManagerFactory::Get()->GetAccountManagerMojoService(
              profile->GetPath().value()));

  auto fake_account_manager_ui = std::make_unique<FakeAccountManagerUI>();
  FakeAccountManagerUI* fake_account_manager_ui_ptr =
      fake_account_manager_ui.get();
  account_manager_mojo_service.SetAccountManagerUI(
      std::move(fake_account_manager_ui));

  ShowReauthForAccount(profile, kReauthEmail,
                       signin_metrics::AccessPoint::kWebSignin);

  EXPECT_EQ(1, fake_account_manager_ui_ptr
                   ->show_account_reauthentication_dialog_calls());
  EXPECT_THAT(fake_account_manager_ui_ptr->last_reauth_email(),
              Optional(StrEq(kReauthEmail)));
  EXPECT_EQ(0,
            fake_account_manager_ui_ptr->show_account_addition_dialog_calls());
  EXPECT_EQ(0,
            fake_account_manager_ui_ptr->show_manage_accounts_settings_calls());
  histogram_tester.ExpectUniqueSample(
      account_manager::kAccountAdditionSourceHistogramName,
      account_manager::AccountAdditionSource::kContentAreaReauth,
      /*expected_count=*/1);
  histogram_tester.ExpectTotalCount(
      account_manager::kAccountUpsertionResultStatusHistogramName, 0);

  fake_account_manager_ui_ptr->CloseDialog();

  histogram_tester.ExpectUniqueSample(
      account_manager::kAccountUpsertionResultStatusHistogramName,
      account_manager::AccountUpsertionResult::Status::kCancelledByUser,
      /*expected_count=*/1);
}

}  // namespace signin_ui_util
