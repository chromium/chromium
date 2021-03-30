// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/android/all_passwords_bottom_sheet_controller.h"

#include "base/strings/utf_string_conversions.h"
#include "base/test/mock_callback.h"
#include "base/types/pass_key.h"
#include "chrome/browser/password_manager/password_manager_test_util.h"
#include "chrome/browser/ui/android/passwords/all_passwords_bottom_sheet_view.h"
#include "chrome/test/base/testing_profile.h"
#include "components/autofill/core/common/mojom/autofill_types.mojom-forward.h"
#include "components/password_manager/core/browser/origin_credential_store.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_manager_test_utils.h"
#include "components/password_manager/core/browser/stub_password_manager_client.h"
#include "components/password_manager/core/browser/stub_password_manager_driver.h"
#include "components/password_manager/core/browser/test_password_store.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::Eq;
using ::testing::Pointee;
using ::testing::UnorderedElementsAre;

using password_manager::PasswordForm;
using password_manager::TestPasswordStore;
using password_manager::UiCredential;
using CallbackFunctionMock = testing::MockFunction<void()>;
using autofill::mojom::FocusedFieldType;

using DismissCallback = base::MockCallback<base::OnceCallback<void()>>;

using IsPublicSuffixMatch = UiCredential::IsPublicSuffixMatch;
using IsAffiliationBasedMatch = UiCredential::IsAffiliationBasedMatch;

constexpr char kExampleCom[] = "https://example.com";
constexpr char kExampleOrg[] = "http://www.example.org";
constexpr char kExampleDe[] = "https://www.example.de";

constexpr char kUsername1[] = "alice";
constexpr char kUsername2[] = "bob";

constexpr char kPassword[] = "password123";

class MockPasswordManagerDriver
    : public password_manager::StubPasswordManagerDriver {
 public:
  MOCK_METHOD(void,
              FillIntoFocusedField,
              (bool, const std::u16string&),
              (override));
};

class MockAllPasswordsBottomSheetView : public AllPasswordsBottomSheetView {
 public:
  MOCK_METHOD(void,
              Show,
              (const std::vector<std::unique_ptr<PasswordForm>>&,
               FocusedFieldType),
              (override));
};

class MockPasswordManagerClient
    : public password_manager::StubPasswordManagerClient {
 public:
  MOCK_METHOD(void, OnPasswordSelected, (const std::u16string&), (override));
};

UiCredential MakeUiCredential(const std::string& username,
                              const std::string& password) {
  return UiCredential(base::UTF8ToUTF16(username), base::UTF8ToUTF16(password),
                      url::Origin::Create(GURL(kExampleCom)),
                      IsPublicSuffixMatch(false),
                      IsAffiliationBasedMatch(false), base::Time());
}

PasswordForm MakeSavedPassword(const std::string& signon_realm,
                               const std::string& username) {
  PasswordForm form;
  form.signon_realm = signon_realm;
  form.url = GURL(signon_realm);
  form.username_value = base::ASCIIToUTF16(username);
  form.password_value = base::ASCIIToUTF16(kPassword);
  form.in_store = PasswordForm::Store::kProfileStore;
  return form;
}

PasswordForm MakePasswordException(const std::string& signon_realm) {
  PasswordForm form;
  form.blocked_by_user = true;
  form.signon_realm = signon_realm;
  form.url = GURL(signon_realm);
  form.in_store = PasswordForm::Store::kProfileStore;
  return form;
}

class AllPasswordsBottomSheetControllerTest : public testing::Test {
 protected:
  AllPasswordsBottomSheetControllerTest() {
    createAllPasswordsController(FocusedFieldType::kFillablePasswordField);
  }

  void createAllPasswordsController(
      autofill::mojom::FocusedFieldType focused_field_type) {
    std::unique_ptr<MockAllPasswordsBottomSheetView> mock_view_unique_ptr =
        std::make_unique<MockAllPasswordsBottomSheetView>();
    mock_view_ = mock_view_unique_ptr.get();
    all_passwords_controller_ =
        std::make_unique<AllPasswordsBottomSheetController>(
            base::PassKey<AllPasswordsBottomSheetControllerTest>(),
            std::move(mock_view_unique_ptr), driver_.AsWeakPtr(), store_.get(),
            dissmissal_callback_.Get(), focused_field_type,
            mock_pwd_manager_client_.get());
  }

