// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/android/all_passwords_bottom_sheet_controller.h"

#include "base/strings/utf_string_conversions.h"
#include "base/test/mock_callback.h"
#include "base/util/type_safety/pass_key.h"
#include "chrome/browser/password_manager/password_manager_test_util.h"
#include "chrome/browser/ui/android/passwords/all_passwords_bottom_sheet_view.h"
#include "chrome/test/base/testing_profile.h"
#include "components/autofill/core/common/mojom/autofill_types.mojom-forward.h"
#include "components/autofill/core/common/password_form.h"
#include "components/password_manager/core/browser/origin_credential_store.h"
#include "components/password_manager/core/browser/password_manager_test_utils.h"
#include "components/password_manager/core/browser/stub_password_manager_driver.h"
#include "components/password_manager/core/browser/test_password_store.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::Eq;
using ::testing::Pointee;
using ::testing::UnorderedElementsAre;

using autofill::PasswordForm;
using password_manager::TestPasswordStore;
using password_manager::UiCredential;
using CallbackFunctionMock = testing::MockFunction<void()>;
using autofill::mojom::FocusedFieldType;

using DismissCallback = base::MockCallback<base::OnceCallback<void()>>;

using IsPublicSuffixMatch = UiCredential::IsPublicSuffixMatch;
using IsAffiliationBasedMatch = UiCredential::IsAffiliationBasedMatch;

constexpr char kExampleCom[] = "https://example.com";
constexpr char kExampleOrg[] = "http://www.example.org";

constexpr char kUsername1[] = "alice";
constexpr char kUsername2[] = "bob";

constexpr char kPassword[] = "password123";

class MockPasswordManagerDriver
    : public password_manager::StubPasswordManagerDriver {
 public:
  MOCK_METHOD(void,
              FillIntoFocusedField,
              (bool, const base::string16&),
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

UiCredential MakeUiCredential(const std::string& username,
                              const std::string& password) {
  return UiCredential(base::UTF8ToUTF16(username), base::UTF8ToUTF16(password),
                      url::Origin::Create(GURL(kExampleCom)),
                      IsPublicSuffixMatch(false),
                      IsAffiliationBasedMatch(false));
}

PasswordForm MakeSavedPassword(const std::string& signon_realm,
                               const std::string& username) {
  PasswordForm form;
  form.signon_realm = std::string(signon_realm);
  form.url = GURL(signon_realm);
  form.username_value = base::ASCIIToUTF16(username);
  form.password_value = base::ASCIIToUTF16(kPassword);
  form.username_element = base::ASCIIToUTF16("");
  form.in_store = PasswordForm::Store::kProfileStore;
  return form;
}

class AllPasswordsBottomSheetControllerTest : public testing::Test {
 protected:
  AllPasswordsBottomSheetControllerTest() {
    std::unique_ptr<MockAllPasswordsBottomSheetView> mock_view_unique_ptr =
        std::make_unique<MockAllPasswordsBottomSheetView>();
    mock_view_ = mock_view_unique_ptr.get();
    all_passwords_controller_ =
        std::make_unique<AllPasswordsBottomSheetController>(
            util::PassKey<AllPasswordsBottomSheetControllerTest>(),
            std::move(mock_view_unique_ptr), driver_.AsWeakPtr(), store_.get(),
            dissmissal_callback_.Get(),
            FocusedFieldType::kFillablePasswordField);
  }

  MockPasswordManagerDriver& driver() { return driver_; }

  TestPasswordStore& store() { return *store_; }

  MockAllPasswordsBottomSheetView& view() { return *mock_view_; }

  AllPasswordsBottomSheetController* all_passwords_controller() {
    return all_passwords_controller_.get();
  }

  DismissCallback& dismissal_callback() { return dissmissal_callback_; }

  void RunUntilIdle() { task_env_.RunUntilIdle(); }

 private:
  content::BrowserTaskEnvironment task_env_;
  MockPasswordManagerDriver driver_;
  TestingProfile profile_;
  scoped_refptr<TestPasswordStore> store_ =
      CreateAndUseTestPasswordStore(&profile_);
  MockAllPasswordsBottomSheetView* mock_view_;
  DismissCallback dissmissal_callback_;
  std::unique_ptr<AllPasswordsBottomSheetController> all_passwords_controller_;
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
