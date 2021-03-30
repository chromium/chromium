// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/android/update_password_infobar_delegate_android.h"

#include <memory>
#include <string>

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/password_manager/chrome_password_manager_client.h"
#include "chrome/browser/ui/autofill/chrome_autofill_client.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/password_manager/core/browser/fake_form_fetcher.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_form_manager.h"
#include "components/password_manager/core/browser/password_save_manager_impl.h"
#include "components/password_manager/core/browser/stub_form_saver.h"
#include "components/password_manager/core/browser/stub_password_manager_client.h"
#include "components/password_manager/core/browser/stub_password_manager_driver.h"
#include "components/strings/grit/components_strings.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"

using password_manager::PasswordFormManager;
using password_manager::PasswordSaveManagerImpl;
using testing::ElementsAre;

namespace {

class TestUpdatePasswordInfoBarDelegate : public UpdatePasswordInfoBarDelegate {
 public:
  TestUpdatePasswordInfoBarDelegate(
      content::WebContents* web_contents,
      std::unique_ptr<password_manager::PasswordFormManager> form_to_save,
      bool is_smartlock_branding_enabled)
      : UpdatePasswordInfoBarDelegate(web_contents,
                                      std::move(form_to_save),
                                      is_smartlock_branding_enabled) {}

  ~TestUpdatePasswordInfoBarDelegate() override {}
};

}  // namespace

class UpdatePasswordInfoBarDelegateTest
    : public ChromeRenderViewHostTestHarness {
 public:
  UpdatePasswordInfoBarDelegateTest();
  ~UpdatePasswordInfoBarDelegateTest() override {}

  void SetUp() override;
  void TearDown() override;

  const password_manager::PasswordForm& test_form() { return test_form_; }

  // TODO(crbug.com/1048107): Replace real PasswordFormManager instance with the
  // mock.
  std::unique_ptr<password_manager::PasswordFormManager>
  CreateTestFormManager();

 protected:
  std::unique_ptr<UpdatePasswordInfoBarDelegate> CreateDelegate(
      std::unique_ptr<password_manager::PasswordFormManager>
          password_form_manager,
      bool is_smartlock_branding_enabled);

  password_manager::StubPasswordManagerClient client_;
  password_manager::StubPasswordManagerDriver driver_;

  password_manager::PasswordForm test_form_;
  autofill::FormData observed_form_;

 private:
  password_manager::FakeFormFetcher fetcher_;

  DISALLOW_COPY_AND_ASSIGN(UpdatePasswordInfoBarDelegateTest);
};

UpdatePasswordInfoBarDelegateTest::UpdatePasswordInfoBarDelegateTest() {
  test_form_.url = GURL("https://example.com");
  test_form_.username_value = u"username";
  test_form_.password_value = u"12345";

  // Create a simple sign-in form.
  observed_form_.url = test_form_.url;
  autofill::FormFieldData field;
  field.form_control_type = "text";
  field.value = test_form_.username_value;
  observed_form_.fields.push_back(field);
  field.form_control_type = "password";
  field.value = test_form_.password_value;
  observed_form_.fields.push_back(field);

  // Turn off waiting for server predictions in order to avoid dealing with
  // posted tasks in PasswordFormManager.
  PasswordFormManager::set_wait_for_server_predictions_for_filling(false);
}

void UpdatePasswordInfoBarDelegateTest::SetUp() {
  ChromeRenderViewHostTestHarness::SetUp();
  autofill::ChromeAutofillClient::CreateForWebContents(web_contents());
  ChromePasswordManagerClient::CreateForWebContentsWithAutofillClient(
      web_contents(),
      autofill::ChromeAutofillClient::FromWebContents(web_contents()));
}

void UpdatePasswordInfoBarDelegateTest::TearDown() {
  ChromeRenderViewHostTestHarness::TearDown();
}

std::unique_ptr<password_manager::PasswordFormManager>
UpdatePasswordInfoBarDelegateTest::CreateTestFormManager() {
  auto manager = std::make_unique<password_manager::PasswordFormManager>(
      &client_, driver_.AsWeakPtr(), observed_form_, &fetcher_,
      std::make_unique<PasswordSaveManagerImpl>(
          std::make_unique<password_manager::StubFormSaver>()),
      nullptr /* metrics_recorder */);
  manager->ProvisionallySave(observed_form_, &driver_, nullptr);
  return manager;
}

