// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/android/all_passwords_bottom_sheet_helper.h"

#include <string_view>

#include "base/strings/utf_string_conversions.h"
#include "base/test/mock_callback.h"
#include "chrome/browser/password_manager/password_manager_test_util.h"
#include "chrome/test/base/testing_profile.h"
#include "components/autofill/core/common/mojom/autofill_types.mojom-shared.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_store/test_password_store.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_task_environment.h"

using password_manager::PasswordForm;
using password_manager::TestPasswordStore;

constexpr char kExampleCom[] = "https://example.com";
constexpr char16_t kUsername[] = u"alice";
constexpr char16_t kPassword[] = u"password123";

namespace {

PasswordForm MakeSavedPassword(std::string_view signon_realm,
                               std::u16string_view username) {
  PasswordForm form;
  form.signon_realm = std::string(signon_realm);
  form.url = GURL(signon_realm);
  form.username_value = std::u16string(username);
  form.password_value = kPassword;
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

}  // namespace

class AllPasswordsBottomSheetHelperTest
    : public testing::Test,
      public testing::WithParamInterface<bool> {
 public:
  void SetUp() override {
    profile_.GetPrefs()->SetInteger(
        password_manager::prefs::kPasswordsUseUPMLocalAndSeparateStores,
        static_cast<int>(
            password_manager::prefs::UseUpmLocalAndSeparateStoresState::kOn));
    profile_store_ = CreateAndUseTestPasswordStore(&profile_);
    account_store_ = CreateAndUseTestAccountPasswordStore(&profile_);
  }

 protected:
  TestPasswordStore& profile_store() { return *profile_store_; }
  TestPasswordStore& account_store() { return *account_store_; }

  void RunUntilIdle() { task_env_.RunUntilIdle(); }

 private:
  content::BrowserTaskEnvironment task_env_;
  TestingProfile profile_;
  scoped_refptr<TestPasswordStore> profile_store_;
  scoped_refptr<TestPasswordStore> account_store_;
};

TEST_P(AllPasswordsBottomSheetHelperTest, CallbackIsCalledAfterFetch) {
  if (GetParam()) {
    account_store().AddLogin(MakeSavedPassword(kExampleCom, kUsername));
  } else {
    profile_store().AddLogin(MakeSavedPassword(kExampleCom, kUsername));
  }

  base::MockOnceClosure callback;
  EXPECT_CALL(callback, Run);

  AllPasswordsBottomSheetHelper all_passwords_helper(&profile_store(),
                                                     &account_store());
  all_passwords_helper.SetUpdateCallback(callback.Get());
  all_passwords_helper.SetLastFocusedFieldType(
      autofill::mojom::FocusedFieldType::kFillableUsernameField);

  RunUntilIdle();
}

TEST_P(AllPasswordsBottomSheetHelperTest, CallbackIsNotCalledForEmptyStore) {
  // Exceptions don't count towards stored passwords!
  if (GetParam()) {
    account_store().AddLogin(MakePasswordException(kExampleCom));
  } else {
    profile_store().AddLogin(MakePasswordException(kExampleCom));
  }

  base::MockOnceClosure callback;
  EXPECT_CALL(callback, Run).Times(0);

  AllPasswordsBottomSheetHelper all_passwords_helper(&profile_store(),
                                                     &account_store());
  all_passwords_helper.SetUpdateCallback(callback.Get());
  all_passwords_helper.SetLastFocusedFieldType(
      autofill::mojom::FocusedFieldType::kFillableUsernameField);

  RunUntilIdle();
}

TEST_P(AllPasswordsBottomSheetHelperTest, CallbackIsNotCalledIfUnset) {
  if (GetParam()) {
    account_store().AddLogin(MakeSavedPassword(kExampleCom, kUsername));
  } else {
    profile_store().AddLogin(MakeSavedPassword(kExampleCom, kUsername));
  }

  base::MockOnceClosure callback;
  EXPECT_CALL(callback, Run).Times(0);

  AllPasswordsBottomSheetHelper all_passwords_helper(&profile_store(),
                                                     &account_store());
  all_passwords_helper.SetLastFocusedFieldType(
      autofill::mojom::FocusedFieldType::kFillableUsernameField);

  RunUntilIdle();
}

TEST_P(AllPasswordsBottomSheetHelperTest, CallbackIsNotCalledForUnknownFields) {
  if (GetParam()) {
    account_store().AddLogin(MakeSavedPassword(kExampleCom, kUsername));
  } else {
    profile_store().AddLogin(MakeSavedPassword(kExampleCom, kUsername));
  }

  base::MockOnceClosure callback;
  EXPECT_CALL(callback, Run).Times(0);

  AllPasswordsBottomSheetHelper all_passwords_helper(&profile_store(),
                                                     &account_store());
  all_passwords_helper.SetUpdateCallback(callback.Get());
  all_passwords_helper.SetLastFocusedFieldType(
      autofill::mojom::FocusedFieldType::kUnknown);

  RunUntilIdle();
}

INSTANTIATE_TEST_SUITE_P(,
                         AllPasswordsBottomSheetHelperTest,
                         ::testing::Bool());
