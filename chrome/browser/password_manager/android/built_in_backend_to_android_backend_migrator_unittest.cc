// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/android/built_in_backend_to_android_backend_migrator.h"

#include "base/feature_list.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "components/password_manager/core/browser/password_manager_test_utils.h"
#include "components/password_manager/core/browser/password_store/fake_password_store_backend.h"
#include "components/password_manager/core/browser/password_store/mock_password_store_backend.h"
#include "components/password_manager/core/browser/password_store/password_store_backend.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/pref_registry.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/testing_pref_service.h"
#include "components/signin/public/base/signin_pref_names.h"
#include "components/sync/test/test_sync_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::ElementsAre;
using ::testing::ElementsAreArray;
using ::testing::Eq;
using ::testing::Invoke;
using ::testing::IsEmpty;
using ::testing::Pointee;
using ::testing::Return;
using ::testing::UnorderedElementsAreArray;
using ::testing::VariantWith;
using ::testing::WithArg;

namespace password_manager {

namespace {

constexpr base::TimeDelta kLatencyDelta = base::Milliseconds(123u);
const PasswordStoreBackendError kBackendError = PasswordStoreBackendError(
    PasswordStoreBackendErrorType::kUncategorized,
    PasswordStoreBackendErrorRecoveryType::kUnrecoverable);

PasswordForm CreateTestPasswordForm(int index = 0) {
  PasswordForm form;
  form.url = GURL("https://test" + base::NumberToString(index) + ".com");
  form.signon_realm = form.url.spec();
  form.username_value = u"username" + base::NumberToString16(index);
  form.password_value = u"password" + base::NumberToString16(index);
  form.in_store = PasswordForm::Store::kProfileStore;
  return form;
}

}  // namespace

// Checks that initial/rolling migration is started only when all the conditions
// are satisfied. It also check that migration result is properly recorded in
// prefs.
class BuiltInBackendToAndroidBackendMigratorTest : public testing::Test {
 protected:
  BuiltInBackendToAndroidBackendMigratorTest() = default;
  ~BuiltInBackendToAndroidBackendMigratorTest() override = default;

  void Init(int current_migration_version = 0) {
    prefs_.registry()->RegisterIntegerPref(
        prefs::kCurrentMigrationVersionToGoogleMobileServices, 0);
    prefs_.SetInteger(prefs::kCurrentMigrationVersionToGoogleMobileServices,
                      current_migration_version);
    prefs_.registry()->RegisterDoublePref(prefs::kTimeOfLastMigrationAttempt,
                                          0.0);
    prefs_.registry()->RegisterBooleanPref(
        prefs::kRequiresMigrationAfterSyncStatusChange, false);
    prefs_.registry()->RegisterStringPref(
        ::prefs::kGoogleServicesLastSyncingUsername, "testaccount@gmail.com");
    prefs_.registry()->RegisterBooleanPref(
        prefs::kUnenrolledFromGoogleMobileServicesDueToErrors, false);
    prefs_.registry()->RegisterIntegerPref(
        prefs::kUnenrolledFromGoogleMobileServicesAfterApiErrorCode, 0);
    prefs_.registry()->RegisterIntegerPref(
        prefs::kTimesReenrolledToGoogleMobileServices, 0);
    prefs_.registry()->RegisterIntegerPref(
        prefs::kTimesAttemptedToReenrollToGoogleMobileServices, 0);
    CreateMigrator(&built_in_backend_, &android_backend_, &prefs_);
  }

  void InitSyncService(bool is_password_sync_enabled) {
    if (is_password_sync_enabled) {
      sync_service_.GetUserSettings()->SetSelectedTypes(
          /*sync_everything=*/false,
          /*types=*/{syncer::UserSelectableType::kPasswords});
    } else {
      sync_service_.GetUserSettings()->SetSelectedTypes(
          /*sync_everything=*/false, /*types=*/{});
    }
    migrator()->OnSyncServiceInitialized(&sync_service_);
  }

  void CreateMigrator(PasswordStoreBackend* built_in_backend,
                      PasswordStoreBackend* android_backend,
                      PrefService* prefs) {
    migrator_ = std::make_unique<BuiltInBackendToAndroidBackendMigrator>(
        built_in_backend, android_backend, prefs);
  }

  PasswordStoreBackend& built_in_backend() { return built_in_backend_; }
  PasswordStoreBackend& android_backend() { return android_backend_; }

  base::test::ScopedFeatureList& feature_list() { return feature_list_; }
  TestingPrefServiceSimple* prefs() { return &prefs_; }
  BuiltInBackendToAndroidBackendMigrator* migrator() { return migrator_.get(); }

  void RunUntilIdle() { task_env_.RunUntilIdle(); }
  void FastForwardBy(base::TimeDelta delta) { task_env_.FastForwardBy(delta); }