std::unique_ptr<UpdatePasswordInfoBarDelegate>
UpdatePasswordInfoBarDelegateTest::CreateDelegate(
    std::unique_ptr<password_manager::PasswordFormManager>
        password_form_manager,
    bool is_smartlock_branding_enabled) {
  std::unique_ptr<UpdatePasswordInfoBarDelegate> delegate(
      new TestUpdatePasswordInfoBarDelegate(web_contents(),
                                            std::move(password_form_manager),
                                            is_smartlock_branding_enabled));
  return delegate;
}

TEST_F(UpdatePasswordInfoBarDelegateTest, HasDetailsMessageForSignedIn) {
  std::unique_ptr<password_manager::PasswordFormManager> password_form_manager(
      CreateTestFormManager());
  std::unique_ptr<PasswordManagerInfoBarDelegate> infobar(
      CreateDelegate(std::move(password_form_manager),
                     true /* is_smartlock_branding_enabled */));
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_SAVE_PASSWORD_FOOTER),
            infobar->GetDetailsMessageText());
}

TEST_F(UpdatePasswordInfoBarDelegateTest, EmptyDetailsMessageForNotSignedIn) {
  std::unique_ptr<password_manager::PasswordFormManager> password_form_manager(
      CreateTestFormManager());
  std::unique_ptr<PasswordManagerInfoBarDelegate> infobar(
      CreateDelegate(std::move(password_form_manager),
                     false /* is_smartlock_branding_enabled */));
  EXPECT_TRUE(infobar->GetDetailsMessageText().empty());
}

TEST_F(UpdatePasswordInfoBarDelegateTest, NoCurrentForms) {
  std::vector<std::u16string> usernames;
  std::vector<std::unique_ptr<password_manager::PasswordForm>> current_forms;
  std::u16string default_username = u"username";
  unsigned int selected_username =
      UpdatePasswordInfoBarDelegate::GetDisplayUsernames(
          current_forms, default_username, &usernames);
  EXPECT_EQ(default_username, usernames[selected_username]);
  EXPECT_THAT(usernames, ElementsAre(default_username));
}

TEST_F(UpdatePasswordInfoBarDelegateTest, MultipleCurrentForms) {
  std::vector<std::unique_ptr<password_manager::PasswordForm>> current_forms;
  password_manager::PasswordForm additional_form;
  additional_form.username_value = u"another username";
  current_forms.push_back(
      std::make_unique<password_manager::PasswordForm>(test_form_));
  current_forms.push_back(
      std::make_unique<password_manager::PasswordForm>(additional_form));

  std::u16string default_username = u"another username";

  std::vector<std::u16string> usernames;
  unsigned int selected_username =
      UpdatePasswordInfoBarDelegate::GetDisplayUsernames(
          current_forms, default_username, &usernames);
  EXPECT_EQ(default_username, usernames[selected_username]);
  EXPECT_THAT(usernames,
              ElementsAre(test_form_.username_value, default_username));
}

TEST_F(UpdatePasswordInfoBarDelegateTest, EmptyUsername) {
  std::vector<std::unique_ptr<password_manager::PasswordForm>> current_forms;
  password_manager::PasswordForm additional_form;
  current_forms.push_back(
      std::make_unique<password_manager::PasswordForm>(test_form_));
  current_forms.push_back(
      std::make_unique<password_manager::PasswordForm>(additional_form));

  std::u16string default_username = test_form_.username_value;

  std::vector<std::u16string> usernames;
  unsigned int selected_username =
      UpdatePasswordInfoBarDelegate::GetDisplayUsernames(
          current_forms, default_username, &usernames);
  std::u16string empty_username =
      l10n_util::GetStringUTF16(IDS_PASSWORD_MANAGER_EMPTY_LOGIN);

  EXPECT_EQ(default_username, usernames[selected_username]);
  EXPECT_THAT(usernames, ElementsAre(default_username, empty_username));
}
