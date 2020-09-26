// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/password_store_x.h"

#include <stddef.h>

#include <string>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/sequenced_task_runner.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "chrome/test/base/testing_browser_process.h"
#include "components/os_crypt/os_crypt.h"
#include "components/os_crypt/os_crypt_mocker.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"
#include "components/password_manager/core/browser/password_manager_test_utils.h"
#include "components/password_manager/core/browser/password_store_change.h"
#include "components/password_manager/core/browser/password_store_consumer.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using autofill::PasswordForm;
using password_manager::FormRetrievalResult;
using password_manager::PasswordStoreChange;
using password_manager::PasswordStoreChangeList;
using password_manager::PrimaryKeyToFormMap;
using password_manager::UnorderedPasswordFormElementsAre;
using testing::ElementsAreArray;
using testing::Field;
using testing::IsEmpty;
using testing::Pointee;
using testing::UnorderedElementsAre;

namespace {

const char kPassword[] = "password_value";
const char kUsername[] = "username_value";
const char kAnotherUsername[] = "another_username_value";
class MockPasswordStoreConsumer
    : public password_manager::PasswordStoreConsumer {
 public:
  MOCK_METHOD1(OnGetPasswordStoreResultsConstRef,
               void(const std::vector<std::unique_ptr<PasswordForm>>&));

  // GMock cannot mock methods with move-only args.
  void OnGetPasswordStoreResults(
      std::vector<std::unique_ptr<PasswordForm>> results) override {
    OnGetPasswordStoreResultsConstRef(results);
  }
};

PasswordForm MakePasswordForm() {
  PasswordForm form;
  form.url = GURL("http://www.origin.com");
  form.username_element = base::UTF8ToUTF16("username_element");
  form.username_value = base::UTF8ToUTF16(kUsername);
  form.password_element = base::UTF8ToUTF16("password_element");
  form.password_value = base::UTF8ToUTF16(kPassword);
  form.signon_realm = form.url.GetOrigin().spec();
  form.in_store = autofill::PasswordForm::Store::kProfileStore;
  return form;
}

}  // namespace

class PasswordStoreXTest : public testing::Test {
 public:
  PasswordStoreXTest() {
    ignore_result(temp_dir_.CreateUniqueTempDir());
    fake_pref_service_.registry()->RegisterIntegerPref(
        password_manager::prefs::kMigrationToLoginDBStep,
        PasswordStoreX::NOT_ATTEMPTED);
    OSCryptMocker::SetUp();
  }

  ~PasswordStoreXTest() override { OSCryptMocker::TearDown(); }

  PrefService* fake_pref_service() { return &fake_pref_service_; }

  base::FilePath test_login_db_file_path() const {
    return temp_dir_.GetPath().Append(FILE_PATH_LITERAL("login_test"));
  }

  void WaitForPasswordStore() { task_environment_.RunUntilIdle(); }

 protected:
  base::HistogramTester histogram_tester_;

 private:
  TestingPrefServiceSimple fake_pref_service_;
  base::test::TaskEnvironment task_environment_;
  base::ScopedTempDir temp_dir_;

  DISALLOW_COPY_AND_ASSIGN(PasswordStoreXTest);
};

TEST_F(PasswordStoreXTest, MigrationCompleted) {
  IntegerPrefMember migration_step_pref_;
  migration_step_pref_.Init(password_manager::prefs::kMigrationToLoginDBStep,
                            fake_pref_service());
  // Signal that the migration has been completed.
  migration_step_pref_.SetValue(PasswordStoreX::LOGIN_DB_REPLACED);

  // Add existing credential into loginDB. It should be the only thing that's
  // available in the store.
  auto login_db = std::make_unique<password_manager::LoginDatabase>(
      test_login_db_file_path(), password_manager::IsAccountStore(false));
  ASSERT_TRUE(login_db->Init());
  ignore_result(login_db->AddLogin(MakePasswordForm()));
  login_db.reset();

  // Create the store.
  login_db = std::make_unique<password_manager::LoginDatabase>(
      test_login_db_file_path(), password_manager::IsAccountStore(false));
  scoped_refptr<PasswordStoreX> store =
      new PasswordStoreX(std::move(login_db), fake_pref_service());
  store->Init(nullptr);

  // Check the contents are still around.
  MockPasswordStoreConsumer consumer;
  EXPECT_CALL(consumer, OnGetPasswordStoreResultsConstRef(
                            testing::ElementsAre(Pointee(MakePasswordForm()))));
  store->GetAutofillableLogins(&consumer);

  WaitForPasswordStore();
  store->ShutdownOnUIThread();
  store.reset();
  WaitForPasswordStore();

  // Check if the database is encrypted.
  password_manager::LoginDatabase login_db2(
      test_login_db_file_path(), password_manager::IsAccountStore(false));
  // Disable encryption.
  login_db2.disable_encryption();
  EXPECT_TRUE(login_db2.Init());
  // Read the password again.
  std::vector<std::unique_ptr<PasswordForm>> stored_forms;
  EXPECT_TRUE(login_db2.GetAutofillableLogins(&stored_forms));
  EXPECT_EQ(1U, stored_forms.size());
  // Password values don't match because they have been stored encrypted and
  // read unencrypted.
  EXPECT_NE(kPassword, base::UTF16ToUTF8(stored_forms[0]->password_value));
  EXPECT_THAT(migration_step_pref_.GetValue(),
              PasswordStoreX::LOGIN_DB_REPLACED);
}

