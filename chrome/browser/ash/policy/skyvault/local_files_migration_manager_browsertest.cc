// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/skyvault/local_files_migration_manager.h"

#include <memory>

#include "base/test/scoped_feature_list.h"
#include "base/test/scoped_mock_time_message_loop_task_runner.h"
#include "base/time/time.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/download/download_dir_util.h"
#include "chrome/browser/policy/policy_test_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace policy::local_user_files {

namespace {

class MockMigrationObserver : public LocalFilesMigrationManager::Observer {
 public:
  MockMigrationObserver() = default;
  ~MockMigrationObserver() = default;

  MOCK_METHOD(void, OnMigrationSucceeded, (), (override));
};

}  // namespace

class LocalFilesMigrationManagerTest : public policy::PolicyTest {
 public:
  LocalFilesMigrationManagerTest() {
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{features::kSkyVault, features::kSkyVaultV2},
        /*disabled_features=*/{});
  }
  ~LocalFilesMigrationManagerTest() override = default;

 protected:
  void SetMigrationPolicies(bool local_user_files_allowed,
                            bool local_user_files_migration_enabled) {
    policy::PolicyMap policies;
    policy::PolicyTest::SetPolicy(&policies,
                                  policy::key::kLocalUserFilesAllowed,
                                  base::Value(local_user_files_allowed));
    policy::PolicyTest::SetPolicy(
        &policies, policy::key::kLocalUserFilesMigrationEnabled,
        base::Value(local_user_files_migration_enabled));
    provider_.UpdateChromePolicy(policies);
  }

  base::test::ScopedFeatureList scoped_feature_list_;
};

class LocalFilesMigrationManagerLocationTest
    : public LocalFilesMigrationManagerTest,
      public ::testing::WithParamInterface</*default_location*/ std::string> {
 public:
  static std::string ParamToName(const testing::TestParamInfo<ParamType> info) {
    return info.param;
  }

  LocalFilesMigrationManagerLocationTest() = default;
  ~LocalFilesMigrationManagerLocationTest() = default;

 protected:
  std::string DefaultLocation() { return GetParam(); }
};

IN_PROC_BROWSER_TEST_P(LocalFilesMigrationManagerLocationTest,
                       MigrationNotifiesObservers) {
  base::ScopedMockTimeMessageLoopTaskRunner task_runner;
  MockMigrationObserver observer;
  EXPECT_CALL(observer, OnMigrationSucceeded).Times(1);
  LocalFilesMigrationManager manager;
  manager.AddObserver(&observer);

  browser()->profile()->GetPrefs()->SetString(prefs::kFilesAppDefaultLocation,
                                              GetParam());
  // Changing the LocalUserFilesAllowed policy should trigger the migration and
  // update, after the timeout.
  SetMigrationPolicies(/*local_user_files_allowed=*/false,
                       /*local_user_files_migration_enabled=*/true);
  task_runner->FastForwardBy(base::TimeDelta(base::Hours(24)));
}

IN_PROC_BROWSER_TEST_P(LocalFilesMigrationManagerLocationTest,
                       NoMigrationIfLocalFilesAllowed) {
  MockMigrationObserver observer;
  EXPECT_CALL(observer, OnMigrationSucceeded).Times(0);
  LocalFilesMigrationManager manager;
  manager.AddObserver(&observer);

  browser()->profile()->GetPrefs()->SetString(prefs::kFilesAppDefaultLocation,
                                              DefaultLocation());
  SetMigrationPolicies(/*local_user_files_allowed=*/true,
                       /*local_user_files_migration_enabled=*/true);
}

IN_PROC_BROWSER_TEST_P(LocalFilesMigrationManagerLocationTest,
                       NoMigrationIfDisabled) {
  MockMigrationObserver observer;
  EXPECT_CALL(observer, OnMigrationSucceeded).Times(0);
  LocalFilesMigrationManager manager;
  manager.AddObserver(&observer);

  browser()->profile()->GetPrefs()->SetString(prefs::kFilesAppDefaultLocation,
                                              DefaultLocation());
  SetMigrationPolicies(/*local_user_files_allowed=*/false,
                       /*local_user_files_migration_enabled=*/false);
}

IN_PROC_BROWSER_TEST_F(LocalFilesMigrationManagerTest,
                       NoMigrationIfNoDefaultLocation) {
  MockMigrationObserver observer;
  EXPECT_CALL(observer, OnMigrationSucceeded).Times(0);
  LocalFilesMigrationManager manager;
  manager.AddObserver(&observer);

  browser()->profile()->GetPrefs()->SetString(prefs::kFilesAppDefaultLocation,
                                              "");
  SetMigrationPolicies(/*local_user_files_allowed=*/false,
                       /*local_user_files_migration_enabled=*/true);
}

INSTANTIATE_TEST_SUITE_P(
    LocalUserFiles,
    LocalFilesMigrationManagerLocationTest,
    ::testing::Values(download_dir_util::kLocationGoogleDrive,
                      download_dir_util::kLocationOneDrive),
    LocalFilesMigrationManagerLocationTest::ParamToName);

}  // namespace policy::local_user_files
