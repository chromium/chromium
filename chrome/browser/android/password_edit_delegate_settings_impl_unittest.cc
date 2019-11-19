// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/password_edit_delegate_settings_impl.h"

#include <memory>
#include <string>
#include <vector>

#include "base/memory/scoped_refptr.h"
#include "base/strings/strcat.h"
#include "base/strings/string16.h"
#include "base/strings/string_piece_forward.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/android/password_edit_delegate_settings_impl.h"
#include "chrome/browser/password_manager/password_store_factory.h"
#include "chrome/test/base/testing_profile.h"
#include "components/autofill/core/common/password_form.h"
#include "components/password_manager/core/browser/password_list_sorter.h"
#include "components/password_manager/core/browser/password_manager_test_utils.h"
#include "components/password_manager/core/browser/test_password_store.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using testing::ElementsAre;
using testing::Pair;
using testing::UnorderedElementsAre;

namespace {

constexpr char kExampleCom[] = "https://example.com/";
constexpr char kExampleOrg[] = "https://example.org/";
constexpr char kNewPassword[] = "new_pass";
constexpr char kNewUser[] = "new_user";
constexpr char kPassword1[] = "pass";
constexpr char kPassword2[] = "pass2";
constexpr char kUsername1[] = "user";
constexpr char kUsername2[] = "user2";

std::vector<std::pair<std::string, std::string>> GetUsernamesAndPasswords(
    const std::vector<autofill::PasswordForm>& forms) {
  std::vector<std::pair<std::string, std::string>> result;
  result.reserve(forms.size());
  for (const auto& form : forms) {
    result.emplace_back(base::UTF16ToUTF8(form.username_value),
                        base::UTF16ToUTF8(form.password_value));
  }

  return result;
}

autofill::PasswordForm MakeSavedForm(const GURL& origin,
                                     base::StringPiece username,
                                     base::StringPiece password) {
  autofill::PasswordForm form;
  form.origin = origin;
  form.signon_realm = origin.GetOrigin().spec();
  form.username_element = base::ASCIIToUTF16("Email");
  form.username_value = base::ASCIIToUTF16(username);
  form.password_element = base::ASCIIToUTF16("Passwd");
  form.password_value = base::ASCIIToUTF16(password);
  return form;
}

std::vector<std::unique_ptr<autofill::PasswordForm>> ExtractEquivalentForms(
    const autofill::PasswordForm& edited_form,
    const std::vector<autofill::PasswordForm>& saved_forms) {
  std::string sort_key = password_manager::CreateSortKey(edited_form);
  std::vector<std::unique_ptr<autofill::PasswordForm>> equivalent_forms;
  for (const autofill::PasswordForm& form : saved_forms) {
    if (password_manager::CreateSortKey(form) == sort_key) {
      equivalent_forms.push_back(
          std::make_unique<autofill::PasswordForm>(form));
    }
  }
  return equivalent_forms;
}

std::vector<base::string16> ExtractUsernamesSameOrigin(
    const GURL& origin,
    const std::vector<autofill::PasswordForm>& saved_forms) {
  std::vector<base::string16> existing_usernames;
  for (const auto& form : saved_forms) {
    if (form.origin == origin) {
      existing_usernames.push_back(form.username_value);
    }
  }
  return existing_usernames;
}

}  // namespace

class PasswordEditDelegateSettingsImplTest : public testing::Test {
 protected:
  PasswordEditDelegateSettingsImplTest() = default;

  ~PasswordEditDelegateSettingsImplTest() override {
    store_->ShutdownOnUIThread();
    task_environment_.RunUntilIdle();
  }

  password_manager::TestPasswordStore* GetStore() { return store_.get(); }

  void RunUntilIdle() { task_environment_.RunUntilIdle(); }

  std::unique_ptr<PasswordEditDelegateSettingsImpl> CreateTestDelegate(
      const std::vector<std::unique_ptr<autofill::PasswordForm>>& forms,
      std::vector<base::string16> existing_usernames);

