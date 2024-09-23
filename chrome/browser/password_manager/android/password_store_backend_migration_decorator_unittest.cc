// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/android/password_store_backend_migration_decorator.h"

#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "base/test/test_mock_time_task_runner.h"
#include "base/time/time.h"
#include "components/password_manager/core/browser/features/password_features.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"
#include "components/password_manager/core/browser/password_manager_test_utils.h"
#include "components/password_manager/core/browser/password_store/mock_password_store_backend.h"
#include "components/password_manager/core/browser/password_store/password_store.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "components/signin/public/base/signin_pref_names.h"
#include "components/sync/test/test_sync_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace password_manager {
namespace {

using ::testing::_;
using ::testing::ElementsAreArray;
using ::testing::Mock;
using ::testing::NiceMock;
using ::testing::Ref;
using ::testing::Return;
using ::testing::VariantWith;
using ::testing::WithArg;
using ::testing::WithArgs;

PasswordForm CreateTestForm() {
  PasswordForm form;
  form.username_value = u"admin";
  form.password_value = u"admin";
  form.signon_realm = "https://admin.google.com/";
  form.url = GURL(form.signon_realm);
  return form;
}

}  // namespace

class PasswordStoreBackendMigrationDecoratorTest : public testing::Test {
 protected:
  void SetUp() override {
    prefs_.registry()->RegisterDoublePref(prefs::kTimeOfLastMigrationAttempt,
                                          0);
    prefs_.registry()->RegisterBooleanPref(
        prefs::kUnenrolledFromGoogleMobileServicesDueToErrors, false);
    prefs_.registry()->RegisterIntegerPref(
        prefs::kCurrentMigrationVersionToGoogleMobileServices, 0);
    prefs_.registry()->RegisterIntegerPref(
        prefs::kPasswordsUseUPMLocalAndSeparateStores,
        static_cast<int>(
            password_manager::prefs::UseUpmLocalAndSeparateStoresState::
                kOffAndMigrationPending));

    auto built_in_backend =
        std::make_unique<NiceMock<MockPasswordStoreBackend>>();
    auto android_backend =
        std::make_unique<NiceMock<MockPasswordStoreBackend>>();
    built_in_backend_ = built_in_backend.get();
    android_backend_ = android_backend.get();
    backend_migration_decorator_ =
        std::make_unique<PasswordStoreBackendMigrationDecorator>(
            std::move(built_in_backend), std::move(android_backend), &prefs_);
    sync_service_.GetUserSettings()->SetSelectedTypes(
        /*sync_everything=*/false, /*types=*/{});
    backend_migration_decorator()->OnSyncServiceInitialized(&sync_service_);
  }

  void TearDown() override {
    EXPECT_CALL(android_backend(), Shutdown);
    EXPECT_CALL(built_in_backend(), Shutdown);
    built_in_backend_ = nullptr;
    android_backend_ = nullptr;
    backend_migration_decorator()->Shutdown(base::DoNothing());
  }

  PasswordStoreBackend* backend_migration_decorator() {
    return backend_migration_decorator_.get();
  }
  MockPasswordStoreBackend& built_in_backend() { return *built_in_backend_; }
  MockPasswordStoreBackend& android_backend() { return *android_backend_; }

  TestingPrefServiceSimple& prefs() { return prefs_; }

  void RunUntilIdle() { task_env_.RunUntilIdle(); }

  void FastForwardUntilNoTasksRemain() {
    task_env_.FastForwardUntilNoTasksRemain();
  }

  void FastForwardBy(base::TimeDelta delta) { task_env_.FastForwardBy(delta); }

  int GetPendingMainThreadTaskCount() {
    return task_env_.GetPendingMainThreadTaskCount();
  }

 private:
  base::test::SingleThreadTaskEnvironment task_env_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  raw_ptr<NiceMock<MockPasswordStoreBackend>> built_in_backend_;
  raw_ptr<NiceMock<MockPasswordStoreBackend>> android_backend_;
  TestingPrefServiceSimple prefs_;
  syncer::TestSyncService sync_service_;
  std::unique_ptr<PasswordStoreBackendMigrationDecorator>
      backend_migration_decorator_;
};

