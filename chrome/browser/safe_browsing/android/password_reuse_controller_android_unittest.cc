// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/android/password_reuse_controller_android.h"

#include <memory>
#include <string>

#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "chrome/browser/safe_browsing/chrome_password_protection_service.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/test_renderer_host.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/build_info.h"
#endif

namespace safe_browsing {

using ReusedPasswordAccountType = safe_browsing::LoginReputationClientRequest::
    PasswordReuseEvent::ReusedPasswordAccountType;

using OnWarningDone = base::OnceCallback<void(WarningAction)>;

using MockOnWarningDone = base::MockCallback<OnWarningDone>;

class PasswordReuseControllerAndroidTest
    : public ChromeRenderViewHostTestHarness {
 public:
  PasswordReuseControllerAndroid* MakeController(
      ChromePasswordProtectionService* service,
      ReusedPasswordAccountType password_type,
      OnWarningDone done_callback) {
    // The *Dialog() methods used by the tests below all invoke `delete this;`,
    // thus there is no memory leak here.
    return new PasswordReuseControllerAndroid(
        web_contents(), service, password_type, std::move(done_callback));
  }

  void AssertWarningActionEquality(WarningAction expected_action_warning,
                                   WarningAction warning_action) {
    ASSERT_EQ(expected_action_warning, warning_action);
  }
};

TEST_F(PasswordReuseControllerAndroidTest, ClickedIgnore) {
  ReusedPasswordAccountType password_type;

  MakeController(
      nullptr, password_type,
      base::BindOnce(
          &PasswordReuseControllerAndroidTest::AssertWarningActionEquality,
          base::Unretained(this), WarningAction::IGNORE_WARNING))
      ->IgnoreDialog();
}

TEST_F(PasswordReuseControllerAndroidTest, ClickedClose) {
  ReusedPasswordAccountType password_type;

  MakeController(
      nullptr, password_type,
      base::BindOnce(
          &PasswordReuseControllerAndroidTest::AssertWarningActionEquality,
          base::Unretained(this), WarningAction::CLOSE))
      ->CloseDialog();
}

TEST_F(PasswordReuseControllerAndroidTest, VerifyButtonText) {
#if BUILDFLAG(IS_ANDROID)
  if (base::android::BuildInfo::GetInstance()->is_automotive()) {
    GTEST_SKIP() << "This test should not run on automotive.";
  }
#endif  // BUILDFLAG(IS_ANDROID)

  MockOnWarningDone empty_callback;
  ReusedPasswordAccountType password_type;

  PasswordReuseControllerAndroid* controller =
      MakeController(nullptr, password_type, empty_callback.Get());

  {
    ASSERT_EQ(l10n_util::GetStringUTF16(IDS_CLOSE),
              controller->GetPrimaryButtonText());
    ASSERT_EQ(std::u16string(), controller->GetSecondaryButtonText());
  }
  {
    password_type.set_account_type(ReusedPasswordAccountType::SAVED_PASSWORD);
    password_type.set_is_account_syncing(false);

    controller->SetReusedPasswordAccountTypeForTesting(password_type);

    ASSERT_EQ(l10n_util::GetStringUTF16(IDS_PAGE_INFO_CHECK_PASSWORDS_BUTTON),
              controller->GetPrimaryButtonText());
    ASSERT_EQ(
        l10n_util::GetStringUTF16(IDS_PAGE_INFO_IGNORE_PASSWORD_WARNING_BUTTON),
        controller->GetSecondaryButtonText());
  }
  {
    password_type.set_account_type(ReusedPasswordAccountType::GMAIL);
    password_type.set_is_account_syncing(true);

    controller->SetReusedPasswordAccountTypeForTesting(password_type);

    ASSERT_EQ(l10n_util::GetStringUTF16(IDS_PAGE_INFO_PROTECT_ACCOUNT_BUTTON),
              controller->GetPrimaryButtonText());
    ASSERT_EQ(
        l10n_util::GetStringUTF16(IDS_PAGE_INFO_IGNORE_PASSWORD_WARNING_BUTTON),
        controller->GetSecondaryButtonText());
  }
  {
    ReusedPasswordAccountType empty_reused_password;
    controller->SetReusedPasswordAccountTypeForTesting(empty_reused_password);

    ASSERT_EQ(l10n_util::GetStringUTF16(IDS_CLOSE),
              controller->GetPrimaryButtonText());
    ASSERT_EQ(std::u16string(), controller->GetSecondaryButtonText());
  }
  {
    password_type.set_account_type(ReusedPasswordAccountType::GMAIL);
    password_type.set_is_account_syncing(false);

    controller->SetReusedPasswordAccountTypeForTesting(password_type);

    ASSERT_EQ(l10n_util::GetStringUTF16(IDS_CLOSE),
              controller->GetPrimaryButtonText());
    ASSERT_EQ(std::u16string(), controller->GetSecondaryButtonText());
  }

  delete controller;
}

#if BUILDFLAG(IS_ANDROID)
TEST_F(PasswordReuseControllerAndroidTest, VerifyButtonTextOnAutomotive) {
  if (!base::android::BuildInfo::GetInstance()->is_automotive()) {
    GTEST_SKIP() << "This test should only run on automotive.";
  }
  MockOnWarningDone empty_callback;
  ReusedPasswordAccountType password_type;

  PasswordReuseControllerAndroid* controller =
      MakeController(nullptr, password_type, empty_callback.Get());

  {
    ASSERT_EQ(l10n_util::GetStringUTF16(IDS_CLOSE),
              controller->GetPrimaryButtonText());
    ASSERT_EQ(std::u16string(), controller->GetSecondaryButtonText());
  }
  {
    password_type.set_account_type(ReusedPasswordAccountType::SAVED_PASSWORD);
    password_type.set_is_account_syncing(false);

    controller->SetReusedPasswordAccountTypeForTesting(password_type);

    ASSERT_EQ(l10n_util::GetStringUTF16(IDS_CLOSE),
              controller->GetPrimaryButtonText());
    ASSERT_EQ(std::u16string(), controller->GetSecondaryButtonText());
  }
  {
    password_type.set_account_type(ReusedPasswordAccountType::GMAIL);
    password_type.set_is_account_syncing(true);

    controller->SetReusedPasswordAccountTypeForTesting(password_type);

    ASSERT_EQ(l10n_util::GetStringUTF16(IDS_PAGE_INFO_PROTECT_ACCOUNT_BUTTON),
              controller->GetPrimaryButtonText());
    ASSERT_EQ(
        l10n_util::GetStringUTF16(IDS_PAGE_INFO_IGNORE_PASSWORD_WARNING_BUTTON),
        controller->GetSecondaryButtonText());
  }
  {
    ReusedPasswordAccountType empty_reused_password;
    controller->SetReusedPasswordAccountTypeForTesting(empty_reused_password);

    ASSERT_EQ(l10n_util::GetStringUTF16(IDS_CLOSE),
              controller->GetPrimaryButtonText());
    ASSERT_EQ(std::u16string(), controller->GetSecondaryButtonText());
  }
  {
    password_type.set_account_type(ReusedPasswordAccountType::GMAIL);
    password_type.set_is_account_syncing(false);

    controller->SetReusedPasswordAccountTypeForTesting(password_type);

    ASSERT_EQ(l10n_util::GetStringUTF16(IDS_CLOSE),
              controller->GetPrimaryButtonText());
    ASSERT_EQ(std::u16string(), controller->GetSecondaryButtonText());
  }

  delete controller;
}
#endif  // BUILDFLAG(IS_ANDROID)

TEST_F(PasswordReuseControllerAndroidTest, WebContentDestroyed) {
  base::HistogramTester histograms;
  ReusedPasswordAccountType password_type;

  MakeController(
      nullptr, password_type,
      base::BindOnce(
          &PasswordReuseControllerAndroidTest::AssertWarningActionEquality,
          base::Unretained(this), WarningAction::IGNORE_WARNING));

  DeleteContents();
  // This histogram is logged in the destructor of the controller. If it is
  // logged, it indicates that the controller is properly destroyed after the
  // WebContents is destroyed.
  histograms.ExpectTotalCount("PasswordProtection.ModalWarningDialogLifetime",
                              /*count=*/1);
}

}  // namespace safe_browsing