  const std::vector<autofill::PasswordForm>& GetStoredPasswordsForRealm(
      base::StringPiece signon_realm);

  void InitializeStoreWithForms(
      const std::vector<autofill::PasswordForm>& saved_forms);

 private:
  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;
  scoped_refptr<password_manager::TestPasswordStore> store_ =
      base::WrapRefCounted(static_cast<password_manager::TestPasswordStore*>(
          PasswordStoreFactory::GetInstance()
              ->SetTestingFactoryAndUse(
                  &profile_,
                  base::BindRepeating(&password_manager::BuildPasswordStore<
                                      content::BrowserContext,
                                      password_manager::TestPasswordStore>))
              .get()));
};

std::unique_ptr<PasswordEditDelegateSettingsImpl>
PasswordEditDelegateSettingsImplTest::CreateTestDelegate(
    const std::vector<std::unique_ptr<autofill::PasswordForm>>& forms,
    std::vector<base::string16> existing_usernames) {
  return std::make_unique<PasswordEditDelegateSettingsImpl>(
      &profile_, forms, std::move(existing_usernames));
}

const std::vector<autofill::PasswordForm>&
PasswordEditDelegateSettingsImplTest::GetStoredPasswordsForRealm(
    base::StringPiece signon_realm) {
  const auto& stored_passwords = GetStore()->stored_passwords();
  auto for_realm_it = stored_passwords.find(signon_realm);
  return for_realm_it->second;
}

void PasswordEditDelegateSettingsImplTest::InitializeStoreWithForms(
    const std::vector<autofill::PasswordForm>& saved_forms) {
  for (const auto& form : saved_forms) {
    GetStore()->AddLogin(form);
  }
  RunUntilIdle();
}

TEST_F(PasswordEditDelegateSettingsImplTest, EditPassword) {
  autofill::PasswordForm edited_form =
      MakeSavedForm(GURL(kExampleCom), kUsername1, kPassword1);
  std::vector<autofill::PasswordForm> saved_forms = {edited_form};
  InitializeStoreWithForms(saved_forms);
  std::vector<std::unique_ptr<autofill::PasswordForm>> forms_to_change =
      ExtractEquivalentForms(edited_form, saved_forms);
  std::vector<base::string16> existing_usernames =
      ExtractUsernamesSameOrigin(edited_form.origin, saved_forms);

  std::unique_ptr<PasswordEditDelegateSettingsImpl> password_edit_delegate =
      CreateTestDelegate(forms_to_change, std::move(existing_usernames));
  password_edit_delegate->EditSavedPassword(base::ASCIIToUTF16(kUsername1),
                                            base::ASCIIToUTF16(kNewPassword));
  RunUntilIdle();

  EXPECT_THAT(GetUsernamesAndPasswords(GetStoredPasswordsForRealm(kExampleCom)),
              ElementsAre(Pair(kUsername1, kNewPassword)));
}

TEST_F(PasswordEditDelegateSettingsImplTest, EditUsername) {
  autofill::PasswordForm edited_form =
      MakeSavedForm(GURL(kExampleCom), kUsername1, kPassword1);

  std::vector<autofill::PasswordForm> saved_forms = {edited_form};
  InitializeStoreWithForms(saved_forms);

  std::vector<std::unique_ptr<autofill::PasswordForm>> forms_to_change =
      ExtractEquivalentForms(edited_form, saved_forms);
  std::vector<base::string16> existing_usernames =
      ExtractUsernamesSameOrigin(edited_form.origin, saved_forms);

  std::unique_ptr<PasswordEditDelegateSettingsImpl> password_edit_delegate =
      CreateTestDelegate(forms_to_change, std::move(existing_usernames));
  password_edit_delegate->EditSavedPassword(base::ASCIIToUTF16(kNewUser),
                                            base::ASCIIToUTF16(kPassword1));
  RunUntilIdle();

  EXPECT_THAT(GetUsernamesAndPasswords(GetStoredPasswordsForRealm(kExampleCom)),
              ElementsAre(Pair(kNewUser, kPassword1)));
}