TEST_F(PasswordStoreBackendMigrationDecoratorTest,
       RecordsSchedulingOfLocalPwdMigration) {
  base::HistogramTester histogram_tester;
  prefs().SetInteger(
      prefs::kPasswordsUseUPMLocalAndSeparateStores,
      static_cast<int>(
          prefs::UseUpmLocalAndSeparateStoresState::kOffAndMigrationPending));
  backend_migration_decorator()->InitBackend(
      /*affiliated_match_helper=*/nullptr,
      /*remote_form_changes_received=*/base::DoNothing(),
      /*sync_enabled_or_disabled_cb=*/base::DoNothing(),
      /*completion=*/base::DoNothing());
  // Migration should be scheduled.
  ASSERT_EQ(1, GetPendingMainThreadTaskCount());
  histogram_tester.ExpectUniqueSample(
      "PasswordManager.UnifiedPasswordManager.MigrationForLocalUsers."
      "ProgressState",
      metrics_util::LocalPwdMigrationProgressState::kScheduled, 1);
}

TEST_F(PasswordStoreBackendMigrationDecoratorTest, GetAllLoginsAsync) {
  EXPECT_CALL(built_in_backend(), GetAllLoginsAsync);
  EXPECT_CALL(android_backend(), GetAllLoginsAsync).Times(0);

  backend_migration_decorator()->GetAllLoginsAsync(base::DoNothing());
}

TEST_F(PasswordStoreBackendMigrationDecoratorTest,
       GetAllLoginsAsyncAfterMigration) {
  // Indicate completeness of local password migration.
  prefs().SetInteger(
      prefs::kPasswordsUseUPMLocalAndSeparateStores,
      static_cast<int>(prefs::UseUpmLocalAndSeparateStoresState::kOn));

  EXPECT_CALL(built_in_backend(), GetAllLoginsAsync).Times(0);
  EXPECT_CALL(android_backend(), GetAllLoginsAsync);

  backend_migration_decorator()->GetAllLoginsAsync(base::DoNothing());
}

TEST_F(PasswordStoreBackendMigrationDecoratorTest, IsAbleToSavePasswords) {
  EXPECT_CALL(built_in_backend(), IsAbleToSavePasswords).WillOnce(Return(true));
  EXPECT_CALL(android_backend(), IsAbleToSavePasswords).Times(0);

  EXPECT_TRUE(backend_migration_decorator()->IsAbleToSavePasswords());
}

TEST_F(PasswordStoreBackendMigrationDecoratorTest,
       IsAbleToSavePasswordsAfterMigration) {
  prefs().SetInteger(
      prefs::kPasswordsUseUPMLocalAndSeparateStores,
      static_cast<int>(prefs::UseUpmLocalAndSeparateStoresState::kOn));

  EXPECT_CALL(built_in_backend(), IsAbleToSavePasswords).Times(0);
  EXPECT_CALL(android_backend(), IsAbleToSavePasswords).WillOnce(Return(false));

  EXPECT_FALSE(backend_migration_decorator()->IsAbleToSavePasswords());
}

TEST_F(PasswordStoreBackendMigrationDecoratorTest,
       GetAllLoginsWithAffiliationAndBrandingAsync) {
  EXPECT_CALL(built_in_backend(), GetAllLoginsWithAffiliationAndBrandingAsync);
  EXPECT_CALL(android_backend(), GetAllLoginsWithAffiliationAndBrandingAsync)
      .Times(0);

  backend_migration_decorator()->GetAllLoginsWithAffiliationAndBrandingAsync(
      base::DoNothing());
}

TEST_F(PasswordStoreBackendMigrationDecoratorTest,
       GetAllLoginsWithAffiliationAndBrandingAsyncAfterMigration) {
  prefs().SetInteger(
      prefs::kPasswordsUseUPMLocalAndSeparateStores,
      static_cast<int>(prefs::UseUpmLocalAndSeparateStoresState::kOn));

  EXPECT_CALL(built_in_backend(), GetAllLoginsWithAffiliationAndBrandingAsync)
      .Times(0);
  EXPECT_CALL(android_backend(), GetAllLoginsWithAffiliationAndBrandingAsync);

  backend_migration_decorator()->GetAllLoginsWithAffiliationAndBrandingAsync(
      base::DoNothing());
}

TEST_F(PasswordStoreBackendMigrationDecoratorTest, GetAutofillableLoginsAsync) {
  EXPECT_CALL(built_in_backend(), GetAutofillableLoginsAsync);
  EXPECT_CALL(android_backend(), GetAutofillableLoginsAsync).Times(0);

  backend_migration_decorator()->GetAutofillableLoginsAsync(base::DoNothing());
}