  MockPasswordManagerDriver& driver() { return driver_; }

  TestPasswordStore& store() { return *store_; }

  MockAllPasswordsBottomSheetView& view() { return *mock_view_; }

  AllPasswordsBottomSheetController* all_passwords_controller() {
    return all_passwords_controller_.get();
  }

  DismissCallback& dismissal_callback() { return dissmissal_callback_; }

  void RunUntilIdle() { task_env_.RunUntilIdle(); }

  MockPasswordManagerClient& client() {
    return *mock_pwd_manager_client_.get();
  }

 private:
  content::BrowserTaskEnvironment task_env_;
  MockPasswordManagerDriver driver_;
  TestingProfile profile_;
  scoped_refptr<TestPasswordStore> store_ =
      CreateAndUseTestPasswordStore(&profile_);
  MockAllPasswordsBottomSheetView* mock_view_;
  DismissCallback dissmissal_callback_;
  std::unique_ptr<AllPasswordsBottomSheetController> all_passwords_controller_;
  std::unique_ptr<MockPasswordManagerClient> mock_pwd_manager_client_ =
      std::make_unique<MockPasswordManagerClient>();
};

TEST_F(AllPasswordsBottomSheetControllerTest, Show) {
  auto form1 = MakeSavedPassword(kExampleCom, kUsername1);
  auto form2 = MakeSavedPassword(kExampleCom, kUsername2);
  auto form3 = MakeSavedPassword(kExampleOrg, kUsername1);
  auto form4 = MakeSavedPassword(kExampleOrg, kUsername2);

  store().AddLogin(form1);
  store().AddLogin(form2);
  store().AddLogin(form3);
  store().AddLogin(form4);
  // Exceptions are not shown. Sites where saving is disabled still show pwds.
  store().AddLogin(MakePasswordException(kExampleDe));
  store().AddLogin(MakePasswordException(kExampleCom));

  EXPECT_CALL(view(),
              Show(UnorderedElementsAre(Pointee(Eq(form1)), Pointee(Eq(form2)),
                                        Pointee(Eq(form3)), Pointee(Eq(form4))),
                   FocusedFieldType::kFillablePasswordField));
  all_passwords_controller()->Show();

  // Show method uses the store which has async work.
  RunUntilIdle();
}

TEST_F(AllPasswordsBottomSheetControllerTest, OnCredentialSelected) {
  UiCredential credential = MakeUiCredential(kUsername1, kPassword);

  EXPECT_CALL(driver(),
              FillIntoFocusedField(true, base::ASCIIToUTF16(kPassword)));

  all_passwords_controller()->OnCredentialSelected(
      base::UTF8ToUTF16(kUsername1), base::UTF8ToUTF16(kPassword));
}

TEST_F(AllPasswordsBottomSheetControllerTest, OnDismiss) {
  EXPECT_CALL(dismissal_callback(), Run());
  all_passwords_controller()->OnDismiss();
}

TEST_F(AllPasswordsBottomSheetControllerTest,
       OnCredentialSelectedTriggersPhishGuard) {
  EXPECT_CALL(client(), OnPasswordSelected(base::UTF8ToUTF16(kPassword)));

  all_passwords_controller()->OnCredentialSelected(
      base::UTF8ToUTF16(kUsername1), base::UTF8ToUTF16(kPassword));
}

TEST_F(AllPasswordsBottomSheetControllerTest,
       PhishGuardIsNotCalledForUsername) {
  createAllPasswordsController(FocusedFieldType::kFillableUsernameField);
  EXPECT_CALL(client(), OnPasswordSelected).Times(0);

  all_passwords_controller()->OnCredentialSelected(
      base::UTF8ToUTF16(kUsername1), base::UTF8ToUTF16(kPassword));
}