 private:
  base::test::SingleThreadTaskEnvironment task_env_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  base::test::ScopedFeatureList feature_list_;
  TestingPrefServiceSimple prefs_;
  syncer::TestSyncService sync_service_;
  FakePasswordStoreBackend built_in_backend_;
  FakePasswordStoreBackend android_backend_{
      IsAccountStore(false),
      FakePasswordStoreBackend::UpdateAlwaysSucceeds(true)};
  std::unique_ptr<BuiltInBackendToAndroidBackendMigrator> migrator_;
};

TEST_F(BuiltInBackendToAndroidBackendMigratorTest,
       CurrentMigrationVersionIsUpdatedWhenMigrationIsNeeded_SyncOn) {
  Init();
  InitSyncService(/*is_password_sync_enabled=*/true);

  migrator()->StartMigrationIfNecessary(
      /*should_attempt_upm_reenrollment=*/false);
  RunUntilIdle();

  EXPECT_EQ(1, prefs()->GetInteger(
                   prefs::kCurrentMigrationVersionToGoogleMobileServices));
  EXPECT_EQ(
      base::Time::Now().InSecondsFSinceUnixEpoch(),
      prefs()->GetDouble(password_manager::prefs::kTimeOfLastMigrationAttempt));
}

TEST_F(BuiltInBackendToAndroidBackendMigratorTest,
       PrefsUnchangedWhenMigrationIsNeeded_SyncOff) {
  Init();

  InitSyncService(/*is_password_sync_enabled=*/false);

  migrator()->StartMigrationIfNecessary(
      /*should_attempt_upm_reenrollment=*/false);
  RunUntilIdle();

  EXPECT_EQ(0, prefs()->GetInteger(
                   prefs::kCurrentMigrationVersionToGoogleMobileServices));
  EXPECT_EQ(0, prefs()->GetDouble(
                   password_manager::prefs::kTimeOfLastMigrationAttempt));
}

TEST_F(BuiltInBackendToAndroidBackendMigratorTest,
       AllPrefsAreUpdatedWhenMigrationIsNeeded_SyncOff) {
  feature_list().InitAndEnableFeature(
      features::kUnifiedPasswordManagerLocalPasswordsAndroidWithMigration);
  Init();

  InitSyncService(/*is_password_sync_enabled=*/false);

  migrator()->StartMigrationIfNecessary(
      /*should_attempt_upm_reenrollment=*/false);
  RunUntilIdle();

  EXPECT_EQ(1, prefs()->GetInteger(
                   prefs::kCurrentMigrationVersionToGoogleMobileServices));
  EXPECT_EQ(
      base::Time::Now().InSecondsFSinceUnixEpoch(),
      prefs()->GetDouble(password_manager::prefs::kTimeOfLastMigrationAttempt));
}

TEST_F(BuiltInBackendToAndroidBackendMigratorTest,
       PrefsUnchangedWhenAttemptedMigrationEarlierToday) {
  Init();

  prefs()->SetDouble(
      password_manager::prefs::kTimeOfLastMigrationAttempt,
      (base::Time::Now() - base::Hours(2)).InSecondsFSinceUnixEpoch());

  migrator()->StartMigrationIfNecessary(
      /*should_attempt_upm_reenrollment=*/false);
  RunUntilIdle();

  EXPECT_EQ(0, prefs()->GetInteger(
                   prefs::kCurrentMigrationVersionToGoogleMobileServices));
  EXPECT_EQ(
      (base::Time::Now() - base::Hours(2)).InSecondsFSinceUnixEpoch(),
      prefs()->GetDouble(password_manager::prefs::kTimeOfLastMigrationAttempt));
}

TEST_F(BuiltInBackendToAndroidBackendMigratorTest,
       LastAttemptUnchangedWhenRollingMigrationDisabled) {
  Init(/*current_migration_version=*/1);

  migrator()->StartMigrationIfNecessary(
      /*should_attempt_upm_reenrollment=*/false);
  RunUntilIdle();

  EXPECT_EQ(1, prefs()->GetInteger(
                   prefs::kCurrentMigrationVersionToGoogleMobileServices));
  EXPECT_EQ(0, prefs()->GetDouble(
                   password_manager::prefs::kTimeOfLastMigrationAttempt));
}

TEST_F(BuiltInBackendToAndroidBackendMigratorTest,
       LastAttemptUpdatedInPrefsWhenRollingMigrationEnabled) {
  // Setup the pref to indicate that the initial migration has happened already.
  feature_list().InitAndEnableFeature(
      features::kUnifiedPasswordManagerLocalPasswordsAndroidWithMigration);
  Init(/*current_migration_version=*/1);

  migrator()->StartMigrationIfNecessary(
      /*should_attempt_upm_reenrollment=*/false);
  RunUntilIdle();

  EXPECT_EQ(1, prefs()->GetInteger(
                   prefs::kCurrentMigrationVersionToGoogleMobileServices));
  EXPECT_EQ(
      base::Time::Now().InSecondsFSinceUnixEpoch(),
      prefs()->GetDouble(password_manager::prefs::kTimeOfLastMigrationAttempt));
}

TEST_F(BuiltInBackendToAndroidBackendMigratorTest,
       InitialMigrationNeverStartedMetrics) {
  base::HistogramTester histogram_tester;
  const char kMigrationFinishedMetric[] =
      "PasswordManager.UnifiedPasswordManager.WasMigrationDone";
  Init();

  histogram_tester.ExpectTotalCount(kMigrationFinishedMetric, 1);
  histogram_tester.ExpectBucketCount(kMigrationFinishedMetric, false, 1);
}

TEST_F(BuiltInBackendToAndroidBackendMigratorTest,
       InitialMigrationFinishedMetrics) {
  base::HistogramTester histogram_tester;
  const char kMigrationFinishedMetric[] =
      "PasswordManager.UnifiedPasswordManager.WasMigrationDone";

  Init(/*current_migration_version=*/1);

  histogram_tester.ExpectUniqueSample(kMigrationFinishedMetric, true, 1);
}

TEST_F(BuiltInBackendToAndroidBackendMigratorTest,
       MigrationForSyncingUserShouldMoveLocalOnlyDataToAndroidBackend) {
  Init();
  InitSyncService(/*is_password_sync_enabled=*/true);

  PasswordForm form = CreateTestPasswordForm();
  android_backend().AddLoginAsync(form, base::DoNothing());

  // 'skip_zero_click' is a local only field in PasswordForm and hence not
  // available in Android backend before the migration.
  PasswordForm form_with_local_data = form;
  form_with_local_data.skip_zero_click = true;
  built_in_backend().AddLoginAsync(form_with_local_data, base::DoNothing());

  migrator()->StartMigrationIfNecessary(
      /*should_attempt_upm_reenrollment=*/false);
  RunUntilIdle();

  base::MockCallback<LoginsOrErrorReply> mock_reply;
  EXPECT_CALL(
      mock_reply,
      Run(VariantWith<LoginsResult>(ElementsAre(form_with_local_data))));
  android_backend().GetAllLoginsAsync(mock_reply.Get());
  RunUntilIdle();
}

TEST_F(BuiltInBackendToAndroidBackendMigratorTest,
       MigrationUserAfterSyncDisablingShouldMoveLocalOnlyDataToBuiltInBackend) {
  Init();

  // Simulate sync being recently disabled.
  prefs()->SetBoolean(prefs::kRequiresMigrationAfterSyncStatusChange, true);
  InitSyncService(/*is_password_sync_enabled=*/false);

  PasswordForm form = CreateTestPasswordForm();
  built_in_backend().AddLoginAsync(form, base::DoNothing());

  // 'skip_zero_click' is a local only field in PasswordForm and hence not
  // available in the built-in backend before the migration.
  PasswordForm form_with_local_data = form;
  form_with_local_data.skip_zero_click = true;
  android_backend().AddLoginAsync(form_with_local_data, base::DoNothing());

  migrator()->StartMigrationIfNecessary(
      /*should_attempt_upm_reenrollment=*/false);
  RunUntilIdle();

  base::MockCallback<LoginsOrErrorReply> mock_reply;
  EXPECT_CALL(
      mock_reply,
      Run(VariantWith<LoginsResult>(ElementsAre(form_with_local_data))));
  android_backend().GetAllLoginsAsync(mock_reply.Get());
  RunUntilIdle();
}

// Tests that migration removes blocklisted entries with non-empty username or
// values from the built in backlend before writing to the Android backend.
TEST_F(BuiltInBackendToAndroidBackendMigratorTest,
       MigrationClearsBlocklistedCredentials) {
  Init();
  InitSyncService(/*is_password_sync_enabled=*/true);

  // Add two incorrect entries to the local database to check if they will be
  // removed before writing to the android backend
  PasswordForm form_1 = CreateTestPasswordForm(1);
  form_1.blocked_by_user = true;
  form_1.username_value.clear();
  built_in_backend().AddLoginAsync(form_1, base::DoNothing());

  PasswordForm form_2 = CreateTestPasswordForm(2);
  form_2.blocked_by_user = true;
  form_1.password_value.clear();
  built_in_backend().AddLoginAsync(form_2, base::DoNothing());

  migrator()->StartMigrationIfNecessary(
      /*should_attempt_upm_reenrollment=*/false);
  RunUntilIdle();

  base::MockCallback<LoginsOrErrorReply> mock_reply;
  // Credentials should be cleaned in both android and built in backends.
  EXPECT_CALL(mock_reply, Run(VariantWith<LoginsResult>((IsEmpty())))).Times(2);
  android_backend().GetAllLoginsAsync(mock_reply.Get());
  built_in_backend().GetAllLoginsAsync(mock_reply.Get());
  RunUntilIdle();
}

// Tests that migration does not affect username and password for
// non-blocklisted entries.
TEST_F(BuiltInBackendToAndroidBackendMigratorTest,
       MigrationDoesNotClearNonBlocklistedCredentials) {
  Init();
  InitSyncService(/*is_password_sync_enabled=*/true);

  // Add two incorrect entries to the local database to check if they will be
  // fixed before writing to the android backend
  PasswordForm form_1 = CreateTestPasswordForm(1);
  built_in_backend().AddLoginAsync(form_1, base::DoNothing());

  PasswordForm form_2 = CreateTestPasswordForm(2);
  built_in_backend().AddLoginAsync(form_2, base::DoNothing());

  // Add one form to be updated.
  android_backend().AddLoginAsync(form_1, base::DoNothing());
  RunUntilIdle();

  migrator()->StartMigrationIfNecessary(
      /*should_attempt_upm_reenrollment=*/false);
  RunUntilIdle();

  base::MockCallback<LoginsOrErrorReply> mock_reply;
  // Credentials should be cleaned in both android and built in backends.
  EXPECT_CALL(mock_reply,
              Run(VariantWith<LoginsResult>(ElementsAre(form_1, form_2))))
      .Times(2);
  android_backend().GetAllLoginsAsync(mock_reply.Get());
  built_in_backend().GetAllLoginsAsync(mock_reply.Get());
  RunUntilIdle();
}

TEST_F(BuiltInBackendToAndroidBackendMigratorTest,
       ReenrollmentAttemptShouldMoveLocalOnlyDataToAndroidBackend) {
  Init();
  InitSyncService(/*is_password_sync_enabled=*/true);

  prefs()->SetBoolean(prefs::kUnenrolledFromGoogleMobileServicesDueToErrors,
                      true);
  const int initial_num_reenrollments =
      prefs()->GetInteger(prefs::kTimesReenrolledToGoogleMobileServices);
  prefs()->SetInteger(prefs::kTimesAttemptedToReenrollToGoogleMobileServices,
                      10);

  PasswordForm form = CreateTestPasswordForm();
  android_backend().AddLoginAsync(form, base::DoNothing());

  // 'skip_zero_click' is a local only field in PasswordForm and hence not
  // available in Android backend before the migration.
  PasswordForm form_with_local_data = form;
  form_with_local_data.skip_zero_click = true;
  built_in_backend().AddLoginAsync(form_with_local_data, base::DoNothing());

  migrator()->StartMigrationIfNecessary(
      /*should_attempt_upm_reenrollment=*/true);
  RunUntilIdle();

  base::MockCallback<LoginsOrErrorReply> mock_reply;
  EXPECT_CALL(
      mock_reply,
      Run(VariantWith<LoginsResult>(ElementsAre(form_with_local_data))));
  android_backend().GetAllLoginsAsync(mock_reply.Get());
  RunUntilIdle();

  // Since the migration has completed successfully, the user should be
  // reenrolled into UPM.
  EXPECT_FALSE(prefs()->GetBoolean(
      prefs::kUnenrolledFromGoogleMobileServicesDueToErrors));
  EXPECT_EQ(prefs()->GetInteger(prefs::kTimesReenrolledToGoogleMobileServices),
            initial_num_reenrollments + 1);
  EXPECT_EQ(prefs()->GetInteger(
                prefs::kTimesAttemptedToReenrollToGoogleMobileServices),
            0);
}

// Holds the built in and android backend's logins and the expected result after
// the migration.
struct MigrationParam {
  struct Entry {
    Entry(int index,
          std::string password = "",
          base::TimeDelta date_created = base::TimeDelta())
        : index(index), password(password), date_created(date_created) {}

