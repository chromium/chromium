// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/android/account_chooser_dialog_android.h"

#include "base/functional/bind.h"
#include "base/memory/scoped_refptr.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "chrome/browser/password_manager/chrome_password_manager_client.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/device_reauth/biometric_authenticator.h"
#include "components/device_reauth/mock_biometric_authenticator.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"
#include "components/password_manager/core/browser/password_manager_test_utils.h"
#include "components/password_manager/core/browser/stub_password_manager_client.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "content/public/browser/visibility.h"
#include "content/public/browser/web_contents.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

using base::test::RunOnceCallback;
using device_reauth::BiometricAuthRequester;
using device_reauth::MockBiometricAuthenticator;
using testing::_;
using testing::Eq;
using testing::Pointee;
using testing::Return;

password_manager::PasswordFormData kFormData1 = {
    password_manager::PasswordForm::Scheme::kHtml,
    "http://example.com/",
    "http://example.com/origin",
    "http://example.com/action",
    u"submit_element",
    u"username_element",
    u"password_element",
    u"",
    u"",
    true,
    1,
};

password_manager::PasswordFormData kFormData2 = {
    password_manager::PasswordForm::Scheme::kHtml,
    "http://test.com/",
    "http://test.com/origin",
    "http://test.com/action",
    u"submit_element",
    u"username_element",
    u"password_element",
    u"",
    u"",
    true,
    1,
};

class MockPasswordManagerClient
    : public password_manager::StubPasswordManagerClient {
 public:
  MOCK_METHOD(scoped_refptr<device_reauth::BiometricAuthenticator>,
              GetBiometricAuthenticator,
              (),
              (override));
};

}  // namespace

class AccountChooserDialogAndroidTest : public ChromeRenderViewHostTestHarness {
 public:
  AccountChooserDialogAndroidTest();

  AccountChooserDialogAndroidTest(const AccountChooserDialogAndroidTest&) =
      delete;
  AccountChooserDialogAndroidTest& operator=(
      const AccountChooserDialogAndroidTest&) = delete;

  ~AccountChooserDialogAndroidTest() override {}

  void SetUp() override;

 protected:
  AccountChooserDialogAndroid* CreateDialogOneAccount();
  AccountChooserDialogAndroid* CreateDialogManyAccounts();

  AccountChooserDialogAndroid* CreateDialog(
      std::vector<std::unique_ptr<password_manager::PasswordForm>> credentials);

  MockPasswordManagerClient client_;

  scoped_refptr<MockBiometricAuthenticator> authenticator_ =
      base::MakeRefCounted<MockBiometricAuthenticator>();

