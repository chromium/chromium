// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/android/all_passwords_bottom_sheet_helper.h"

#include "base/strings/string_piece.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/mock_callback.h"
#include "chrome/browser/password_manager/password_manager_test_util.h"
#include "chrome/test/base/testing_profile.h"
#include "components/autofill/core/common/mojom/autofill_types.mojom-shared.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_store/test_password_store.h"
#include "content/public/test/browser_task_environment.h"

using password_manager::PasswordForm;
using password_manager::TestPasswordStore;

constexpr char kExampleCom[] = "https://example.com";
constexpr char16_t kUsername[] = u"alice";
constexpr char16_t kPassword[] = u"password123";

namespace {

PasswordForm MakeSavedPassword(base::StringPiece signon_realm,
                               base::StringPiece16 username) {
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

class AllPasswordsBottomSheetHelperTest : public testing::Test {
 protected:
  TestPasswordStore& store() { return *store_; }

  void RunUntilIdle() { task_env_.RunUntilIdle(); }

 private:
  content::BrowserTaskEnvironment task_env_;
  TestingProfile profile_;
  scoped_refptr<TestPasswordStore> store_ =
      CreateAndUseTestPasswordStore(&profile_);
};

TEST_F(AllPasswordsBottomSheetHelperTest, CallbackIsCalledAfterFetch) {
  store().AddLogin(MakeSavedPassword(kExampleCom, kUsername));

  base::MockOnceClosure callback;
  EXPECT_CALL(callback, Run);

  AllPasswordsBottomSheetHelper all_passwords_helper(&store());
  all_passwords_helper.SetUpdateCallback(callback.Get());
  all_passwords_helper.SetLastFocusedFieldType(
      autofill::mojom::FocusedFieldType::kFillableUsernameField);

  RunUntilIdle();
}

TEST_F(AllPasswordsBottomSheetHelperTest, CallbackIsNotCalledForEmptyStore) {
  // Exceptions don't count towards stored passwords!
  store().AddLogin(MakePasswordException(kExampleCom));

  base::MockOnceClosure callback;
  EXPECT_CALL(callback, Run).Times(0);

  AllPasswordsBottomSheetHelper all_passwords_helper(&store());
  all_passwords_helper.SetUpdateCallback(callback.Get());
  all_passwords_helper.SetLastFocusedFieldType(
      autofill::mojom::FocusedFieldType::kFillableUsernameField);

  RunUntilIdle();
}

TEST_F(AllPasswordsBottomSheetHelperTest, CallbackIsNotCalledIfUnset) {
  store().AddLogin(MakeSavedPassword(kExampleCom, kUsername));

  base::MockOnceClosure callback;
  EXPECT_CALL(callback, Run).Times(0);

  AllPasswordsBottomSheetHelper all_passwords_helper(&store());
  all_passwords_helper.SetLastFocusedFieldType(
      autofill::mojom::FocusedFieldType::kFillableUsernameField);

  RunUntilIdle();
}
TEST_F(AllPasswordsBottomSheetHelperTest, CallbackIsNotCalledForUnknownFields) {
  store().AddLogin(MakeSavedPassword(kExampleCom, kUsername));

  base::MockOnceClosure callback;
  EXPECT_CALL(callback, Run).Times(0);

  AllPasswordsBottomSheetHelper all_passwords_helper(&store());
  all_passwords_helper.SetUpdateCallback(callback.Get());
  all_passwords_helper.SetLastFocusedFieldType(
      autofill::mojom::FocusedFieldType::kUnknown);

  RunUntilIdle();
}
