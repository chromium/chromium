// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/dips/dips_cleanup_service.h"

#include "base/files/file_util.h"
#include "base/memory/raw_ptr.h"
#include "base/test/test_file_util.h"
#include "chrome/browser/dips/dips_service_impl.h"
#include "chrome/browser/dips/dips_test_utils.h"
#include "chrome/browser/dips/dips_utils.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

class DIPSCleanupServiceTest : public testing::Test {
 protected:
  void WaitOnStorage(DIPSServiceImpl* service) {
    service->storage()->FlushPostedTasksForTesting();
  }

 private:
  content::BrowserTaskEnvironment task_environment_;
};

TEST_F(DIPSCleanupServiceTest, DontCreateServiceIfFeatureEnabled) {
  ScopedInitDIPSFeature init_dips(true);

  TestingProfile profile;
  EXPECT_EQ(DIPSCleanupService::Get(&profile), nullptr);
}

TEST_F(DIPSCleanupServiceTest, CreateServiceIfFeatureDisabled) {
  ScopedInitDIPSFeature init_dips(false);

  TestingProfile profile;
  EXPECT_NE(DIPSCleanupService::Get(&profile), nullptr);
}

TEST_F(DIPSCleanupServiceTest, DeleteDbFilesIfFeatureDisabled) {
  base::FilePath data_path = base::CreateUniqueTempDirectoryScopedToTest();

  {
    // Ensure the DIPS feature is enabled and the database is set to be
    // persisted.
    ScopedInitDIPSFeature enable_dips(true, {{"persist_database", "true"}});

    std::unique_ptr<TestingProfile> profile =
        TestingProfile::Builder().SetPath(data_path).Build();

    DIPSServiceImpl* dips_service = DIPSServiceImpl::Get(profile.get());
    ASSERT_NE(dips_service, nullptr);
    ASSERT_EQ(DIPSCleanupService::Get(profile.get()), nullptr);

    WaitOnStorage(dips_service);
    dips_service->WaitForFileDeletionCompleteForTesting();

    ASSERT_TRUE(base::PathExists(GetDIPSFilePath(profile.get())));
  }

  // This should be equivalent to the DIPS db filepath in |data_path|.
  ASSERT_TRUE(base::PathExists(data_path.Append(kDIPSFilename)));

  {
    // Disable the DIPS feature.
    ScopedInitDIPSFeature disable_dips(false);

    std::unique_ptr<TestingProfile> profile =
        TestingProfile::Builder().SetPath(data_path).Build();

    DIPSCleanupService* cleanup_service =
        DIPSCleanupService::Get(profile.get());
    ASSERT_NE(cleanup_service, nullptr);
    ASSERT_EQ(DIPSServiceImpl::Get(profile.get()), nullptr);

    cleanup_service->WaitOnCleanupForTesting();
    EXPECT_FALSE(base::PathExists(GetDIPSFilePath(profile.get())));
  }
}
