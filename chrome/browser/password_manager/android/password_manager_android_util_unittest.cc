// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/android/password_manager_android_util.h"

#include <limits>
#include <memory>

#include "base/android/build_info.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/to_string.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/test_file_util.h"
#include "build/build_config.h"
#include "chrome/browser/password_manager/android/mock_password_manager_util_bridge.h"
#include "components/browser_sync/sync_to_signin_migration.h"
#include "components/password_manager/core/browser/export/login_db_deprecation_password_exporter.h"
#include "components/password_manager/core/browser/password_manager_constants.h"
#include "components/password_manager/core/browser/split_stores_and_local_upm.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "components/signin/public/base/consent_level.h"
#include "components/sync/base/data_type.h"
#include "components/sync/base/pref_names.h"
#include "components/sync/test/test_sync_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using password_manager::GetLocalUpmMinGmsVersion;
using password_manager::prefs::kUpmAutoExportCsvNeedsDeletion;
using password_manager::prefs::kUpmUnmigratedPasswordsExported;
using testing::Return;

namespace password_manager_android_util {
namespace {

class PasswordManagerAndroidUtilTest : public testing::Test {
 public:
  PasswordManagerAndroidUtilTest() {
    password_manager::RegisterLegacySplitStoresPref(pref_service_.registry());
    pref_service_.registry()->RegisterBooleanPref(
        password_manager::prefs::kCredentialsEnableService, false);
    pref_service_.registry()->RegisterBooleanPref(
        password_manager::prefs::kCredentialsEnableAutosignin, false);
    pref_service_.registry()->RegisterBooleanPref(
        password_manager::prefs::kEmptyProfileStoreLoginDatabase, false);
    pref_service_.registry()->RegisterBooleanPref(
        syncer::prefs::internal::kSyncInitialSyncFeatureSetupComplete, false);
    pref_service_.registry()->RegisterBooleanPref(
        syncer::prefs::internal::kSyncKeepEverythingSynced, false);
    pref_service_.registry()->RegisterBooleanPref(
        base::StrCat(
            {syncer::prefs::internal::
                 kSyncDataTypeStatusForSyncToSigninMigrationPrefix,
             ".", syncer::DataTypeToStableLowerCaseString(syncer::PASSWORDS)}),
        false);
    pref_service_.registry()->RegisterBooleanPref(
        password_manager::prefs::kUpmUnmigratedPasswordsExported, false);
    pref_service_.registry()->RegisterBooleanPref(
        password_manager::prefs::kUpmAutoExportCsvNeedsDeletion, false);

    SetPasswordSyncEnabledPref(false);
    base::WriteFile(login_db_directory_.Append(
                        password_manager::kLoginDataForProfileFileName),
                    "");

    // Most tests check the modern GmsCore case.
    base::android::BuildInfo::GetInstance()->set_gms_version_code_for_test(
        base::NumberToString(GetLocalUpmMinGmsVersion()));
  }

  // MaybeDeleteLoginDatabases() reads whether password sync is enabled
  // from a pref rather than the SyncService. This helper sets such pref.
  void SetPasswordSyncEnabledPref(bool enabled) {
    if (enabled) {
      pref_service_.SetBoolean(
          syncer::prefs::internal::kSyncInitialSyncFeatureSetupComplete, true);
      pref_service_.SetBoolean(
          syncer::prefs::internal::kSyncKeepEverythingSynced, true);
      pref_service_.SetBoolean(
          base::StrCat(
              {syncer::prefs::internal::
                   kSyncDataTypeStatusForSyncToSigninMigrationPrefix,
               ".",
               syncer::DataTypeToStableLowerCaseString(syncer::PASSWORDS)}),
          true);
      ASSERT_EQ(browser_sync::GetSyncToSigninMigrationDataTypeDecision(
                    &pref_service_, syncer::PASSWORDS,
                    syncer::prefs::internal::kSyncPasswords),
                browser_sync::SyncToSigninMigrationDataTypeDecision::kMigrate);
    } else {
      pref_service_.SetBoolean(
          syncer::prefs::internal::kSyncInitialSyncFeatureSetupComplete, false);
      ASSERT_EQ(browser_sync::GetSyncToSigninMigrationDataTypeDecision(
                    &pref_service_, syncer::PASSWORDS,
                    syncer::prefs::internal::kSyncPasswords),
                browser_sync::SyncToSigninMigrationDataTypeDecision::
                    kDontMigrateTypeDisabled);
    }
  }

  TestingPrefServiceSimple* pref_service() { return &pref_service_; }