    PasswordForm ToPasswordForm() const {
      PasswordForm form = CreateTestPasswordForm(index);
      form.password_value = base::ASCIIToUTF16(password);
      form.date_created = base::Time() + date_created;
      return form;
    }

    int index;
    std::string password;
    base::TimeDelta date_created;
  };

  std::vector<PasswordForm> GetBuiltInLogins() const {
    return EntriesToPasswordForms(built_in_logins);
  }

  std::vector<PasswordForm> GetAndroidLogins() const {
    return EntriesToPasswordForms(android_logins);
  }

  std::vector<PasswordForm> GetMergedLogins() const {
    return EntriesToPasswordForms(merged_logins);
  }

  std::vector<PasswordForm> GetUpdatedAndroidLogins() const {
    return EntriesToPasswordForms(updated_android_logins);
  }

  std::vector<PasswordForm> EntriesToPasswordForms(
      const std::vector<Entry>& entries) const {
    std::vector<PasswordForm> v;
    base::ranges::transform(entries, std::back_inserter(v),
                            &Entry::ToPasswordForm);
    return v;
  }

  std::vector<Entry> built_in_logins;
  std::vector<Entry> android_logins;
  std::vector<Entry> merged_logins;
  std::vector<Entry> updated_android_logins;
};

// Tests that initial and rolling migration actually works by comparing
// passwords in built-in/android backend before and after migration.
class BuiltInBackendToAndroidBackendMigratorTestWithMigrationParams
    : public BuiltInBackendToAndroidBackendMigratorTest,
      public testing::WithParamInterface<MigrationParam> {};

// Tests the initial migration result.
TEST_P(BuiltInBackendToAndroidBackendMigratorTestWithMigrationParams,
       InitialMigrationForSyncingUsers) {
  BuiltInBackendToAndroidBackendMigratorTest::Init();

  InitSyncService(/*is_password_sync_enabled=*/true);

  const MigrationParam& p = GetParam();

  for (const auto& login : p.GetBuiltInLogins()) {
    built_in_backend().AddLoginAsync(login, base::DoNothing());
  }
  for (const auto& login : p.GetAndroidLogins()) {
    android_backend().AddLoginAsync(login, base::DoNothing());
  }
  RunUntilIdle();

  migrator()->StartMigrationIfNecessary(
      /*should_attempt_upm_reenrollment=*/false);
  RunUntilIdle();

  // The built-in logins should not be affected.
  base::MockCallback<LoginsOrErrorReply> built_in_reply;
  EXPECT_CALL(
      built_in_reply,
      Run(VariantWith<LoginsResult>(ElementsAreArray(p.GetBuiltInLogins()))));
  built_in_backend().GetAllLoginsAsync(built_in_reply.Get());

  // The android logins are updated. Existing logins are retained.
  base::MockCallback<LoginsOrErrorReply> android_reply;
  EXPECT_CALL(android_reply, Run(VariantWith<LoginsResult>(ElementsAreArray(
                                 p.GetUpdatedAndroidLogins()))));
  android_backend().GetAllLoginsAsync(android_reply.Get());
  RunUntilIdle();
}

// Tests the initial migration result.
TEST_P(BuiltInBackendToAndroidBackendMigratorTestWithMigrationParams,
       MigrationAfterEnrollingIntoTheExperiment) {
  // Set current_migration_version to 0 to imitate a user enrolling into the
  // experiment.
  BuiltInBackendToAndroidBackendMigratorTest::Init(
      /*current_migration_version=*/0);

  InitSyncService(/*is_password_sync_enabled=*/false);

  feature_list().InitAndEnableFeature(
      features::kUnifiedPasswordManagerLocalPasswordsAndroidWithMigration);

  const MigrationParam& p = GetParam();

  for (const auto& login : p.GetBuiltInLogins()) {
    built_in_backend().AddLoginAsync(login, base::DoNothing());
  }
  for (const auto& login : p.GetAndroidLogins()) {
    android_backend().AddLoginAsync(login, base::DoNothing());
  }
  RunUntilIdle();

  migrator()->StartMigrationIfNecessary(
      /*should_attempt_upm_reenrollment=*/false);
  RunUntilIdle();

  for (auto* const backend : {&android_backend(), &built_in_backend()}) {
    base::MockCallback<LoginsOrErrorReply> mock_reply;
    EXPECT_CALL(
        mock_reply,
        Run(VariantWith<LoginsResult>(ElementsAreArray(p.GetMergedLogins()))));
    backend->GetAllLoginsAsync(mock_reply.Get());
    RunUntilIdle();
  }
}

TEST_P(BuiltInBackendToAndroidBackendMigratorTestWithMigrationParams,
       RollingMigration) {
  // Setup the pref to indicate that the initial migration has happened already.
  // This implies that rolling migration will take place!
  feature_list().InitAndEnableFeature(
      features::kUnifiedPasswordManagerLocalPasswordsAndroidWithMigration);
  BuiltInBackendToAndroidBackendMigratorTest::Init(
      /*current_migration_version=*/1);

  const MigrationParam& p = GetParam();

  for (const auto& login : p.GetBuiltInLogins()) {
    built_in_backend().AddLoginAsync(login, base::DoNothing());
  }
  for (const auto& login : p.GetAndroidLogins()) {
    android_backend().AddLoginAsync(login, base::DoNothing());
  }
  RunUntilIdle();

  migrator()->StartMigrationIfNecessary(
      /*should_attempt_upm_reenrollment=*/false);
  RunUntilIdle();

  for (auto* const backend : {&android_backend(), &built_in_backend()}) {
    base::MockCallback<LoginsOrErrorReply> mock_reply;
    EXPECT_CALL(
        mock_reply,
        Run(VariantWith<LoginsResult>(ElementsAreArray(p.GetAndroidLogins()))));
    backend->GetAllLoginsAsync(mock_reply.Get());
    RunUntilIdle();
  }
}

INSTANTIATE_TEST_SUITE_P(
    BuiltInBackendToAndroidBackendMigratorTest,
    BuiltInBackendToAndroidBackendMigratorTestWithMigrationParams,
    testing::Values(
        MigrationParam{.built_in_logins = {},
                       .android_logins = {},
                       .merged_logins = {},
                       .updated_android_logins = {}},
        MigrationParam{.built_in_logins = {{1}, {2}},
                       .android_logins = {},
                       .merged_logins = {{1}, {2}},
                       .updated_android_logins = {{1}, {2}}},
        MigrationParam{.built_in_logins = {},
                       .android_logins = {{1}, {2}},
                       .merged_logins = {{1}, {2}},
                       .updated_android_logins = {{1}, {2}}},
        MigrationParam{.built_in_logins = {{1}, {2}},
                       .android_logins = {{3}},
                       .merged_logins = {{1}, {2}, {3}},
                       .updated_android_logins = {{1}, {2}, {3}}},
        MigrationParam{.built_in_logins = {{1}, {2}, {3}},
                       .android_logins = {{1}, {2}, {3}},
                       .merged_logins = {{1}, {2}, {3}},
                       .updated_android_logins = {{1}, {2}, {3}}},
        MigrationParam{
            .built_in_logins = {{1, "old_password", base::Days(1)}, {2}},
            .android_logins = {{1, "new_password", base::Days(2)}, {3}},
            .merged_logins = {{1, "new_password", base::Days(2)}, {2}, {3}},
            .updated_android_logins = {{1, "old_password", base::Days(1)},
                                       {2},
                                       {3}}},
        MigrationParam{
            .built_in_logins = {{1, "new_password", base::Days(2)}, {2}},
            .android_logins = {{1, "old_password", base::Days(1)}, {3}},
            .merged_logins = {{1, "new_password", base::Days(2)}, {2}, {3}},
            .updated_android_logins = {{1, "new_password", base::Days(2)},
                                       {2},
                                       {3}}}));

struct MigrationParamForMetrics {
  // Whether migration has already happened.
  bool migration_ran_before;
  // Whether password sync is enabled in settings.
  bool is_sync_enabled;
  // Whether non-syncable migration is required after a change in sync status.
  bool is_non_syncable_data_migration;
  // Whether migration should complete successfully or not.
  bool is_successful_migration;
  // Expected migration type for metrics recording.
  std::string expected_migration_type;
};

class BuiltInBackendToAndroidBackendMigratorTestMetrics
    : public BuiltInBackendToAndroidBackendMigratorTest,
      public testing::WithParamInterface<MigrationParamForMetrics> {
 protected:
  BuiltInBackendToAndroidBackendMigratorTestMetrics() {
    prefs()->registry()->RegisterIntegerPref(
        prefs::kCurrentMigrationVersionToGoogleMobileServices, 0);
    prefs()->registry()->RegisterDoublePref(prefs::kTimeOfLastMigrationAttempt,
                                            0.0);
    prefs()->registry()->RegisterBooleanPref(
        prefs::kRequiresMigrationAfterSyncStatusChange, false);
    prefs()->registry()->RegisterStringPref(
        ::prefs::kGoogleServicesLastSyncingUsername, "testaccount@gmail.com");
    prefs()->registry()->RegisterBooleanPref(
        prefs::kUnenrolledFromGoogleMobileServicesDueToErrors, false);
    prefs()->registry()->RegisterIntegerPref(
        prefs::kUnenrolledFromGoogleMobileServicesAfterApiErrorCode, 0);
    prefs()->registry()->RegisterIntegerPref(
        prefs::kTimesReenrolledToGoogleMobileServices, 0);
    prefs()->registry()->RegisterIntegerPref(
        prefs::kTimesAttemptedToReenrollToGoogleMobileServices, 0);

    if (GetParam().migration_ran_before) {
      // Setup the pref to indicate that the initial migration has happened
      // already.
      prefs()->SetInteger(prefs::kCurrentMigrationVersionToGoogleMobileServices,
                          1);
    }

    latency_metric_ = "PasswordManager.UnifiedPasswordManager." +
                      GetParam().expected_migration_type + ".Latency";
    success_metric_ = "PasswordManager.UnifiedPasswordManager." +
                      GetParam().expected_migration_type + ".Success";

    CreateMigrator(&built_in_backend_, &android_backend_, prefs());

    if (GetParam().is_non_syncable_data_migration) {
      prefs()->SetBoolean(prefs::kRequiresMigrationAfterSyncStatusChange, true);
    }

    if (GetParam().expected_migration_type == "ReenrollmentAttempt") {
      prefs()->SetBoolean(prefs::kUnenrolledFromGoogleMobileServicesDueToErrors,
                          true);
    }
  }

  std::string latency_metric_;
  std::string success_metric_;
  ::testing::StrictMock<MockPasswordStoreBackend> built_in_backend_;
  ::testing::StrictMock<MockPasswordStoreBackend> android_backend_;
};

TEST_P(BuiltInBackendToAndroidBackendMigratorTestMetrics,
       MigrationMetricsTest) {
  base::HistogramTester histogram_tester;

  InitSyncService(/*is_password_sync_enabled=*/GetParam().is_sync_enabled);

  auto test_migration_callback = [](LoginsOrErrorReply reply) -> void {
    LoginsResultOrError result = GetParam().is_successful_migration
                                     ? LoginsResultOrError(LoginsResult())
                                     : LoginsResultOrError(kBackendError);
    base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE, base::BindOnce(std::move(reply), std::move(result)),
        kLatencyDelta);
  };