TEST_F(PasswordStoreBackendMigrationDecoratorTest,
       GetAutofillableLoginsAsyncAfterMigration) {
  // Indicate completeness of local password migration.
  prefs().SetInteger(
      prefs::kPasswordsUseUPMLocalAndSeparateStores,
      static_cast<int>(prefs::UseUpmLocalAndSeparateStoresState::kOn));

  EXPECT_CALL(built_in_backend(), GetAutofillableLoginsAsync).Times(0);
  EXPECT_CALL(android_backend(), GetAutofillableLoginsAsync);

  backend_migration_decorator()->GetAutofillableLoginsAsync(base::DoNothing());
}

TEST_F(PasswordStoreBackendMigrationDecoratorTest, FillMatchingLoginsAsync) {
  std::vector<PasswordFormDigest> forms = {
      PasswordFormDigest(PasswordForm::Scheme::kHtml, "https://google.com/",
                         GURL("https://google.com/"))};

  EXPECT_CALL(built_in_backend(),
              FillMatchingLoginsAsync(_, false, ElementsAreArray(forms)));
  EXPECT_CALL(android_backend(), FillMatchingLoginsAsync).Times(0);

  backend_migration_decorator()->FillMatchingLoginsAsync(
      base::DoNothing(), /*include_psl=*/false, forms);
}

TEST_F(PasswordStoreBackendMigrationDecoratorTest,
       FillMatchingLoginsAsyncAfterMigration) {
  // Indicate completeness of local password migration.
  prefs().SetInteger(
      prefs::kPasswordsUseUPMLocalAndSeparateStores,
      static_cast<int>(prefs::UseUpmLocalAndSeparateStoresState::kOn));

  std::vector<PasswordFormDigest> forms = {
      PasswordFormDigest(PasswordForm::Scheme::kHtml, "https://google.com/",
                         GURL("https://google.com/"))};
  EXPECT_CALL(built_in_backend(), FillMatchingLoginsAsync).Times(0);
  EXPECT_CALL(android_backend(),
              FillMatchingLoginsAsync(_, false, ElementsAreArray(forms)));

  backend_migration_decorator()->FillMatchingLoginsAsync(
      base::DoNothing(), /*include_psl=*/false, forms);
}

TEST_F(PasswordStoreBackendMigrationDecoratorTest,
       GetGroupedMatchingLoginsAsync) {
  PasswordFormDigest form_digest(PasswordForm::Scheme::kHtml,
                                 "https://google.com/",
                                 GURL("https://google.com/"));
  EXPECT_CALL(built_in_backend(),
              GetGroupedMatchingLoginsAsync(Ref(form_digest), _));
  EXPECT_CALL(android_backend(), GetGroupedMatchingLoginsAsync).Times(0);

  backend_migration_decorator()->GetGroupedMatchingLoginsAsync(
      form_digest, base::DoNothing());
}

TEST_F(PasswordStoreBackendMigrationDecoratorTest,
       GetGroupedMatchingLoginsAsyncAfterMigration) {
  // Indicate completeness of local password migration.
  prefs().SetInteger(
      prefs::kPasswordsUseUPMLocalAndSeparateStores,
      static_cast<int>(prefs::UseUpmLocalAndSeparateStoresState::kOn));

  PasswordFormDigest form_digest(PasswordForm::Scheme::kHtml,
                                 "https://google.com/",
                                 GURL("https://google.com/"));
  EXPECT_CALL(built_in_backend(), GetGroupedMatchingLoginsAsync).Times(0);
  EXPECT_CALL(android_backend(),
              GetGroupedMatchingLoginsAsync(Ref(form_digest), _));

  backend_migration_decorator()->GetGroupedMatchingLoginsAsync(
      form_digest, base::DoNothing());
}

TEST_F(PasswordStoreBackendMigrationDecoratorTest, AddLoginAsync) {
  EXPECT_CALL(built_in_backend(), AddLoginAsync(CreateTestForm(), _));
  EXPECT_CALL(android_backend(), AddLoginAsync).Times(0);

  backend_migration_decorator()->AddLoginAsync(CreateTestForm(),
                                               base::DoNothing());
}