  const base::FilePath& login_db_directory() { return login_db_directory_; }

  std::unique_ptr<MockPasswordManagerUtilBridge>
  GetMockBridgeWithBackendPresent() {
    auto mock_bridge = std::make_unique<MockPasswordManagerUtilBridge>();
    ON_CALL(*mock_bridge, IsInternalBackendPresent).WillByDefault(Return(true));
    return mock_bridge;
  }

 private:
  TestingPrefServiceSimple pref_service_;
  const base::FilePath login_db_directory_ =
      base::CreateUniqueTempDirectoryScopedToTest();
};

TEST_F(PasswordManagerAndroidUtilTest,
       PasswordManagerNotAvailableNoInternalBackend) {
  // Make sure all the other criteria are fulfilled.
  base::android::BuildInfo::GetInstance()->set_gms_version_code_for_test(
      base::NumberToString(GetLocalUpmMinGmsVersion()));
  password_manager::SetLegacySplitStoresPrefForTest(pref_service(), true);
  pref_service()->SetBoolean(kUpmUnmigratedPasswordsExported, false);

  std::unique_ptr<MockPasswordManagerUtilBridge> mock_util_bridge =
      std::make_unique<MockPasswordManagerUtilBridge>();
  EXPECT_CALL(*mock_util_bridge, IsInternalBackendPresent)
      .WillOnce(Return(false));
  EXPECT_FALSE(
      IsPasswordManagerAvailable(pref_service(), std::move(mock_util_bridge)));
}

TEST_F(PasswordManagerAndroidUtilTest,
       PasswordManagerNotAvailableGmsVersionTooLow) {
  std::unique_ptr<MockPasswordManagerUtilBridge> mock_util_bridge =
      std::make_unique<MockPasswordManagerUtilBridge>();
  EXPECT_CALL(*mock_util_bridge, IsInternalBackendPresent)
      .WillOnce(Return(true));
  password_manager::SetLegacySplitStoresPrefForTest(pref_service(), true);
  pref_service()->SetBoolean(kUpmUnmigratedPasswordsExported, false);

  // Set a GMS Core version that is lower than the min required version.
  base::android::BuildInfo::GetInstance()->set_gms_version_code_for_test(
      base::NumberToString(GetLocalUpmMinGmsVersion() - 1));

  EXPECT_FALSE(
      IsPasswordManagerAvailable(pref_service(), std::move(mock_util_bridge)));
}

TEST_F(PasswordManagerAndroidUtilTest,
       PasswordManagerNotAvailablePasswordsUnmigratedPasswords) {
  std::unique_ptr<MockPasswordManagerUtilBridge> mock_util_bridge =
      std::make_unique<MockPasswordManagerUtilBridge>();
  EXPECT_CALL(*mock_util_bridge, IsInternalBackendPresent)
      .WillOnce(Return(true));

  base::android::BuildInfo::GetInstance()->set_gms_version_code_for_test(
      base::NumberToString(GetLocalUpmMinGmsVersion()));

  password_manager::SetLegacySplitStoresPrefForTest(pref_service(), false);
  pref_service()->SetBoolean(kUpmUnmigratedPasswordsExported, false);

  EXPECT_FALSE(
      IsPasswordManagerAvailable(pref_service(), std::move(mock_util_bridge)));
}

TEST_F(PasswordManagerAndroidUtilTest, PasswordManagerAvailableNoUpmMigration) {
  std::unique_ptr<MockPasswordManagerUtilBridge> mock_util_bridge =
      std::make_unique<MockPasswordManagerUtilBridge>();
  EXPECT_CALL(*mock_util_bridge, IsInternalBackendPresent)
      .WillOnce(Return(true));

  base::android::BuildInfo::GetInstance()->set_gms_version_code_for_test(
      base::NumberToString(GetLocalUpmMinGmsVersion()));

  password_manager::SetLegacySplitStoresPrefForTest(pref_service(), false);
  pref_service()->SetBoolean(kUpmUnmigratedPasswordsExported, true);

  EXPECT_TRUE(
      IsPasswordManagerAvailable(pref_service(), std::move(mock_util_bridge)));
}

TEST_F(PasswordManagerAndroidUtilTest, PasswordManagerAvailableUpmMigration) {
  std::unique_ptr<MockPasswordManagerUtilBridge> mock_util_bridge =
      std::make_unique<MockPasswordManagerUtilBridge>();
  EXPECT_CALL(*mock_util_bridge, IsInternalBackendPresent)
      .WillOnce(Return(true));

  base::android::BuildInfo::GetInstance()->set_gms_version_code_for_test(
      base::NumberToString(GetLocalUpmMinGmsVersion()));

  password_manager::SetLegacySplitStoresPrefForTest(pref_service(), true);
  pref_service()->SetBoolean(kUpmUnmigratedPasswordsExported, false);

  EXPECT_TRUE(
      IsPasswordManagerAvailable(pref_service(), std::move(mock_util_bridge)));
}

TEST_F(PasswordManagerAndroidUtilTest, TestRecordsUpmNotActiveWhenNoGms) {
  base::android::BuildInfo::GetInstance()->set_gms_version_code_for_test(
      base::NumberToString(GetLocalUpmMinGmsVersion() - 1));

  base::HistogramTester histogram_tester;
  std::unique_ptr<MockPasswordManagerUtilBridge> mock_bridge =
      GetMockBridgeWithBackendPresent();
  EXPECT_CALL(*mock_bridge, IsGooglePlayServicesUpdatable)
      .WillOnce(Return(false));
  MaybeDeleteLoginDatabases(pref_service(), login_db_directory(),
                            std::move(mock_bridge));
  histogram_tester.ExpectUniqueSample("PasswordManager.LocalUpmActivated",
                                      false, 1);
  histogram_tester.ExpectUniqueSample(
      "PasswordManager.Android.NotAvailableReason",
      PasswordManagerNotAvailableReason::kNoGmsCore, 1);
}

TEST_F(PasswordManagerAndroidUtilTest, TestRecordsUpmNotActiveWhenGmsTooOld) {
  base::android::BuildInfo::GetInstance()->set_gms_version_code_for_test(
      base::NumberToString(GetLocalUpmMinGmsVersion() - 1));

  base::HistogramTester histogram_tester;
  std::unique_ptr<MockPasswordManagerUtilBridge> mock_bridge =
      GetMockBridgeWithBackendPresent();
  EXPECT_CALL(*mock_bridge, IsGooglePlayServicesUpdatable)
      .WillOnce(Return(true));
  MaybeDeleteLoginDatabases(pref_service(), login_db_directory(),
                            std::move(mock_bridge));
  histogram_tester.ExpectUniqueSample("PasswordManager.LocalUpmActivated",
                                      false, 1);
  histogram_tester.ExpectUniqueSample(
      "PasswordManager.Android.NotAvailableReason",
      PasswordManagerNotAvailableReason::kOutdatedGmsCore, 1);
}

TEST_F(PasswordManagerAndroidUtilTest,
       TestRecordsUpmNotActivateBeforeAutoExport) {
  base::android::BuildInfo::GetInstance()->set_gms_version_code_for_test(
      base::NumberToString(GetLocalUpmMinGmsVersion()));

  password_manager::SetLegacySplitStoresPrefForTest(pref_service(), false);
  pref_service()->SetBoolean(
      password_manager::prefs::kUpmUnmigratedPasswordsExported, false);
  base::HistogramTester histogram_tester;
  MaybeDeleteLoginDatabases(pref_service(), login_db_directory(),
                            GetMockBridgeWithBackendPresent());
  histogram_tester.ExpectUniqueSample("PasswordManager.LocalUpmActivated",
                                      false, 1);
  histogram_tester.ExpectUniqueSample(
      "PasswordManager.Android.NotAvailableReason",
      PasswordManagerNotAvailableReason::kAutoExportPending, 1);
}

TEST_F(PasswordManagerAndroidUtilTest, TestRecordsUpmActiveIfExported) {
  base::android::BuildInfo::GetInstance()->set_gms_version_code_for_test(
      base::NumberToString(GetLocalUpmMinGmsVersion()));

  password_manager::SetLegacySplitStoresPrefForTest(pref_service(), false);
  pref_service()->SetBoolean(
      password_manager::prefs::kUpmUnmigratedPasswordsExported, true);
  base::HistogramTester histogram_tester;
  MaybeDeleteLoginDatabases(pref_service(), login_db_directory(),
                            GetMockBridgeWithBackendPresent());
  histogram_tester.ExpectUniqueSample("PasswordManager.LocalUpmActivated", true,
                                      1);
  histogram_tester.ExpectTotalCount(
      "PasswordManager.Android.NotAvailableReason", 0);
}

TEST_F(PasswordManagerAndroidUtilTest, TestRecordsUpmActiveIfAlreadyActive) {
  base::android::BuildInfo::GetInstance()->set_gms_version_code_for_test(
      base::NumberToString(GetLocalUpmMinGmsVersion()));

  password_manager::SetLegacySplitStoresPrefForTest(pref_service(), true);
  pref_service()->SetBoolean(
      password_manager::prefs::kUpmUnmigratedPasswordsExported, false);
  base::HistogramTester histogram_tester;
  MaybeDeleteLoginDatabases(pref_service(), login_db_directory(),
                            GetMockBridgeWithBackendPresent());
  histogram_tester.ExpectUniqueSample("PasswordManager.LocalUpmActivated", true,
                                      1);
  histogram_tester.ExpectTotalCount(
      "PasswordManager.Android.NotAvailableReason", 0);
}

TEST_F(PasswordManagerAndroidUtilTest,
       InitUnmigratedExportUnchangedIfMigrated) {
  password_manager::SetLegacySplitStoresPrefForTest(pref_service(), true);
  MaybeDeleteLoginDatabases(pref_service(), login_db_directory(),
                            GetMockBridgeWithBackendPresent());
  EXPECT_TRUE(pref_service()
                  ->FindPreference(
                      password_manager::prefs::kUpmUnmigratedPasswordsExported)
                  ->IsDefaultValue());
}

TEST_F(PasswordManagerAndroidUtilTest, InitUnmigratedExportPrefTrueEmptyDb) {
  password_manager::SetLegacySplitStoresPrefForTest(pref_service(), false);
  pref_service()->SetBoolean(
      password_manager::prefs::kEmptyProfileStoreLoginDatabase, true);
  pref_service()->SetBoolean(
      password_manager::prefs::kUpmUnmigratedPasswordsExported, false);
  MaybeDeleteLoginDatabases(pref_service(), login_db_directory(),
                            GetMockBridgeWithBackendPresent());
  EXPECT_TRUE(pref_service()->GetBoolean(
      password_manager::prefs::kUpmUnmigratedPasswordsExported));
}

TEST_F(PasswordManagerAndroidUtilTest,
       DeletesLoginDataFilesAfterUnmigratedPasswordsExported) {
  base::HistogramTester histogram_tester;
  const char kRemovalStatusProfileMetric[] =
      "PasswordManager.ProfileLoginData.RemovalStatus";
  const char kRemovalStatusAccountMetric[] =
      "PasswordManager.AccountLoginData.RemovalStatus";

  // Assume an unmigrated user.
  password_manager::SetLegacySplitStoresPrefForTest(pref_service(), false);
  // With unmigrated passwords exported.
  pref_service()->SetBoolean(kUpmUnmigratedPasswordsExported, true);

  // And for whom the initial passwords deletion failed, so they still have
  // passwords in the db.
  pref_service()->SetBoolean(
      password_manager::prefs::kEmptyProfileStoreLoginDatabase, false);

  // Creating the login data files for testing.
  base::FilePath profile_db_path = login_db_directory().Append(
      password_manager::kLoginDataForProfileFileName);
  base::FilePath account_db_path = login_db_directory().Append(
      password_manager::kLoginDataForAccountFileName);
  base::FilePath profile_db_journal_path = login_db_directory().Append(
      password_manager::kLoginDataJournalForProfileFileName);
  base::FilePath account_db_journal_path = login_db_directory().Append(
      password_manager::kLoginDataJournalForAccountFileName);

  base::WriteFile(profile_db_path, "Test content");
  base::WriteFile(account_db_path, "Test content");
  base::WriteFile(profile_db_journal_path, "Test content");
  base::WriteFile(account_db_journal_path, "Test content");

  EXPECT_TRUE(PathExists(profile_db_path));
  EXPECT_TRUE(PathExists(account_db_path));
  EXPECT_TRUE(PathExists(profile_db_journal_path));
  EXPECT_TRUE(PathExists(account_db_journal_path));

  MaybeDeleteLoginDatabases(pref_service(), login_db_directory(),
                            GetMockBridgeWithBackendPresent());

  EXPECT_FALSE(PathExists(profile_db_path));
  EXPECT_FALSE(PathExists(account_db_path));
  EXPECT_FALSE(PathExists(profile_db_journal_path));
  EXPECT_FALSE(PathExists(account_db_journal_path));
  EXPECT_TRUE(pref_service()->GetBoolean(
      password_manager::prefs::kEmptyProfileStoreLoginDatabase));

  histogram_tester.ExpectUniqueSample(kRemovalStatusProfileMetric, true, 1);
  histogram_tester.ExpectUniqueSample(kRemovalStatusAccountMetric, true, 1);
}

// This test is relevant for users for whom prior db deletion attempts failed.
TEST_F(PasswordManagerAndroidUtilTest,
       DeletesLoginDataFilesForAlreadyMigratedUser) {
  base::HistogramTester histogram_tester;
  const char kRemovalStatusProfileMetric[] =
      "PasswordManager.ProfileLoginData.RemovalStatus";
  const char kRemovalStatusAccountMetric[] =
      "PasswordManager.AccountLoginData.RemovalStatus";

  // Assume a migrated user.
  password_manager::SetLegacySplitStoresPrefForTest(pref_service(), true);
  // No unmigrated passwords, so nothing was exported.
  pref_service()->SetBoolean(kUpmUnmigratedPasswordsExported, false);
  // And for whom the initial passwords deletion failed, so they still have
  // passwords in the db.
  pref_service()->SetBoolean(
      password_manager::prefs::kEmptyProfileStoreLoginDatabase, false);

  // Creating the login data files for testing.
  base::FilePath profile_db_path = login_db_directory().Append(
      password_manager::kLoginDataForProfileFileName);
  base::FilePath account_db_path = login_db_directory().Append(
      password_manager::kLoginDataForAccountFileName);
  base::FilePath profile_db_journal_path = login_db_directory().Append(
      password_manager::kLoginDataJournalForProfileFileName);
  base::FilePath account_db_journal_path = login_db_directory().Append(
      password_manager::kLoginDataJournalForAccountFileName);

  base::WriteFile(profile_db_path, "Test content");
  base::WriteFile(account_db_path, "Test content");
  base::WriteFile(profile_db_journal_path, "Test content");
  base::WriteFile(account_db_journal_path, "Test content");

  EXPECT_TRUE(PathExists(profile_db_path));
  EXPECT_TRUE(PathExists(account_db_path));
  EXPECT_TRUE(PathExists(profile_db_journal_path));
  EXPECT_TRUE(PathExists(account_db_journal_path));

  MaybeDeleteLoginDatabases(pref_service(), login_db_directory(),
                            GetMockBridgeWithBackendPresent());

  EXPECT_FALSE(PathExists(profile_db_path));
  EXPECT_FALSE(PathExists(account_db_path));
  EXPECT_FALSE(PathExists(profile_db_journal_path));
  EXPECT_FALSE(PathExists(account_db_journal_path));
  EXPECT_TRUE(pref_service()->GetBoolean(
      password_manager::prefs::kEmptyProfileStoreLoginDatabase));

  histogram_tester.ExpectUniqueSample(kRemovalStatusProfileMetric, true, 1);
  histogram_tester.ExpectUniqueSample(kRemovalStatusAccountMetric, true, 1);
}

TEST_F(PasswordManagerAndroidUtilTest, DeletesExportedCsvIfNeeded) {
  base::HistogramTester histogram_tester;
  // Signal that the file needs deleting. This would happen if the original
  // deletion attempt failed.
  pref_service()->SetBoolean(kUpmAutoExportCsvNeedsDeletion, true);

  // Creating the csv for testing.
  base::FilePath csv_path =
      login_db_directory().Append(password_manager::kExportedPasswordsFileName);

  base::WriteFile(csv_path, "Test content");

  EXPECT_TRUE(PathExists(csv_path));

  MaybeDeleteLoginDatabases(pref_service(), login_db_directory(),
                            GetMockBridgeWithBackendPresent());

  EXPECT_FALSE(PathExists(csv_path));
  EXPECT_FALSE(pref_service()->GetBoolean(kUpmAutoExportCsvNeedsDeletion));

  histogram_tester.ExpectUniqueSample(
      "PasswordManager.UPM.AutoExportedCsvStartupDeletionSuccess", true, 1);
}

TEST_F(PasswordManagerAndroidUtilTest, DoesntDeleteExportedCsvIfNotNeeded) {
  base::HistogramTester histogram_tester;

  pref_service()->SetBoolean(kUpmAutoExportCsvNeedsDeletion, false);

  // Creating the csv for testing.
  base::FilePath csv_path =
      login_db_directory().Append(password_manager::kExportedPasswordsFileName);

  base::WriteFile(csv_path, "Test content");

  EXPECT_TRUE(PathExists(csv_path));

  MaybeDeleteLoginDatabases(pref_service(), login_db_directory(),
                            GetMockBridgeWithBackendPresent());

  EXPECT_TRUE(PathExists(csv_path));
  EXPECT_FALSE(pref_service()->GetBoolean(kUpmAutoExportCsvNeedsDeletion));

  histogram_tester.ExpectTotalCount(
      "PasswordManager.UPM.AutoExportedCsvStartupDeletionSuccess", 0);
}

}  // namespace
}  // namespace password_manager_android_util