  if (GetParam().expected_migration_type == "InitialMigrationForSyncUsers" ||
      GetParam().expected_migration_type ==
          "NonSyncableDataMigrationToAndroidBackend" ||
      GetParam().expected_migration_type == "ReenrollmentAttemptMigration") {
    EXPECT_CALL(built_in_backend_, GetAllLoginsAsync)
        .WillOnce(WithArg<0>(Invoke(test_migration_callback)));
  } else if (GetParam().expected_migration_type ==
             "NonSyncableDataMigrationToBuiltInBackend") {
    EXPECT_CALL(android_backend_, GetAllLoginsForAccountAsync)
        .WillOnce(WithArg<1>(Invoke(test_migration_callback)));
  }

  migrator()->StartMigrationIfNecessary(GetParam().expected_migration_type ==
                                        "ReenrollmentAttemptMigration");
  FastForwardBy(kLatencyDelta);

  histogram_tester.ExpectTotalCount(latency_metric_, 1);
  histogram_tester.ExpectTimeBucketCount(latency_metric_, kLatencyDelta, 1);
  histogram_tester.ExpectUniqueSample(success_metric_,
                                      GetParam().is_successful_migration, 1);
}

// TODO(crbug.com/1306001): Add cases for rolling migration and non-syncing
// users or clean up.
INSTANTIATE_TEST_SUITE_P(
    BuiltInBackendToAndroidBackendMigratorTest,
    BuiltInBackendToAndroidBackendMigratorTestMetrics,
    testing::Values(
        // Successful initial migration.
        MigrationParamForMetrics{
            .migration_ran_before = false,
            .is_sync_enabled = true,
            .is_non_syncable_data_migration = false,
            .is_successful_migration = true,
            .expected_migration_type = "InitialMigrationForSyncUsers"},
        // Unsuccessful initial migration.
        MigrationParamForMetrics{
            .migration_ran_before = false,
            .is_sync_enabled = true,
            .is_non_syncable_data_migration = false,
            .is_successful_migration = false,
            .expected_migration_type = "InitialMigrationForSyncUsers"},
        // Successful non-syncable data migration to the android backend.
        MigrationParamForMetrics{
            .migration_ran_before = true,
            .is_sync_enabled = true,
            .is_non_syncable_data_migration = true,
            .is_successful_migration = true,
            .expected_migration_type =
                "NonSyncableDataMigrationToAndroidBackend"},
        // Unsuccessful non-syncable data migration to the android backend.
        MigrationParamForMetrics{
            .migration_ran_before = true,
            .is_sync_enabled = true,
            .is_non_syncable_data_migration = true,
            .is_successful_migration = false,
            .expected_migration_type =
                "NonSyncableDataMigrationToAndroidBackend"},
        // Successful non-syncable data migration to the built-in backend.
        MigrationParamForMetrics{
            .migration_ran_before = true,
            .is_sync_enabled = false,
            .is_non_syncable_data_migration = true,
            .is_successful_migration = true,
            .expected_migration_type =
                "NonSyncableDataMigrationToBuiltInBackend"},
        // Unsuccessful non-syncable data migration to the built-in backend.
        MigrationParamForMetrics{
            .migration_ran_before = true,
            .is_sync_enabled = false,
            .is_non_syncable_data_migration = true,
            .is_successful_migration = false,
            .expected_migration_type =
                "NonSyncableDataMigrationToBuiltInBackend"},
        // Successful reenrollment attempt.
        MigrationParamForMetrics{
            .migration_ran_before = true,
            .is_sync_enabled = true,
            .is_non_syncable_data_migration = false,
            .is_successful_migration = true,
            .expected_migration_type = "ReenrollmentAttemptMigration"},
        // Unsuccessful reenrollment attempt.
        MigrationParamForMetrics{
            .migration_ran_before = true,
            .is_sync_enabled = true,
            .is_non_syncable_data_migration = false,
            .is_successful_migration = false,
            .expected_migration_type = "ReenrollmentAttemptMigration"}));