TEST_F(PasswordStoreBackendMigrationDecoratorTest,
       AddLoginAsyncAfterMigration) {
  // Indicate completeness of local password migration.
  prefs().SetInteger(
      prefs::kPasswordsUseUPMLocalAndSeparateStores,
      static_cast<int>(prefs::UseUpmLocalAndSeparateStoresState::kOn));

  EXPECT_CALL(built_in_backend(), AddLoginAsync).Times(0);
  EXPECT_CALL(android_backend(), AddLoginAsync(CreateTestForm(), _));

  backend_migration_decorator()->AddLoginAsync(CreateTestForm(),
                                               base::DoNothing());
}

TEST_F(PasswordStoreBackendMigrationDecoratorTest, UpdateLoginAsync) {
  EXPECT_CALL(built_in_backend(), UpdateLoginAsync(CreateTestForm(), _));
  EXPECT_CALL(android_backend(), UpdateLoginAsync).Times(0);

  backend_migration_decorator()->UpdateLoginAsync(CreateTestForm(),
                                                  base::DoNothing());
}

TEST_F(PasswordStoreBackendMigrationDecoratorTest,
       UpdateLoginAsyncAfterMigration) {
  // Indicate completeness of local password migration.
  prefs().SetInteger(
      prefs::kPasswordsUseUPMLocalAndSeparateStores,
      static_cast<int>(prefs::UseUpmLocalAndSeparateStoresState::kOn));

  EXPECT_CALL(built_in_backend(), UpdateLoginAsync).Times(0);
  EXPECT_CALL(android_backend(), UpdateLoginAsync(CreateTestForm(), _));

  backend_migration_decorator()->UpdateLoginAsync(CreateTestForm(),
                                                  base::DoNothing());
}

TEST_F(PasswordStoreBackendMigrationDecoratorTest, RemoveLoginAsync) {
  EXPECT_CALL(built_in_backend(), RemoveLoginAsync(_, CreateTestForm(), _));
  EXPECT_CALL(android_backend(), RemoveLoginAsync).Times(0);

  backend_migration_decorator()->RemoveLoginAsync(FROM_HERE, CreateTestForm(),
                                                  base::DoNothing());
}

TEST_F(PasswordStoreBackendMigrationDecoratorTest,
       RemoveLoginAsyncAfterMigration) {
  // Indicate completeness of local password migration.
  prefs().SetInteger(
      prefs::kPasswordsUseUPMLocalAndSeparateStores,
      static_cast<int>(prefs::UseUpmLocalAndSeparateStoresState::kOn));

  EXPECT_CALL(built_in_backend(), RemoveLoginAsync(_, CreateTestForm(), _));
  EXPECT_CALL(android_backend(), RemoveLoginAsync(_, CreateTestForm(), _));

  backend_migration_decorator()->RemoveLoginAsync(FROM_HERE, CreateTestForm(),
                                                  base::DoNothing());
}

TEST_F(PasswordStoreBackendMigrationDecoratorTest,
       RemoveLoginsByURLAndTimeAsync) {
  base::RepeatingCallback<bool(const GURL&)> url_filter =
      base::BindRepeating([](const GURL& url) { return true; });
  base::Time delete_begin = base::Time::FromTimeT(1000);
  base::Time delete_end = base::Time::FromTimeT(2000);

  EXPECT_CALL(built_in_backend(),
              RemoveLoginsByURLAndTimeAsync(_, url_filter, delete_begin,
                                            delete_end, _, _));
  EXPECT_CALL(android_backend(), RemoveLoginsByURLAndTimeAsync).Times(0);

  backend_migration_decorator()->RemoveLoginsByURLAndTimeAsync(
      FROM_HERE, url_filter, delete_begin, delete_end,
      base::OnceCallback<void(bool)>(), base::DoNothing());
}

TEST_F(PasswordStoreBackendMigrationDecoratorTest,
       RemoveLoginsByURLAndTimeAsyncAfterMigration) {
  // Indicate completeness of local password migration.
  prefs().SetInteger(
      prefs::kPasswordsUseUPMLocalAndSeparateStores,
      static_cast<int>(prefs::UseUpmLocalAndSeparateStoresState::kOn));

  base::RepeatingCallback<bool(const GURL&)> url_filter =
      base::BindRepeating([](const GURL& url) { return true; });
  base::Time delete_begin = base::Time::FromTimeT(1000);
  base::Time delete_end = base::Time::FromTimeT(2000);
  EXPECT_CALL(built_in_backend(),
              RemoveLoginsByURLAndTimeAsync(_, url_filter, delete_begin,
                                            delete_end, _, _));
  EXPECT_CALL(android_backend(),
              RemoveLoginsByURLAndTimeAsync(_, url_filter, delete_begin,
                                            delete_end, _, _));

  backend_migration_decorator()->RemoveLoginsByURLAndTimeAsync(
      FROM_HERE, url_filter, delete_begin, delete_end,
      base::OnceCallback<void(bool)>(), base::DoNothing());
}

