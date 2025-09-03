// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/android/password_manager_android_util.h"

#include <limits>
#include <memory>

#include "base/android/android_info.h"
#include "base/android/device_info.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/to_string.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/test_file_util.h"
#include "build/build_config.h"
#include "chrome/browser/password_manager/android/mock_password_manager_util_bridge.h"
#include "components/password_manager/core/browser/password_manager_constants.h"
#include "components/password_manager/core/browser/split_stores_and_local_upm.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using password_manager::GetSplitStoresUpmMinVersion;
using password_manager::prefs::kUpmAutoExportCsvNeedsDeletion;
using testing::Return;

namespace password_manager_android_util {
namespace {

class PasswordManagerAndroidUtilTest : public testing::Test {
 public:
  PasswordManagerAndroidUtilTest() {
    pref_service_.registry()->RegisterBooleanPref(
        password_manager::prefs::kUpmAutoExportCsvNeedsDeletion, false);
    base::WriteFile(login_db_directory_.Append(
                        password_manager::kLoginDataForProfileFileName),
                    "");

    // Most tests check the modern GmsCore case.
    base::android::device_info::set_gms_version_code_for_test(
        base::NumberToString(GetSplitStoresUpmMinVersion()));
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
  base::android::device_info::set_gms_version_code_for_test(
      base::NumberToString(GetSplitStoresUpmMinVersion()));

  std::unique_ptr<MockPasswordManagerUtilBridge> mock_util_bridge =
      std::make_unique<MockPasswordManagerUtilBridge>();
  EXPECT_CALL(*mock_util_bridge, IsInternalBackendPresent)
      .WillOnce(Return(false));
  EXPECT_FALSE(IsPasswordManagerAvailable(std::move(mock_util_bridge)));
}

TEST_F(PasswordManagerAndroidUtilTest,
       PasswordManagerNotAvailableGmsVersionTooLow) {
  std::unique_ptr<MockPasswordManagerUtilBridge> mock_util_bridge =
      std::make_unique<MockPasswordManagerUtilBridge>();
  EXPECT_CALL(*mock_util_bridge, IsInternalBackendPresent)
      .WillOnce(Return(true));

  // Set a GMS Core version that is lower than the min required version.
  base::android::device_info::set_gms_version_code_for_test(
      base::NumberToString(GetSplitStoresUpmMinVersion() - 1));

  EXPECT_FALSE(IsPasswordManagerAvailable(std::move(mock_util_bridge)));
}

TEST_F(PasswordManagerAndroidUtilTest, PasswordManagerAvailable) {
  std::unique_ptr<MockPasswordManagerUtilBridge> mock_util_bridge =
      std::make_unique<MockPasswordManagerUtilBridge>();
  EXPECT_CALL(*mock_util_bridge, IsInternalBackendPresent)
      .WillOnce(Return(true));

  base::android::device_info::set_gms_version_code_for_test(
      base::NumberToString(GetSplitStoresUpmMinVersion()));

  EXPECT_TRUE(IsPasswordManagerAvailable(std::move(mock_util_bridge)));
}

TEST_F(PasswordManagerAndroidUtilTest, DeletesLoginDataFiles) {
  base::HistogramTester histogram_tester;
  const char kRemovalStatusProfileMetric[] =
      "PasswordManager.ProfileLoginData.RemovalStatus";
  const char kRemovalStatusAccountMetric[] =
      "PasswordManager.AccountLoginData.RemovalStatus";

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
      login_db_directory().Append(kExportedPasswordsFileName);

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
      login_db_directory().Append(kExportedPasswordsFileName);

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