class BuiltInBackendToAndroidBackendMigratorWithMockAndroidBackendTest
    : public BuiltInBackendToAndroidBackendMigratorTest {
 protected:
  BuiltInBackendToAndroidBackendMigratorWithMockAndroidBackendTest() {
    prefs()->registry()->RegisterIntegerPref(
        prefs::kCurrentMigrationVersionToGoogleMobileServices, 0);
    prefs()->registry()->RegisterDoublePref(prefs::kTimeOfLastMigrationAttempt,
                                            0.0);
    prefs()->registry()->RegisterBooleanPref(
        prefs::kRequiresMigrationAfterSyncStatusChange, false);

    CreateMigrator(&built_in_backend_, &android_backend_, prefs());
  }

  PasswordStoreBackend& built_in_backend() { return built_in_backend_; }

  ::testing::NiceMock<MockPasswordStoreBackend> android_backend_;

 private:
  FakePasswordStoreBackend built_in_backend_;
};

TEST_F(BuiltInBackendToAndroidBackendMigratorWithMockAndroidBackendTest,
       DoesNotCompleteMigrationWhenWritingToAndroidBackendFails_SyncOn) {
  InitSyncService(/*is_password_sync_enabled=*/true);

  // Add two credentials to the built-in backend.
  built_in_backend().AddLoginAsync(CreateTestPasswordForm(/*index=*/1),
                                   base::DoNothing());
  built_in_backend().AddLoginAsync(CreateTestPasswordForm(/*index=*/2),
                                   base::DoNothing());

  // Simulate an Android backend that fails to write.
  ON_CALL(android_backend_, UpdateLoginAsync)
      .WillByDefault(
          WithArg<1>(Invoke([](PasswordChangesOrErrorReply callback) -> void {
            base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
                FROM_HERE, base::BindOnce(std::move(callback), kBackendError));
          })));

  // Once one UpdateLoginAsync() call fails, all consecutive ones will not be
  // executed. Check that exactly one UpdateLoginAsync() is called.
  EXPECT_CALL(android_backend_, UpdateLoginAsync).Times(1);

  migrator()->StartMigrationIfNecessary(
      /*should_attempt_upm_reenrollment=*/false);

  // Migration version is still 0 since migration didn't complete.
  EXPECT_EQ(0, prefs()->GetInteger(
                   prefs::kCurrentMigrationVersionToGoogleMobileServices));
  RunUntilIdle();
}

