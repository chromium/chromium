// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_list/search/files/zero_state_file_provider.h"

#include <string>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/raw_ptr.h"
#include "base/test/scoped_running_on_chromeos.h"
#include "chrome/browser/ash/app_list/search/test/test_search_controller.h"
#include "chrome/browser/ash/file_manager/path_util.h"
#include "chrome/browser/ash/file_suggest/file_suggest_keyed_service_factory.h"
#include "chrome/browser/ash/file_suggest/local_file_suggestion_provider.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace app_list::test {
namespace {

using ::file_manager::file_tasks::FileTasksObserver;
using ::testing::UnorderedElementsAre;

MATCHER_P(Title, title, "") {
  return arg->title() == title;
}

}  // namespace

class ZeroStateFileProviderTest : public testing::Test {
 protected:
  ZeroStateFileProviderTest() = default;
  ~ZeroStateFileProviderTest() override = default;

  void SetUp() override {
    testing_profile_manager_ = std::make_unique<TestingProfileManager>(
        TestingBrowserProcess::GetGlobal());
    EXPECT_TRUE(testing_profile_manager_->SetUp());
    profile_ = testing_profile_manager_->CreateTestingProfile(
        "primary_profile@test", {});

    // The downloads directory depends on whether it is inside or outside
    // chromeos. So this needs to be in scope before |provider_| and
    // |downloads_folder_|.
    base::test::ScopedRunningOnChromeOS running_on_chromeos;

    auto provider = std::make_unique<ZeroStateFileProvider>(profile_);
    provider_ = provider.get();
    search_controller_.AddProvider(std::move(provider));

    downloads_folder_ =
        file_manager::util::GetDownloadsFolderForProfile(profile_);
    ASSERT_TRUE(base::CreateDirectory(downloads_folder_));

    Wait();
  }

  base::FilePath Path(const std::string& filename) {
    return profile_->GetPath().AppendASCII(filename);
  }

  base::FilePath DownloadsPath(const std::string& filename) {
    return downloads_folder_.AppendASCII(filename);
  }

  void WriteFile(const base::FilePath& path) {
    CHECK(base::WriteFile(path, "abcd"));
    CHECK(base::PathExists(path));
    Wait();
  }

  FileTasksObserver::FileOpenEvent OpenEvent(const base::FilePath& path) {
    FileTasksObserver::FileOpenEvent e;
    e.path = path;
    e.open_type = FileTasksObserver::OpenType::kOpen;
    return e;
  }

  void StartSearch(const std::u16string& query) {
    search_controller_.StartSearch(query);
  }

  void StartZeroStateSearch() {
    search_controller_.StartZeroState(base::DoNothing(), base::TimeDelta());
  }

  const SearchProvider::Results& LastResults() {
    return search_controller_.last_results();
  }

  void Wait() { task_environment_.RunUntilIdle(); }

  content::BrowserTaskEnvironment task_environment_;

  std::unique_ptr<TestingProfileManager> testing_profile_manager_;
  raw_ptr<TestingProfile> profile_ = nullptr;
  base::ScopedTempDir temp_dir_;
  base::FilePath downloads_folder_;

  TestSearchController search_controller_;
  raw_ptr<ZeroStateFileProvider> provider_ = nullptr;
};

TEST_F(ZeroStateFileProviderTest, NoResultsWithQuery) {
  StartSearch(u"query");
  Wait();
  EXPECT_TRUE(LastResults().empty());
}

TEST_F(ZeroStateFileProviderTest, FilterScreenshots) {
  WriteFile(Path("ScreenshotNonDownload.png"));
  WriteFile(DownloadsPath("ScreenshotNonPng.jpg"));
  WriteFile(DownloadsPath("NotScreenshot.png"));
  WriteFile(DownloadsPath("Screenshot123.png"));

  auto* keyed_service =
      ash::FileSuggestKeyedServiceFactory::GetInstance()->GetService(profile_);
  keyed_service->local_file_suggestion_provider_for_test()->OnFilesOpened(
      {OpenEvent(Path("ScreenshotNonDownload.png")),
       OpenEvent(DownloadsPath("ScreenshotNonPng.jpg")),
       OpenEvent(DownloadsPath("NotScreenshot.png")),
       OpenEvent(DownloadsPath("Screenshot123.png"))});

  StartZeroStateSearch();
  Wait();

  // Screenshot123 matches the criteria for a screenshot and should be filtered
  // out.
  EXPECT_THAT(LastResults(),
              UnorderedElementsAre(Title(u"ScreenshotNonDownload.png"),
                                   Title(u"ScreenshotNonPng.jpg"),
                                   Title(u"NotScreenshot.png")));
}

}  // namespace app_list::test
