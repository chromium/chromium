// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/password_store_mac.h"

#include <utility>

#include "base/files/scoped_temp_dir.h"
#include "base/macros.h"
#include "base/scoped_observer.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_task_environment.h"
#include "chrome/browser/prefs/browser_prefs.h"
#include "components/os_crypt/os_crypt_mocker.h"
#include "components/password_manager/core/browser/login_database.h"
#include "components/password_manager/core/browser/password_manager_test_utils.h"
#include "components/password_manager/core/browser/password_store_consumer.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

using autofill::PasswordForm;
using password_manager::MigrationStatus;
using password_manager::PasswordStore;
using password_manager::PasswordStoreChange;
using password_manager::PasswordStoreChangeList;
using testing::_;
using testing::ElementsAre;
using testing::IsEmpty;
using testing::Pointee;

class MockPasswordStoreConsumer
    : public password_manager::PasswordStoreConsumer {
 public:
  MockPasswordStoreConsumer() = default;

  const std::vector<std::unique_ptr<PasswordForm>>& forms() const {
    return forms_;
  }

 private:
  void OnGetPasswordStoreResults(
      std::vector<std::unique_ptr<PasswordForm>> results) override {
    forms_.swap(results);
  }

  std::vector<std::unique_ptr<PasswordForm>> forms_;

  DISALLOW_COPY_AND_ASSIGN(MockPasswordStoreConsumer);
};

class MockPasswordStoreObserver
    : public password_manager::PasswordStore::Observer {
 public:
  explicit MockPasswordStoreObserver(PasswordStoreMac* password_store)
      : guard_(this) {
    guard_.Add(password_store);
  }
  MOCK_METHOD1(OnLoginsChanged,
               void(const password_manager::PasswordStoreChangeList& changes));

 private:
  ScopedObserver<PasswordStoreMac, MockPasswordStoreObserver> guard_;

  DISALLOW_COPY_AND_ASSIGN(MockPasswordStoreObserver);
};

// A mock LoginDatabase that simulates a failing Init() method.
class BadLoginDatabase : public password_manager::LoginDatabase {
 public:
  BadLoginDatabase() : password_manager::LoginDatabase(base::FilePath()) {}
  ~BadLoginDatabase() override {}

  // LoginDatabase:
  bool Init() override { return false; }

 private:
  DISALLOW_COPY_AND_ASSIGN(BadLoginDatabase);
};

class PasswordStoreMacTest : public testing::TestWithParam<MigrationStatus> {
 public:
  PasswordStoreMacTest();
  ~PasswordStoreMacTest() override;

  void CreateAndInitPasswordStore(
      std::unique_ptr<password_manager::LoginDatabase> login_db);

  void ClosePasswordStore();

  // Wait for all the previously enqueued operations to finish.
  void FinishAsyncProcessing();

  // Add/Update/Remove |form| and verify the operation succeeded.
  void AddForm(const PasswordForm& form);
  void UpdateForm(const PasswordForm& form);
  void RemoveForm(const PasswordForm& form);

  base::FilePath test_login_db_file_path() const;

  // Returns the expected migration status after the password store was inited.
  MigrationStatus GetTargetStatus() const;

  password_manager::LoginDatabase* login_db() const {
    return store_->login_metadata_db();
  }

  PasswordStoreMac* store() { return store_.get(); }

 protected:
  base::test::ScopedTaskEnvironment scoped_task_environment_;
  base::ScopedTempDir db_dir_;
  scoped_refptr<PasswordStoreMac> store_;
  sync_preferences::TestingPrefServiceSyncable testing_prefs_;
};

PasswordStoreMacTest::PasswordStoreMacTest() {
  EXPECT_TRUE(db_dir_.CreateUniqueTempDir());
  RegisterUserProfilePrefs(testing_prefs_.registry());
  testing_prefs_.SetInteger(password_manager::prefs::kKeychainMigrationStatus,
                            static_cast<int>(GetParam()));
  // Ensure that LoginDatabase will use the mock keychain if it needs to
  // encrypt/decrypt a password.
  OSCryptMocker::SetUp();
}

PasswordStoreMacTest::~PasswordStoreMacTest() {
  ClosePasswordStore();
  OSCryptMocker::TearDown();
}

void PasswordStoreMacTest::CreateAndInitPasswordStore(
    std::unique_ptr<password_manager::LoginDatabase> login_db) {
  store_ = new PasswordStoreMac(std::move(login_db), &testing_prefs_);
  ASSERT_TRUE(store_->Init(syncer::SyncableService::StartSyncFlare(), nullptr));
}

void PasswordStoreMacTest::ClosePasswordStore() {
  if (!store_)
    return;
  store_->ShutdownOnUIThread();
  store_ = nullptr;
}

void PasswordStoreMacTest::FinishAsyncProcessing() {
  scoped_task_environment_.RunUntilIdle();
}

base::FilePath PasswordStoreMacTest::test_login_db_file_path() const {
  return db_dir_.GetPath().Append(FILE_PATH_LITERAL("login.db"));
}

MigrationStatus PasswordStoreMacTest::GetTargetStatus() const {
  if (GetParam() == MigrationStatus::NOT_STARTED ||
      GetParam() == MigrationStatus::FAILED_ONCE ||
      GetParam() == MigrationStatus::FAILED_TWICE) {
    return MigrationStatus::MIGRATION_STOPPED;
  }
  return GetParam();
}

void PasswordStoreMacTest::AddForm(const PasswordForm& form) {
  MockPasswordStoreObserver mock_observer(store());

  password_manager::PasswordStoreChangeList list;
  list.push_back(password_manager::PasswordStoreChange(
      password_manager::PasswordStoreChange::ADD, form));
  EXPECT_CALL(mock_observer, OnLoginsChanged(list));
  store()->AddLogin(form);
  FinishAsyncProcessing();
}