TEST_F(BuiltInBackendToAndroidBackendMigratorWithMockAndroidBackendTest,
       DoesNotCompleteMigrationWhenWritingToAndroidBackendFails_SyncOff) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      features::kUnifiedPasswordManagerLocalPasswordsAndroidWithMigration);

  // Sync state doesn't affect this test, run it arbitrarily for non-sync'ing
  // users.
  InitSyncService(/*is_password_sync_enabled=*/false);

  // Add two credentials to the built-in backend.
  built_in_backend().AddLoginAsync(CreateTestPasswordForm(/*index=*/1),
                                   base::DoNothing());
  built_in_backend().AddLoginAsync(CreateTestPasswordForm(/*index=*/2),
                                   base::DoNothing());

  // Simulate an empty Android backend.
  EXPECT_CALL(android_backend_, GetAllLoginsAsync)
      .WillOnce(WithArg<0>(Invoke([](LoginsOrErrorReply reply) -> void {
        base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
            FROM_HERE, base::BindOnce(std::move(reply), LoginsResult()));
      })));

  // Simulate an Android backend that fails to write.
  ON_CALL(android_backend_, AddLoginAsync)
      .WillByDefault(
          WithArg<1>(Invoke([](PasswordChangesOrErrorReply callback) -> void {
            base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
                FROM_HERE, base::BindOnce(std::move(callback), kBackendError));
          })));

  // Once one AddLoginAsync() call fails, all consecutive ones will not be
  // executed. Check that exactly one AddLoginAsync() is called.
  EXPECT_CALL(android_backend_, AddLoginAsync).Times(1);

  migrator()->StartMigrationIfNecessary(
      /*should_attempt_upm_reenrollment=*/false);

  // Migration version is still 0 since migration didn't complete.
  EXPECT_EQ(0, prefs()->GetInteger(
                   prefs::kCurrentMigrationVersionToGoogleMobileServices));
  RunUntilIdle();
}

