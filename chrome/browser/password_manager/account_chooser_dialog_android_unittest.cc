// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/account_chooser_dialog_android.h"

#include "base/bind.h"
#include "base/macros.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/password_manager/chrome_password_manager_client.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/autofill/core/common/password_form.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"
#include "components/password_manager/core/browser/password_manager_test_utils.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "content/public/browser/web_contents.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

password_manager::PasswordFormData kFormData = {
    autofill::PasswordForm::Scheme::kHtml,
    "http://example.com/",
    "http://example.com/origin",
    "http://example.com/action",
    L"submit_element",
    L"username_element",
    L"password_element",
    L"",
    L"",
    true,
    1,
};

}  // namespace

class AccountChooserDialogAndroidTest : public ChromeRenderViewHostTestHarness {
 public:
  AccountChooserDialogAndroidTest() {}
  ~AccountChooserDialogAndroidTest() override {}

  void SetUp() override;

  MOCK_METHOD1(OnChooseCredential, void(const autofill::PasswordForm*));

 protected:
  AccountChooserDialogAndroid* CreateDialogOneAccount();
  AccountChooserDialogAndroid* CreateDialogManyAccounts();

  AccountChooserDialogAndroid* CreateDialog(
      std::vector<std::unique_ptr<autofill::PasswordForm>> credentials);

 private:
  DISALLOW_COPY_AND_ASSIGN(AccountChooserDialogAndroidTest);
};

void AccountChooserDialogAndroidTest::SetUp() {
  ChromeRenderViewHostTestHarness::SetUp();
  ChromePasswordManagerClient::CreateForWebContentsWithAutofillClient(
      web_contents(), nullptr);
}

AccountChooserDialogAndroid* AccountChooserDialogAndroidTest::CreateDialog(
    std::vector<std::unique_ptr<autofill::PasswordForm>> credentials) {
  return new AccountChooserDialogAndroid(
      web_contents(), std::move(credentials), GURL("https://example.com"),
      base::Bind(&AccountChooserDialogAndroidTest::OnChooseCredential,
                 base::Unretained(this)));
}

AccountChooserDialogAndroid*
AccountChooserDialogAndroidTest::CreateDialogOneAccount() {
  std::vector<std::unique_ptr<autofill::PasswordForm>> credentials;
  credentials.push_back(FillPasswordFormWithData(kFormData));
  return CreateDialog(std::move(credentials));
}

AccountChooserDialogAndroid*
AccountChooserDialogAndroidTest::CreateDialogManyAccounts() {
  std::vector<std::unique_ptr<autofill::PasswordForm>> credentials;
  credentials.push_back(FillPasswordFormWithData(kFormData));
  credentials.push_back(FillPasswordFormWithData(kFormData));
  return CreateDialog(std::move(credentials));
}

TEST_F(AccountChooserDialogAndroidTest,
       CheckHistogramsReportingOnceAccountViaOnAccountClick) {
  base::HistogramTester histogram_tester;
  AccountChooserDialogAndroid* dialog = CreateDialogOneAccount();
  dialog->OnCredentialClicked(
      base::android::AttachCurrentThread(), nullptr /* obj */,
      0 /* credential_item */,
      false /* signin_button_clicked */);
  dialog->Destroy(base::android::AttachCurrentThread(), nullptr);

  histogram_tester.ExpectUniqueSample(
      "PasswordManager.AccountChooserDialogOneAccount",
      password_manager::metrics_util::ACCOUNT_CHOOSER_CREDENTIAL_CHOSEN, 1);
  histogram_tester.ExpectTotalCount(
      "PasswordManager.AccountChooserDialogMultipleAccounts", 0);
}

TEST_F(AccountChooserDialogAndroidTest,
       CheckHistogramsReportingOneAccountChoosenViaSigninButton) {
  base::HistogramTester histogram_tester;
  AccountChooserDialogAndroid* dialog = CreateDialogOneAccount();
  dialog->OnCredentialClicked(
      base::android::AttachCurrentThread(), nullptr /* obj */,
      0 /* credential_item */,
      true /* signin_button_clicked */);
  dialog->Destroy(base::android::AttachCurrentThread(), nullptr);
  histogram_tester.ExpectUniqueSample(
      "PasswordManager.AccountChooserDialogOneAccount",
      password_manager::metrics_util::ACCOUNT_CHOOSER_SIGN_IN, 1);
  histogram_tester.ExpectTotalCount(
      "PasswordManager.AccountChooserDialogMultipleAccounts", 0);
}

TEST_F(AccountChooserDialogAndroidTest, CheckHistogramsReportingManyAccounts) {
  base::HistogramTester histogram_tester;
  AccountChooserDialogAndroid* dialog = CreateDialogManyAccounts();
  dialog->OnCredentialClicked(
      base::android::AttachCurrentThread(), nullptr /* obj */,
      0 /* credential_item */,
      false /* signin_button_clicked */);
  dialog->Destroy(base::android::AttachCurrentThread(), nullptr);

  histogram_tester.ExpectUniqueSample(
      "PasswordManager.AccountChooserDialogMultipleAccounts",
      password_manager::metrics_util::ACCOUNT_CHOOSER_CREDENTIAL_CHOSEN, 1);
  histogram_tester.ExpectTotalCount(
      "PasswordManager.AccountChooserDialogOneAccount", 0);
}