TEST_F(PasswordEditDelegateSettingsImplTest, EditUsernameAndPassword) {
  autofill::PasswordForm edited_form =
      MakeSavedForm(GURL(kExampleCom), kUsername1, kPassword1);

  std::vector<autofill::PasswordForm> saved_forms = {edited_form};
  InitializeStoreWithForms(saved_forms);

  std::vector<std::unique_ptr<autofill::PasswordForm>> forms_to_change =
      ExtractEquivalentForms(edited_form, saved_forms);
  std::vector<base::string16> existing_usernames =
      ExtractUsernamesSameOrigin(edited_form.origin, saved_forms);

  std::unique_ptr<PasswordEditDelegateSettingsImpl> password_edit_delegate =
      CreateTestDelegate(forms_to_change, std::move(existing_usernames));
  password_edit_delegate->EditSavedPassword(base::ASCIIToUTF16(kNewUser),
                                            base::ASCIIToUTF16(kNewPassword));
  RunUntilIdle();

  EXPECT_THAT(GetUsernamesAndPasswords(GetStoredPasswordsForRealm(kExampleCom)),
              ElementsAre(Pair(kNewUser, kNewPassword)));
}

TEST_F(PasswordEditDelegateSettingsImplTest, RejectSameUsernameForSameRealm) {
  autofill::PasswordForm edited_form =
      MakeSavedForm(GURL(kExampleCom), kUsername1, kPassword1);
  autofill::PasswordForm other_form =
      MakeSavedForm(GURL(kExampleCom), kUsername2, kPassword2);
  std::vector<autofill::PasswordForm> saved_forms = {edited_form, other_form};
  InitializeStoreWithForms(saved_forms);

  std::vector<std::unique_ptr<autofill::PasswordForm>> forms_to_change =
      ExtractEquivalentForms(edited_form, saved_forms);
  std::vector<base::string16> existing_usernames =
      ExtractUsernamesSameOrigin(edited_form.origin, saved_forms);

  std::unique_ptr<PasswordEditDelegateSettingsImpl> password_edit_delegate =
      CreateTestDelegate(forms_to_change, std::move(existing_usernames));
  password_edit_delegate->EditSavedPassword(base::ASCIIToUTF16(kUsername2),
                                            base::ASCIIToUTF16(kPassword1));
  RunUntilIdle();
  EXPECT_THAT(GetUsernamesAndPasswords(GetStoredPasswordsForRealm(kExampleCom)),
              UnorderedElementsAre(Pair(kUsername1, kPassword1),
                                   Pair(kUsername2, kPassword2)));
}

TEST_F(PasswordEditDelegateSettingsImplTest, UpdateDuplicates) {
  autofill::PasswordForm edited_form = MakeSavedForm(
      GURL(base::StrCat({kExampleCom, "pathA"})), kUsername1, kPassword1);
  autofill::PasswordForm other_form = MakeSavedForm(
      GURL(base::StrCat({kExampleCom, "pathB"})), kUsername1, kPassword1);
  std::vector<autofill::PasswordForm> saved_forms = {edited_form, other_form};
  InitializeStoreWithForms(saved_forms);

  std::vector<std::unique_ptr<autofill::PasswordForm>> forms_to_change =
      ExtractEquivalentForms(edited_form, saved_forms);
  std::vector<base::string16> existing_usernames =
      ExtractUsernamesSameOrigin(edited_form.origin, saved_forms);

  std::unique_ptr<PasswordEditDelegateSettingsImpl> password_edit_delegate =
      CreateTestDelegate(forms_to_change, std::move(existing_usernames));
  password_edit_delegate->EditSavedPassword(base::ASCIIToUTF16(kNewUser),
                                            base::ASCIIToUTF16(kNewPassword));
  RunUntilIdle();

  EXPECT_THAT(GetUsernamesAndPasswords(GetStoredPasswordsForRealm(kExampleCom)),
              UnorderedElementsAre(Pair(kNewUser, kNewPassword),
                                   Pair(kNewUser, kNewPassword)));
}