TEST_F(BuiltInBackendToAndroidBackendMigratorWithMockAndroidBackendTest,
       SecondMigrationCannotStartWhileTheFirstOneHasNotCompleted) {
  InitSyncService(/*is_password_sync_enabled=*/true);

  // Add a form to the built-in backend to have something to migrate.
  PasswordForm form = CreateTestPasswordForm();
  built_in_backend().AddLoginAsync(form, base::DoNothing());

  // Call StartMigrationIfNecessary for the first time.
  migrator()->StartMigrationIfNecessary(
      /*should_attempt_upm_reenrollment=*/false);
  RunUntilIdle();

  // If the user gets evicted from the experiment, migration-related prefs are
  // cleared.
  prefs()->ClearPref(password_manager::prefs::kTimeOfLastMigrationAttempt);

  // Simulate some time passing before the second migration is triggered.
  FastForwardBy(base::Milliseconds(123u));

  // Call StartMigrationIfNecessary for the second time before the first
  // migration finishes in an attempt to reenroll.
  migrator()->StartMigrationIfNecessary(
      /*should_attempt_upm_reenrollment=*/true);
  RunUntilIdle();

  // Check the recorded last migration attempt time. It should not be recorded
  // after the pref was cleared, because the second migration should not be
  // triggered.
  EXPECT_EQ(0, prefs()->GetDouble(
                   password_manager::prefs::kTimeOfLastMigrationAttempt));
}

}  // namespace password_manager
