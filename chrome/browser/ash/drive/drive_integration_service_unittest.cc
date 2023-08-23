// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/drive/drive_integration_service.h"

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace drive {

class DriveIntegrationServiceTest : public testing::Test {
 public:
  DriveIntegrationServiceTest()
      : profile_manager_(TestingBrowserProcess::GetGlobal()) {}

  void SetUp() override { ASSERT_TRUE(profile_manager_.SetUp()); }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  // DriveIntegrationService depends on DriveNotificationManager which depends
  // on InvalidationService. On Chrome OS, the InvalidationServiceFactory
  // uses ash::ProfileHelper, which needs the ProfileManager or a
  // TestProfileManager to be running.
  TestingProfileManager profile_manager_;
};

// This test is flaky on chromeos.
// https://crbug.com/943376
TEST_F(DriveIntegrationServiceTest, DISABLED_ServiceInstanceIdentity) {
  TestingProfile* user1 = profile_manager_.CreateTestingProfile("user1");

  // Integration Service is created as a profile keyed service.
  EXPECT_TRUE(DriveIntegrationServiceFactory::GetForProfile(user1));

  // Shares the same instance with the incognito mode profile.
  Profile* user1_incognito =
      user1->GetPrimaryOTRProfile(/*create_if_needed=*/true);
  EXPECT_EQ(DriveIntegrationServiceFactory::GetForProfile(user1),
            DriveIntegrationServiceFactory::GetForProfile(user1_incognito));

  // For different profiles, different services are running.
  TestingProfile* user2 = profile_manager_.CreateTestingProfile("user2");
  EXPECT_NE(DriveIntegrationServiceFactory::GetForProfile(user1),
            DriveIntegrationServiceFactory::GetForProfile(user2));
}

TEST_F(DriveIntegrationServiceTest, EnsureDirectoryExists) {
  base::ScopedTempDir tmp_dir;
  ASSERT_TRUE(tmp_dir.CreateUniqueTempDir());

  const base::FilePath data_dir = tmp_dir.GetPath().Append("a/b/c");
  EXPECT_FALSE(base::DirectoryExists(data_dir));

  // First time, the data dir should be created.
  EXPECT_EQ(DriveIntegrationService::EnsureDirectoryExists(data_dir),
            DriveIntegrationService::DirResult::kCreated);
  EXPECT_TRUE(base::DirectoryExists(data_dir));

  // Second time, the data dir should already exist.
  EXPECT_EQ(DriveIntegrationService::EnsureDirectoryExists(data_dir),
            DriveIntegrationService::DirResult::kExisting);
  EXPECT_TRUE(base::DirectoryExists(data_dir));

  // Remove the data dir, and replace it with a file.
  EXPECT_TRUE(base::DeleteFile(data_dir));
  EXPECT_FALSE(base::DirectoryExists(data_dir));
  EXPECT_TRUE(base::WriteFile(data_dir, "Whatever"));

  // Trying to create the data dir should now fail.
  EXPECT_FALSE(base::DirectoryExists(data_dir));
  EXPECT_EQ(DriveIntegrationService::EnsureDirectoryExists(data_dir),
            DriveIntegrationService::DirResult::kError);
  EXPECT_FALSE(base::DirectoryExists(data_dir));
}

}  // namespace drive
