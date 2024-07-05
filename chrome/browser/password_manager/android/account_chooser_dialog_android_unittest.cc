// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/android/account_chooser_dialog_android.h"

#include "base/android/build_info.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/mock_callback.h"
#include "chrome/browser/password_manager/chrome_password_manager_client.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/device_reauth/device_authenticator.h"
#include "components/device_reauth/mock_device_authenticator.h"
#include "components/password_manager/core/browser/features/password_features.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"
#include "components/password_manager/core/browser/password_manager_test_utils.h"
#include "components/password_manager/core/browser/stub_password_manager_client.h"
#include "content/public/browser/visibility.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

using base::test::RunOnceCallback;
using device_reauth::MockDeviceAuthenticator;
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
  MOCK_METHOD(std::unique_ptr<device_reauth::DeviceAuthenticator>,
              GetDeviceAuthenticator,
              (),
              (override));
  MOCK_METHOD(bool,
              IsReauthBeforeFillingRequired,
              (device_reauth::DeviceAuthenticator*),
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
  AccountChooserDialogAndroid* CreateDialogManyAccounts();

  AccountChooserDialogAndroid* CreateDialog(
      std::vector<std::unique_ptr<password_manager::PasswordForm>> credentials);

  MockPasswordManagerClient client_;

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
AccountChooserDialogAndroidTest::CreateDialogManyAccounts() {
  std::vector<std::unique_ptr<password_manager::PasswordForm>> credentials;
  credentials.push_back(
      FillPasswordFormWithData(kFormData1, /*is_account_store=*/false));
  credentials.push_back(
      FillPasswordFormWithData(kFormData2, /*is_account_store=*/false));
  return CreateDialog(std::move(credentials));
}

TEST_F(AccountChooserDialogAndroidTest, SendsCredentialIfAuthNotAvailable) {
  // Auth is required to fill passwords in Android automotive.
  if (base::android::BuildInfo::GetInstance()->is_automotive()) {
    GTEST_SKIP();
  }

  AccountChooserDialogAndroid* dialog = CreateDialogManyAccounts();

  auto authenticator = std::make_unique<MockDeviceAuthenticator>();

  EXPECT_CALL(client_, IsReauthBeforeFillingRequired).WillOnce(Return(false));
  EXPECT_CALL(client_, GetDeviceAuthenticator)
      .WillOnce(Return(testing::ByMove(std::move(authenticator))));
  std::unique_ptr<password_manager::PasswordForm> form =
      FillPasswordFormWithData(kFormData2, /*is_account_store=*/false);

  EXPECT_CALL(credential_callback_, Run(Pointee(*form.get())));

  dialog->OnCredentialClicked(base::android::AttachCurrentThread(),
                              nullptr /* obj */, 1 /* credential_item */,
                              false /* signin_button_clicked */);
}

TEST_F(AccountChooserDialogAndroidTest, SendsCredentialIfAuthSuccessful) {
  AccountChooserDialogAndroid* dialog = CreateDialogManyAccounts();

  auto authenticator = std::make_unique<MockDeviceAuthenticator>();

  ON_CALL(client_, IsReauthBeforeFillingRequired).WillByDefault(Return(true));
  EXPECT_CALL(*authenticator, AuthenticateWithMessage)
      .WillOnce(RunOnceCallback<1>(true));
  EXPECT_CALL(client_, GetDeviceAuthenticator)
      .WillOnce(Return(testing::ByMove(std::move(authenticator))));

  std::unique_ptr<password_manager::PasswordForm> form =
      FillPasswordFormWithData(kFormData2, /*is_account_store=*/false);
  EXPECT_CALL(credential_callback_, Run(Pointee(*form.get())));

  dialog->OnCredentialClicked(base::android::AttachCurrentThread(),
                              nullptr /* obj */, 1 /* credential_item */,
                              false /* signin_button_clicked */);
}

TEST_F(AccountChooserDialogAndroidTest, DoesntSendCredentialIfAuthFailed) {
  AccountChooserDialogAndroid* dialog = CreateDialogManyAccounts();

  auto authenticator = std::make_unique<MockDeviceAuthenticator>();

  ON_CALL(client_, IsReauthBeforeFillingRequired).WillByDefault(Return(true));
  EXPECT_CALL(*authenticator, AuthenticateWithMessage)
      .WillOnce(RunOnceCallback<1>(false));
  EXPECT_CALL(client_, GetDeviceAuthenticator)
      .WillOnce(Return(testing::ByMove(std::move(authenticator))));

  std::unique_ptr<password_manager::PasswordForm> form =
      FillPasswordFormWithData(kFormData2, /*is_account_store=*/false);
  EXPECT_CALL(credential_callback_, Run(nullptr));

  dialog->OnCredentialClicked(base::android::AttachCurrentThread(),
                              nullptr /* obj */, 1 /* credential_item */,
                              false /* signin_button_clicked */);
}

TEST_F(AccountChooserDialogAndroidTest, CancelsAuthIfDestroyed) {
  AccountChooserDialogAndroid* dialog = CreateDialogManyAccounts();

  auto authenticator = std::make_unique<MockDeviceAuthenticator>();
  auto* authenticator_ptr = authenticator.get();

  ON_CALL(client_, IsReauthBeforeFillingRequired).WillByDefault(Return(true));
  EXPECT_CALL(*authenticator_ptr, AuthenticateWithMessage);
  EXPECT_CALL(client_, GetDeviceAuthenticator)
      .WillOnce(Return(testing::ByMove(std::move(authenticator))));

  dialog->OnCredentialClicked(base::android::AttachCurrentThread(),
                              nullptr /* obj */, 1 /* credential_item */,
                              false /* signin_button_clicked */);

  EXPECT_CALL(*authenticator_ptr, Cancel());
  dialog->OnVisibilityChanged(content::Visibility::HIDDEN);
}
