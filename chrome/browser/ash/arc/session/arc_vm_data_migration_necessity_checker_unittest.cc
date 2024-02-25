// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/session/arc_vm_data_migration_necessity_checker.h"

#include <optional>

#include "ash/components/arc/arc_features.h"
#include "ash/components/arc/arc_prefs.h"
#include "ash/components/arc/arc_util.h"
#include "ash/components/arc/session/arc_vm_client_adapter.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/ash/components/dbus/arc/arcvm_data_migrator_client.h"
#include "chromeos/ash/components/dbus/arc/fake_arcvm_data_migrator_client.h"
#include "chromeos/ash/components/dbus/upstart/fake_upstart_client.h"
#include "chromeos/ash/components/dbus/upstart/upstart_client.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace arc {
namespace {

class ArcVmDataMigrationNecessityCheckerTest : public testing::Test {
 public:
  ArcVmDataMigrationNecessityCheckerTest() = default;
  ~ArcVmDataMigrationNecessityCheckerTest() override = default;

  void SetUp() override {
    ash::UpstartClient::InitializeFake();
    ash::ArcVmDataMigratorClient::InitializeFake();

    profile_ = std::make_unique<TestingProfile>();
    checker_ =
        std::make_unique<ArcVmDataMigrationNecessityChecker>(profile_.get());
  }

  void TearDown() override {
    checker_.reset();
    profile_.reset();

    ash::ArcVmDataMigratorClient::Shutdown();
    ash::UpstartClient::Shutdown();
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<ArcVmDataMigrationNecessityChecker> checker_;
};

TEST_F(ArcVmDataMigrationNecessityCheckerTest, HasDataToMigrate) {
  std::optional<bool> migration_needed = false;
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(kEnableArcVmDataMigration);
  ash::FakeArcVmDataMigratorClient::Get()->set_has_data_to_migrate(true);
  checker_->Check(base::BindLambdaForTesting(
      [&migration_needed](std::optional<bool> result) {
        migration_needed = result;
      }));
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(migration_needed.has_value());
  EXPECT_TRUE(migration_needed.value());
}

TEST_F(ArcVmDataMigrationNecessityCheckerTest, HasNoDataToMigrate) {
  std::optional<bool> migration_needed = true;
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(kEnableArcVmDataMigration);
  ash::FakeArcVmDataMigratorClient::Get()->set_has_data_to_migrate(false);
  checker_->Check(base::BindLambdaForTesting(
      [&migration_needed](std::optional<bool> result) {
        migration_needed = result;
      }));
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(migration_needed.has_value());
  EXPECT_FALSE(migration_needed.value());
}

TEST_F(ArcVmDataMigrationNecessityCheckerTest, ForceVirtioBlkForData) {
  std::optional<bool> migration_needed = true;
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      {kEnableArcVmDataMigration, kEnableVirtioBlkForData}, {});
  ash::FakeArcVmDataMigratorClient::Get()->set_has_data_to_migrate(true);
  checker_->Check(base::BindLambdaForTesting(
      [&migration_needed](std::optional<bool> result) {
        migration_needed = result;
      }));
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(migration_needed.has_value());
  EXPECT_FALSE(migration_needed.value());
}

TEST_F(ArcVmDataMigrationNecessityCheckerTest, MigrationFinished) {
  std::optional<bool> migration_needed = true;
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(kEnableArcVmDataMigration);
  SetArcVmDataMigrationStatus(profile_->GetPrefs(),
                              ArcVmDataMigrationStatus::kFinished);
  ash::FakeArcVmDataMigratorClient::Get()->set_has_data_to_migrate(true);
  checker_->Check(base::BindLambdaForTesting(
      [&migration_needed](std::optional<bool> result) {
        migration_needed = result;
      }));
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(migration_needed.has_value());
  EXPECT_FALSE(migration_needed.value());
}

TEST_F(ArcVmDataMigrationNecessityCheckerTest, StartJobFailed) {
  std::optional<bool> migration_needed = true;
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(kEnableArcVmDataMigration);
  ash::FakeArcVmDataMigratorClient::Get()->set_has_data_to_migrate(true);
  ash::FakeUpstartClient::Get()->set_start_job_cb(base::BindLambdaForTesting(
      [](const std::string& job_name, const std::vector<std::string>& env) {
        return ash::FakeUpstartClient::StartJobResult(
            job_name != kArcVmDataMigratorJobName);
      }));
  checker_->Check(base::BindLambdaForTesting(
      [&migration_needed](std::optional<bool> result) {
        migration_needed = result;
      }));
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(migration_needed.has_value());
}

TEST_F(ArcVmDataMigrationNecessityCheckerTest, HasDataToMigrateFailed) {
  std::optional<bool> migration_needed = true;
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(kEnableArcVmDataMigration);
  ash::FakeArcVmDataMigratorClient::Get()->set_has_data_to_migrate(
      std::nullopt);
  checker_->Check(base::BindLambdaForTesting(
      [&migration_needed](std::optional<bool> result) {
        migration_needed = result;
      }));
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(migration_needed.has_value());
}

}  // namespace
}  // namespace arc