void PasswordStoreMacTest::UpdateForm(const PasswordForm& form) {
  MockPasswordStoreObserver mock_observer(store());

  password_manager::PasswordStoreChangeList list;
  list.push_back(password_manager::PasswordStoreChange(
      password_manager::PasswordStoreChange::UPDATE, form));
  EXPECT_CALL(mock_observer, OnLoginsChanged(list));
  store()->UpdateLogin(form);
  FinishAsyncProcessing();
}

void PasswordStoreMacTest::RemoveForm(const PasswordForm& form) {
  MockPasswordStoreObserver mock_observer(store());

  password_manager::PasswordStoreChangeList list;
  list.push_back(password_manager::PasswordStoreChange(
      password_manager::PasswordStoreChange::REMOVE, form));
  EXPECT_CALL(mock_observer, OnLoginsChanged(list));
  store()->RemoveLogin(form);
  FinishAsyncProcessing();
}

// ----------- Tests -------------

TEST_P(PasswordStoreMacTest, Sanity) {
  base::HistogramTester histogram_tester;

  CreateAndInitPasswordStore(std::make_unique<password_manager::LoginDatabase>(
      test_login_db_file_path()));
  FinishAsyncProcessing();
  ClosePasswordStore();

  int status = testing_prefs_.GetInteger(
      password_manager::prefs::kKeychainMigrationStatus);
  EXPECT_EQ(static_cast<int>(GetTargetStatus()), status);
  histogram_tester.ExpectUniqueSample(
      "PasswordManager.KeychainMigration.Status", status, 1);
}

TEST_P(PasswordStoreMacTest, StartAndStop) {
  base::HistogramTester histogram_tester;
  // PasswordStore::ShutdownOnUIThread() immediately follows
  // PasswordStore::Init(). The message loop isn't running in between. Anyway,
  // PasswordStore should not collapse.
  CreateAndInitPasswordStore(std::make_unique<password_manager::LoginDatabase>(
      test_login_db_file_path()));
  ClosePasswordStore();

  FinishAsyncProcessing();

  histogram_tester.ExpectUniqueSample(
      "PasswordManager.KeychainMigration.Status",
      static_cast<int>(GetTargetStatus()), 1);
}

TEST_P(PasswordStoreMacTest, OperationsOnABadDatabaseSilentlyFail) {
  // Verify that operations on a PasswordStore with a bad database cause no
  // explosions, but fail without side effect, return no data and trigger no
  // notifications.
  CreateAndInitPasswordStore(std::make_unique<BadLoginDatabase>());
  FinishAsyncProcessing();
  EXPECT_FALSE(login_db());

  // The store should outlive the observer.
  scoped_refptr<PasswordStoreMac> store_refptr = store();
  MockPasswordStoreObserver mock_observer(store());
  EXPECT_CALL(mock_observer, OnLoginsChanged(_)).Times(0);

  // Add a new autofillable login + a blacklisted login.
  password_manager::PasswordFormData www_form_data = {
      PasswordForm::SCHEME_HTML,
      "http://www.facebook.com/",
      "http://www.facebook.com/index.html",
      "login",
      L"username",
      L"password",
      L"submit",
      L"not_joe_user",
      L"12345",
      true,
      1};
  std::unique_ptr<PasswordForm> form = FillPasswordFormWithData(www_form_data);
  std::unique_ptr<PasswordForm> blacklisted_form(new PasswordForm(*form));
  blacklisted_form->signon_realm = "http://foo.example.com";
  blacklisted_form->origin = GURL("http://foo.example.com/origin");
  blacklisted_form->action = GURL("http://foo.example.com/action");
  blacklisted_form->blacklisted_by_user = true;
  store()->AddLogin(*form);
  store()->AddLogin(*blacklisted_form);
  FinishAsyncProcessing();

  // Get all logins; autofillable logins; blacklisted logins.
  MockPasswordStoreConsumer mock_consumer;
  store()->GetLogins(PasswordStore::FormDigest(*form), &mock_consumer);
  FinishAsyncProcessing();
  EXPECT_THAT(mock_consumer.forms(), IsEmpty());

  store()->GetAutofillableLogins(&mock_consumer);
  FinishAsyncProcessing();
  EXPECT_THAT(mock_consumer.forms(), IsEmpty());

  store()->GetBlacklistLogins(&mock_consumer);
  FinishAsyncProcessing();
  EXPECT_THAT(mock_consumer.forms(), IsEmpty());

  // Report metrics.
  store()->ReportMetrics("Test Username", true, false);
  FinishAsyncProcessing();

  // Change the login.
  form->password_value = base::ASCIIToUTF16("a different password");
  store()->UpdateLogin(*form);
  FinishAsyncProcessing();

  // Delete one login; a range of logins.
  store()->RemoveLogin(*form);
  store()->RemoveLoginsCreatedBetween(base::Time(), base::Time::Max(),
                                      base::Closure());
  store()->RemoveLoginsSyncedBetween(base::Time(), base::Time::Max());
  FinishAsyncProcessing();

  // Verify no notifications are fired during shutdown either.
  ClosePasswordStore();
}

INSTANTIATE_TEST_CASE_P(,
                        PasswordStoreMacTest,
                        testing::Values(MigrationStatus::NOT_STARTED,
                                        MigrationStatus::MIGRATED,
                                        MigrationStatus::FAILED_ONCE,
                                        MigrationStatus::FAILED_TWICE,
                                        MigrationStatus::MIGRATED_DELETED,
                                        MigrationStatus::MIGRATED_PARTIALLY,
                                        MigrationStatus::MIGRATION_STOPPED));

}  // namespace