TEST_F(PasswordStoreBackendMigrationDecoratorTest,
       RemoveLoginsCreatedBetweenAsync) {
  base::Time delete_begin = base::Time::FromTimeT(1000);
  base::Time delete_end = base::Time::FromTimeT(2000);

  EXPECT_CALL(built_in_backend(),
              RemoveLoginsCreatedBetweenAsync(_, delete_begin, delete_end, _));
  EXPECT_CALL(android_backend(), RemoveLoginsCreatedBetweenAsync).Times(0);

  backend_migration_decorator()->RemoveLoginsCreatedBetweenAsync(
      FROM_HERE, delete_begin, delete_end, base::DoNothing());
}

TEST_F(PasswordStoreBackendMigrationDecoratorTest,
       RemoveLoginsCreatedBetweenAsyncAfterMigration) {
  // Indicate completeness of local password migration.
  prefs().SetInteger(
      prefs::kPasswordsUseUPMLocalAndSeparateStores,
      static_cast<int>(prefs::UseUpmLocalAndSeparateStoresState::kOn));

  base::Time delete_begin = base::Time::FromTimeT(1000);
  base::Time delete_end = base::Time::FromTimeT(2000);
  EXPECT_CALL(built_in_backend(),
              RemoveLoginsCreatedBetweenAsync(_, delete_begin, delete_end, _));
  EXPECT_CALL(android_backend(),
              RemoveLoginsCreatedBetweenAsync(_, delete_begin, delete_end, _));

  backend_migration_decorator()->RemoveLoginsCreatedBetweenAsync(
      FROM_HERE, delete_begin, delete_end, base::DoNothing());
}

TEST_F(PasswordStoreBackendMigrationDecoratorTest,
       DisableAutoSignInForOriginsAsync) {
  base::RepeatingCallback<bool(const GURL&)> url_filter =
      base::BindRepeating([](const GURL& url) { return true; });

  EXPECT_CALL(built_in_backend(),
              DisableAutoSignInForOriginsAsync(url_filter, _));
  EXPECT_CALL(android_backend(), DisableAutoSignInForOriginsAsync).Times(0);

  backend_migration_decorator()->DisableAutoSignInForOriginsAsync(
      url_filter, base::DoNothing());
}

TEST_F(PasswordStoreBackendMigrationDecoratorTest,
       DisableAutoSignInForOriginsAsyncAfterMigration) {
  // Indicate completeness of local password migration.
  prefs().SetInteger(
      prefs::kPasswordsUseUPMLocalAndSeparateStores,
      static_cast<int>(prefs::UseUpmLocalAndSeparateStoresState::kOn));

  base::RepeatingCallback<bool(const GURL&)> url_filter =
      base::BindRepeating([](const GURL& url) { return true; });
  EXPECT_CALL(built_in_backend(), DisableAutoSignInForOriginsAsync).Times(0);
  EXPECT_CALL(android_backend(),
              DisableAutoSignInForOriginsAsync(url_filter, _));

  backend_migration_decorator()->DisableAutoSignInForOriginsAsync(
      url_filter, base::DoNothing());
}

TEST_F(PasswordStoreBackendMigrationDecoratorTest,
       DisableSavingDuringLocalPasswordsMigration) {
  backend_migration_decorator()->InitBackend(
      /*affiliated_match_helper=*/nullptr,
      /*remote_form_changes_received=*/base::DoNothing(),
      /*sync_enabled_or_disabled_cb=*/base::DoNothing(),
      /*completion=*/base::DoNothing());

  // True so far because the migration is deferred.
  EXPECT_CALL(built_in_backend(), IsAbleToSavePasswords)
      .WillRepeatedly(Return(true));
  ASSERT_TRUE(backend_migration_decorator()->IsAbleToSavePasswords());

  FastForwardUntilNoTasksRemain();

  // False now because the migration is ongoing.
  EXPECT_FALSE(backend_migration_decorator()->IsAbleToSavePasswords());
}