TEST_F(PasswordEditDelegateSettingsImplTest,
       EditUsernameForTheRightCredential) {
  autofill::PasswordForm edited_form =
      MakeSavedForm(GURL(kExampleCom), kUsername1, kPassword1);
  autofill::PasswordForm other_form1 =
      MakeSavedForm(GURL(kExampleCom), kUsername2, kPassword2);
  autofill::PasswordForm other_form2 =
      MakeSavedForm(GURL(kExampleOrg), kUsername1, kPassword1);
  autofill::PasswordForm other_form3 =
      MakeSavedForm(GURL(kExampleOrg), kUsername2, kPassword2);
  std::vector<autofill::PasswordForm> saved_forms = {edited_form, other_form1,
                                                     other_form2, other_form3};
  InitializeStoreWithForms(saved_forms);

  std::vector<std::unique_ptr<autofill::PasswordForm>> forms_to_change =
      ExtractEquivalentForms(edited_form, saved_forms);
  std::vector<base::string16> existing_usernames =
      ExtractUsernamesSameOrigin(edited_form.origin, saved_forms);

  std::unique_ptr<PasswordEditDelegateSettingsImpl> password_edit_delegate =
      CreateTestDelegate(forms_to_change, std::move(existing_usernames));
  password_edit_delegate->EditSavedPassword(base::ASCIIToUTF16(kNewUser),
                                            base::ASCIIToUTF16(kPassword1));
  RunUntilIdle();

  EXPECT_THAT(GetUsernamesAndPasswords(GetStoredPasswordsForRealm(kExampleCom)),
              UnorderedElementsAre(Pair(kNewUser, kPassword1),
                                   Pair(kUsername2, kPassword2)));
  EXPECT_THAT(GetUsernamesAndPasswords(GetStoredPasswordsForRealm(kExampleOrg)),
              UnorderedElementsAre(Pair(kUsername1, kPassword1),
                                   Pair(kUsername2, kPassword2)));
}

TEST_F(PasswordEditDelegateSettingsImplTest,
       EditPasswordForTheRightCredential) {
  autofill::PasswordForm edited_form =
      MakeSavedForm(GURL(kExampleCom), kUsername1, kPassword1);
  autofill::PasswordForm other_form1 =
      MakeSavedForm(GURL(kExampleCom), kUsername2, kPassword2);
  autofill::PasswordForm other_form2 =
      MakeSavedForm(GURL(kExampleOrg), kUsername1, kPassword1);
  autofill::PasswordForm other_form3 =
      MakeSavedForm(GURL(kExampleOrg), kUsername2, kPassword2);
  std::vector<autofill::PasswordForm> saved_forms = {edited_form, other_form1,
                                                     other_form2, other_form3};
  InitializeStoreWithForms(saved_forms);

  std::vector<std::unique_ptr<autofill::PasswordForm>> forms_to_change =
      ExtractEquivalentForms(edited_form, saved_forms);
  std::vector<base::string16> existing_usernames =
      ExtractUsernamesSameOrigin(edited_form.origin, saved_forms);

  std::unique_ptr<PasswordEditDelegateSettingsImpl> password_edit_delegate =
      CreateTestDelegate(forms_to_change, std::move(existing_usernames));
  password_edit_delegate->EditSavedPassword(base::ASCIIToUTF16(kUsername1),
                                            base::ASCIIToUTF16(kNewPassword));
  RunUntilIdle();

  EXPECT_THAT(GetUsernamesAndPasswords(GetStoredPasswordsForRealm(kExampleCom)),
              UnorderedElementsAre(Pair(kUsername1, kNewPassword),
                                   Pair(kUsername2, kPassword2)));
  EXPECT_THAT(GetUsernamesAndPasswords(GetStoredPasswordsForRealm(kExampleOrg)),
              UnorderedElementsAre(Pair(kUsername1, kPassword1),
                                   Pair(kUsername2, kPassword2)));
}