  base::MockCallback<ManagePasswordsState::CredentialsCallback>
      credential_callback_;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

AccountChooserDialogAndroidTest::AccountChooserDialogAndroidTest() {
  scoped_feature_list_.InitAndEnableFeature(
      password_manager::features::kBiometricTouchToFill);
}

void AccountChooserDialogAndroidTest::SetUp() {
  ChromeRenderViewHostTestHarness::SetUp();
}

AccountChooserDialogAndroid* AccountChooserDialogAndroidTest::CreateDialog(
    std::vector<std::unique_ptr<password_manager::PasswordForm>> credentials) {
  return new AccountChooserDialogAndroid(
      web_contents(), &client_, std::move(credentials),
      url::Origin::Create(GURL("https://example.com")),
      credential_callback_.Get());
}

AccountChooserDialogAndroid*
AccountChooserDialogAndroidTest::CreateDialogOneAccount() {
  std::vector<std::unique_ptr<password_manager::PasswordForm>> credentials;
  credentials.push_back(FillPasswordFormWithData(kFormData1));
  return CreateDialog(std::move(credentials));
}

AccountChooserDialogAndroid*
AccountChooserDialogAndroidTest::CreateDialogManyAccounts() {
  std::vector<std::unique_ptr<password_manager::PasswordForm>> credentials;
  credentials.push_back(FillPasswordFormWithData(kFormData1));
  credentials.push_back(FillPasswordFormWithData(kFormData2));
  return CreateDialog(std::move(credentials));
}

TEST_F(AccountChooserDialogAndroidTest,
       CheckHistogramsReportingOnceAccountViaOnAccountClick) {
  base::HistogramTester histogram_tester;
  AccountChooserDialogAndroid* dialog = CreateDialogOneAccount();
  dialog->OnCredentialClicked(base::android::AttachCurrentThread(),
                              nullptr /* obj */, 0 /* credential_item */,
                              false /* signin_button_clicked */);

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
  dialog->OnCredentialClicked(base::android::AttachCurrentThread(),
                              nullptr /* obj */, 0 /* credential_item */,
                              true /* signin_button_clicked */);

  histogram_tester.ExpectUniqueSample(
      "PasswordManager.AccountChooserDialogOneAccount",
      password_manager::metrics_util::ACCOUNT_CHOOSER_SIGN_IN, 1);
  histogram_tester.ExpectTotalCount(
      "PasswordManager.AccountChooserDialogMultipleAccounts", 0);
}

TEST_F(AccountChooserDialogAndroidTest, CheckHistogramsReportingManyAccounts) {
  base::HistogramTester histogram_tester;
  AccountChooserDialogAndroid* dialog = CreateDialogManyAccounts();
  dialog->OnCredentialClicked(base::android::AttachCurrentThread(),
                              nullptr /* obj */, 0 /* credential_item */,
                              false /* signin_button_clicked */);

  histogram_tester.ExpectUniqueSample(
      "PasswordManager.AccountChooserDialogMultipleAccounts",
      password_manager::metrics_util::ACCOUNT_CHOOSER_CREDENTIAL_CHOSEN, 1);
  histogram_tester.ExpectTotalCount(
      "PasswordManager.AccountChooserDialogOneAccount", 0);
}

TEST_F(AccountChooserDialogAndroidTest, SendsCredentialIfAuthNotAvailable) {
  AccountChooserDialogAndroid* dialog = CreateDialogManyAccounts();

  EXPECT_CALL(client_, GetBiometricAuthenticator)
      .WillOnce(Return(authenticator_));
  EXPECT_CALL(*authenticator_.get(),
              CanAuthenticate(BiometricAuthRequester::kAccountChooserDialog))
      .WillOnce(Return(false));
  std::unique_ptr<password_manager::PasswordForm> form =
      FillPasswordFormWithData(kFormData2);

  EXPECT_CALL(credential_callback_, Run(Pointee(*form.get())));

  dialog->OnCredentialClicked(base::android::AttachCurrentThread(),
                              nullptr /* obj */, 1 /* credential_item */,
                              false /* signin_button_clicked */);
}

TEST_F(AccountChooserDialogAndroidTest, SendsCredentialIfAuthSuccessful) {
  AccountChooserDialogAndroid* dialog = CreateDialogManyAccounts();

  EXPECT_CALL(client_, GetBiometricAuthenticator)
      .WillOnce(Return(authenticator_));
  EXPECT_CALL(*authenticator_.get(),
              CanAuthenticate(BiometricAuthRequester::kAccountChooserDialog))
      .WillOnce(Return(true));
  EXPECT_CALL(*authenticator_.get(),
              Authenticate(BiometricAuthRequester::kAccountChooserDialog, _,
                           /*use_last_valid_auth=*/true))
      .WillOnce(RunOnceCallback<1>(true));

  std::unique_ptr<password_manager::PasswordForm> form =
      FillPasswordFormWithData(kFormData2);
  EXPECT_CALL(credential_callback_, Run(Pointee(*form.get())));

  dialog->OnCredentialClicked(base::android::AttachCurrentThread(),
                              nullptr /* obj */, 1 /* credential_item */,
                              false /* signin_button_clicked */);
}

TEST_F(AccountChooserDialogAndroidTest, DoesntSendCredentialIfAuthFailed) {
  AccountChooserDialogAndroid* dialog = CreateDialogManyAccounts();

  EXPECT_CALL(client_, GetBiometricAuthenticator)
      .WillOnce(Return(authenticator_));
  EXPECT_CALL(*authenticator_.get(),
              CanAuthenticate(BiometricAuthRequester::kAccountChooserDialog))
      .WillOnce(Return(true));
  EXPECT_CALL(*authenticator_.get(),
              Authenticate(BiometricAuthRequester::kAccountChooserDialog, _,
                           /*use_last_valid_auth=*/true))
      .WillOnce(RunOnceCallback<1>(false));

  std::unique_ptr<password_manager::PasswordForm> form =
      FillPasswordFormWithData(kFormData2);
  EXPECT_CALL(credential_callback_, Run(nullptr));

  dialog->OnCredentialClicked(base::android::AttachCurrentThread(),
                              nullptr /* obj */, 1 /* credential_item */,
                              false /* signin_button_clicked */);
}

TEST_F(AccountChooserDialogAndroidTest, CancelsAuthIfDestroyed) {
  AccountChooserDialogAndroid* dialog = CreateDialogManyAccounts();

  EXPECT_CALL(client_, GetBiometricAuthenticator)
      .WillOnce(Return(authenticator_));
  EXPECT_CALL(*authenticator_.get(),
              CanAuthenticate(BiometricAuthRequester::kAccountChooserDialog))
      .WillOnce(Return(true));
  EXPECT_CALL(*authenticator_.get(),
              Authenticate(BiometricAuthRequester::kAccountChooserDialog, _,
                           /*use_last_valid_auth=*/true));

  dialog->OnCredentialClicked(base::android::AttachCurrentThread(),
                              nullptr /* obj */, 1 /* credential_item */,
                              false /* signin_button_clicked */);

  EXPECT_CALL(*authenticator_.get(),
              Cancel(BiometricAuthRequester::kAccountChooserDialog));
  dialog->OnVisibilityChanged(content::Visibility::HIDDEN);
}