TEST_F(PasswordStoreBackendMigrationDecoratorTest, RemoteFormChangesReceived) {
  base::MockCallback<PasswordStoreBackend::RemoteChangesReceived>
      original_callback;

  // Both backends receive a callback that they trigger for new remote changes.
  PasswordStoreBackend::RemoteChangesReceived built_in_remote_changes_callback;
  EXPECT_CALL(built_in_backend(), InitBackend)
      .WillOnce(testing::SaveArg<1>(&built_in_remote_changes_callback));
  PasswordStoreBackend::RemoteChangesReceived android_remote_changes_callback;
  EXPECT_CALL(android_backend(), InitBackend)
      .WillOnce(testing::SaveArg<1>(&android_remote_changes_callback));
  backend_migration_decorator()->InitBackend(
      nullptr, original_callback.Get(), base::DoNothing(), base::DoNothing());

  EXPECT_CALL(original_callback, Run).Times(0);
  android_remote_changes_callback.Run(std::nullopt);
  testing::Mock::VerifyAndClearExpectations(&original_callback);

  EXPECT_CALL(original_callback, Run);
  built_in_remote_changes_callback.Run(std::nullopt);
  testing::Mock::VerifyAndClearExpectations(&original_callback);
}

TEST_F(PasswordStoreBackendMigrationDecoratorTest,
       RemoteFormChangesReceivedAfterMigration) {
  // Indicate completeness of local password migration.
  prefs().SetInteger(
      prefs::kPasswordsUseUPMLocalAndSeparateStores,
      static_cast<int>(prefs::UseUpmLocalAndSeparateStoresState::kOn));
  base::MockCallback<PasswordStoreBackend::RemoteChangesReceived>
      original_callback;

  PasswordStoreBackend::RemoteChangesReceived built_in_remote_changes_callback;
  EXPECT_CALL(built_in_backend(), InitBackend)
      .WillOnce(testing::SaveArg<1>(&built_in_remote_changes_callback));
  PasswordStoreBackend::RemoteChangesReceived android_remote_changes_callback;
  EXPECT_CALL(android_backend(), InitBackend)
      .WillOnce(testing::SaveArg<1>(&android_remote_changes_callback));
  backend_migration_decorator()->InitBackend(
      nullptr, original_callback.Get(), base::DoNothing(), base::DoNothing());

  EXPECT_CALL(original_callback, Run);
  android_remote_changes_callback.Run(std::nullopt);
  testing::Mock::VerifyAndClearExpectations(&original_callback);

  EXPECT_CALL(original_callback, Run).Times(0);
  built_in_remote_changes_callback.Run(std::nullopt);
}

TEST_F(PasswordStoreBackendMigrationDecoratorTest,
       CallCompletionCallbackAfterInit) {
  base::MockCallback<base::OnceCallback<void(bool)>> completion_callback;

  // Both backends need to be invoked for a successful completion call.
  EXPECT_CALL(built_in_backend(), InitBackend)
      .WillOnce(WithArg<3>(
          testing::Invoke([](base::OnceCallback<void(bool)> reply) -> void {
            std::move(reply).Run(true);
          })));

  base::OnceCallback<void(bool)> captured_android_backend_reply;
  EXPECT_CALL(android_backend(), InitBackend)
      .WillOnce(WithArg<3>(
          testing::Invoke([&](base::OnceCallback<void(bool)> reply) -> void {
            captured_android_backend_reply = std::move(reply);
          })));

  backend_migration_decorator()->InitBackend(
      nullptr, base::DoNothing(), base::DoNothing(), completion_callback.Get());

  EXPECT_CALL(completion_callback, Run(true));
  std::move(captured_android_backend_reply).Run(true);
}

TEST_F(PasswordStoreBackendMigrationDecoratorTest,
       MigrationIsStartedWithDelayAfterInit) {
  prefs().SetInteger(
      prefs::kPasswordsUseUPMLocalAndSeparateStores,
      static_cast<int>(
          prefs::UseUpmLocalAndSeparateStoresState::kOffAndMigrationPending));

  backend_migration_decorator()->InitBackend(
      nullptr, /* remote_form_changes_received= */ base::DoNothing(),
      /* sync_enabled_or_disabled_cb= */ base::DoNothing(),
      /* completion= */ base::DoNothing());
  // Migration should be scheduled.
  EXPECT_EQ(1, GetPendingMainThreadTaskCount());

  FastForwardBy(kLocalPasswordsMigrationToAndroidBackendDelay);
  // Migration should be started by now.
  EXPECT_EQ(0, GetPendingMainThreadTaskCount());
}

}  // namespace password_manager