TEST_F(PasswordStoreXTest, MigrationNotAttemptedEmptyDB) {
  IntegerPrefMember migration_step_pref_;
  migration_step_pref_.Init(password_manager::prefs::kMigrationToLoginDBStep,
                            fake_pref_service());
  // Signal that the migration has not been attempted.
  migration_step_pref_.SetValue(PasswordStoreX::NOT_ATTEMPTED);

  // Create the store with an empty database.
  auto login_db = std::make_unique<password_manager::LoginDatabase>(
      test_login_db_file_path(), password_manager::IsAccountStore(false));
  password_manager::LoginDatabase* login_db_ptr = login_db.get();

  scoped_refptr<PasswordStoreX> store =
      new PasswordStoreX(std::move(login_db), fake_pref_service());
  store->Init(nullptr);
  WaitForPasswordStore();

  // Add a password to the db.
  PasswordStoreChangeList changes = login_db_ptr->AddLogin(MakePasswordForm());
  EXPECT_EQ(1U, changes.size());

  store->ShutdownOnUIThread();
  store.reset();
  WaitForPasswordStore();

  // Check if the database is encrypted.
  password_manager::LoginDatabase login_db2(
      test_login_db_file_path(), password_manager::IsAccountStore(false));
  login_db2.disable_encryption();
  EXPECT_TRUE(login_db2.Init());
  // Read the password again.
  std::vector<std::unique_ptr<PasswordForm>> stored_forms;
  EXPECT_TRUE(login_db2.GetAutofillableLogins(&stored_forms));
  EXPECT_EQ(1U, stored_forms.size());
  // Password values don't match because they have been stored encrypted and
  // read unencrypted.
  EXPECT_NE(kPassword, base::UTF16ToUTF8(stored_forms[0]->password_value));
  EXPECT_THAT(migration_step_pref_.GetValue(),
              PasswordStoreX::LOGIN_DB_REPLACED);
}

// If the login database contains unmigrated entries, they will be migrated into
// encryption.
TEST_F(PasswordStoreXTest, MigrationNotAttemptedNonEmptyDB) {
  IntegerPrefMember migration_step_pref_;
  migration_step_pref_.Init(password_manager::prefs::kMigrationToLoginDBStep,
                            fake_pref_service());
  // Signal that the migration has not been attempted.
  migration_step_pref_.SetValue(PasswordStoreX::NOT_ATTEMPTED);

  // Add existing credential into loginDB.
  auto existing_login = MakePasswordForm();
  auto login_db = std::make_unique<password_manager::LoginDatabase>(
      test_login_db_file_path(), password_manager::IsAccountStore(false));
  login_db->disable_encryption();
  ASSERT_TRUE(login_db->Init());
  ignore_result(login_db->AddLogin(existing_login));
  login_db.reset();

  // Create the store with a non-empty database.
  login_db = std::make_unique<password_manager::LoginDatabase>(
      test_login_db_file_path(), password_manager::IsAccountStore(false));
  password_manager::LoginDatabase* login_db_ptr = login_db.get();

  scoped_refptr<PasswordStoreX> store =
      new PasswordStoreX(std::move(login_db), fake_pref_service());
  store->Init(nullptr);
  WaitForPasswordStore();

  // Add another password to the db.
  PasswordForm new_login = MakePasswordForm();
  new_login.username_value = base::UTF8ToUTF16(kAnotherUsername);
  PasswordStoreChangeList changes = login_db_ptr->AddLogin(new_login);
  EXPECT_EQ(1U, changes.size());

  store->ShutdownOnUIThread();
  store.reset();
  WaitForPasswordStore();

  // Check that the database is encrypted.
  password_manager::LoginDatabase login_db2(
      test_login_db_file_path(), password_manager::IsAccountStore(false));
  // Disable encryption and get the raw values. An encrypted database would have
  // read both encrypted and unencrypted entries.
  login_db2.disable_encryption();
  EXPECT_TRUE(login_db2.Init());
  // Read the password again.
  std::vector<std::unique_ptr<PasswordForm>> stored_forms;
  EXPECT_TRUE(login_db2.GetAutofillableLogins(&stored_forms));
  EXPECT_EQ(2U, stored_forms.size());

  // Encrypt the passwords to compare them to the raw (i.e. encrypted) values.
  std::string existing_password_encrypted, new_password_encrypted;
  OSCrypt::EncryptString16(existing_login.password_value,
                           &existing_password_encrypted);
  OSCrypt::EncryptString16(new_login.password_value, &new_password_encrypted);
  EXPECT_THAT(
      stored_forms,
      UnorderedElementsAre(
          Pointee(Field(&PasswordForm::password_value,
                        base::UTF8ToUTF16(existing_password_encrypted))),
          Pointee(Field(&PasswordForm::password_value,
                        base::UTF8ToUTF16(new_password_encrypted)))));

  EXPECT_THAT(migration_step_pref_.GetValue(),
              PasswordStoreX::LOGIN_DB_REPLACED);
}
