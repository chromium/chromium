// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_list/search/search_features.h"
#include "chrome/browser/ash/app_list/search/test/app_list_search_test_helper.h"
#include "chrome/browser/ash/drive/drive_integration_service.h"
#include "chrome/browser/ash/drive/drivefs_test_support.h"

namespace app_list::test {

// This class contains additional logic to set up DriveFS and enable testing for
// Drive file search in the launcher.
class AppListDriveSearchBrowserTest : public AppListSearchBrowserTest,
                                      public testing::WithParamInterface<bool> {
 public:
  AppListDriveSearchBrowserTest() {
    if (GetParam()) {
      scoped_feature_list_.InitAndEnableFeature(
          search_features::kLauncherFuzzyMatchAcrossProviders);
    } else {
      scoped_feature_list_.InitAndDisableFeature(
          search_features::kLauncherFuzzyMatchAcrossProviders);
    }
  }

  void SetUpInProcessBrowserTestFixture() override {
    create_drive_integration_service_ = base::BindRepeating(
        &AppListDriveSearchBrowserTest::CreateDriveIntegrationService,
        base::Unretained(this));
    service_factory_for_test_ = std::make_unique<
        drive::DriveIntegrationServiceFactory::ScopedFactoryForTest>(
        &create_drive_integration_service_);
  }

 protected:
  virtual drive::DriveIntegrationService* CreateDriveIntegrationService(
      Profile* profile) {
    base::ScopedAllowBlockingForTesting allow_blocking;
    base::FilePath mount_path = profile->GetPath().Append("drivefs");
    fake_drivefs_helpers_[profile] =
        std::make_unique<drive::FakeDriveFsHelper>(profile, mount_path);
    auto* integration_service = new drive::DriveIntegrationService(
        profile, std::string(), mount_path,
        fake_drivefs_helpers_[profile]->CreateFakeDriveFsListenerFactory());
    return integration_service;
  }

 private:
  drive::DriveIntegrationServiceFactory::FactoryCallback
      create_drive_integration_service_;
  std::unique_ptr<drive::DriveIntegrationServiceFactory::ScopedFactoryForTest>
      service_factory_for_test_;
  std::map<Profile*, std::unique_ptr<drive::FakeDriveFsHelper>>
      fake_drivefs_helpers_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

INSTANTIATE_TEST_SUITE_P(FuzzyMatchForProviders,
                         AppListDriveSearchBrowserTest,
                         testing::Bool());

// Test that Drive files can be searched.
IN_PROC_BROWSER_TEST_P(AppListDriveSearchBrowserTest, FileSearch) {
  base::ScopedAllowBlockingForTesting allow_blocking;

  drive::DriveIntegrationService* drive_service =
      drive::DriveIntegrationServiceFactory::FindForProfile(GetProfile());
  ASSERT_TRUE(drive_service->IsMounted());
  base::FilePath mount_path = drive_service->GetMountPointPath();

  ASSERT_TRUE(base::WriteFile(mount_path.Append("my_file.gdoc"), "content"));
  ASSERT_TRUE(
      base::WriteFile(mount_path.Append("other_file.gsheet"), "content"));

  SearchAndWaitForProviders("my", {ResultType::kDriveSearch});

  const auto results = PublishedResultsForProvider(ResultType::kDriveSearch);
  ASSERT_EQ(results.size(), 1u);
  ASSERT_TRUE(results[0]);
  EXPECT_EQ(base::UTF16ToASCII(results[0]->title()), "my_file");
}

// Test that Drive folders can be searched.
IN_PROC_BROWSER_TEST_P(AppListDriveSearchBrowserTest, FolderSearch) {
  base::ScopedAllowBlockingForTesting allow_blocking;

  drive::DriveIntegrationService* drive_service =
      drive::DriveIntegrationServiceFactory::FindForProfile(GetProfile());
  ASSERT_TRUE(drive_service->IsMounted());
  base::FilePath mount_path = drive_service->GetMountPointPath();

  ASSERT_TRUE(base::CreateDirectory(mount_path.Append("my_folder")));
  ASSERT_TRUE(base::CreateDirectory(mount_path.Append("other_folder")));

  SearchAndWaitForProviders("my", {ResultType::kDriveSearch});

  const auto results = PublishedResultsForProvider(ResultType::kDriveSearch);
  ASSERT_EQ(results.size(), 1u);
  ASSERT_TRUE(results[0]);
  EXPECT_EQ(base::UTF16ToASCII(results[0]->title()), "my_folder");
}

// Test that files are ordered based on access time.
IN_PROC_BROWSER_TEST_P(AppListDriveSearchBrowserTest, ResultOrdering) {
  base::ScopedAllowBlockingForTesting allow_blocking;

  drive::DriveIntegrationService* drive_service =
      drive::DriveIntegrationServiceFactory::FindForProfile(GetProfile());
  ASSERT_TRUE(drive_service->IsMounted());
  base::FilePath mount_path = drive_service->GetMountPointPath();

  base::FilePath older = mount_path.Append("ranking_older.gdoc");
  base::FilePath newer = mount_path.Append("ranking_newer.gdoc");
  base::Time now = base::Time::Now();
  base::Time then = now - base::Seconds(10000);
  ASSERT_TRUE(base::WriteFile(older, "content"));
  ASSERT_TRUE(base::WriteFile(newer, "content"));
  ASSERT_TRUE(base::TouchFile(older, then, then));
  ASSERT_TRUE(base::TouchFile(newer, now, now));

  SearchAndWaitForProviders("ranking", {ResultType::kDriveSearch});

  auto results = PublishedResultsForProvider(ResultType::kDriveSearch);

  // Sort high-to-low by relevance.
  std::sort(results.begin(), results.end(),
            [](const ChromeSearchResult* a, const ChromeSearchResult* b) {
              return a->relevance() > b->relevance();
            });

  ASSERT_EQ(results.size(), 2u);
  EXPECT_EQ(base::UTF16ToASCII(results[0]->title()), "ranking_newer");
  EXPECT_EQ(base::UTF16ToASCII(results[1]->title()), "ranking_older");
}

}  // namespace app_list::test
