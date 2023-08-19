// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_list/search/files/file_search_provider.h"

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "chrome/browser/ash/app_list/search/files/file_result.h"
#include "chrome/browser/ash/app_list/search/search_features.h"
#include "chrome/browser/ash/app_list/search/test/test_search_controller.h"
#include "chrome/browser/ash/file_manager/path_util.h"
#include "chrome/browser/ash/file_manager/trash_common_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/test/base/testing_profile.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_task_environment.h"
#include "storage/browser/file_system/external_mount_points.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace app_list::test {
namespace {

using ::testing::ElementsAre;
using ::testing::UnorderedElementsAre;

MATCHER_P(Title, title, "") {
  return base::UTF16ToUTF8(arg->title()) == title;
}

}  // namespace

class FileSearchProviderTest : public testing::Test,
                               public testing::WithParamInterface<bool> {
 public:
  FileSearchProviderTest() {
    if (GetParam()) {
      scoped_feature_list_.InitAndEnableFeature(
          search_features::kLauncherFuzzyMatchAcrossProviders);
    } else {
      scoped_feature_list_.InitAndDisableFeature(
          search_features::kLauncherFuzzyMatchAcrossProviders);
    }
  }

 protected:
  void SetUp() override {
    profile_ = std::make_unique<TestingProfile>();
    search_controller_ = std::make_unique<TestSearchController>();
    provider_ = std::make_unique<FileSearchProvider>(profile_.get());

    provider_->set_controller(search_controller_.get());

    ASSERT_TRUE(scoped_temp_dir_.CreateUniqueTempDir());
    provider_->SetRootPathForTesting(scoped_temp_dir_.GetPath());

    Wait();
  }

  base::FilePath Path(const std::string& filename) {
    return scoped_temp_dir_.GetPath().Append(filename);
  }

  void WriteFile(const std::string& filename) {
    ASSERT_TRUE(base::WriteFile(Path(filename), "abcd"));
    ASSERT_TRUE(base::PathExists(Path(filename)));
    Wait();
  }

  void CreateDirectory(const std::string& directory_name) {
    ASSERT_TRUE(base::CreateDirectory(Path(directory_name)));
    ASSERT_TRUE(base::PathExists(Path(directory_name)));
    Wait();
  }

  const SearchProvider::Results& LastResults() {
    return search_controller_->last_results();
  }

  void Wait() { task_environment_.RunUntilIdle(); }

  content::BrowserTaskEnvironment task_environment_;
  base::test::ScopedFeatureList scoped_feature_list_;

  std::unique_ptr<Profile> profile_;
  std::unique_ptr<TestSearchController> search_controller_;
  std::unique_ptr<FileSearchProvider> provider_;
  base::ScopedTempDir scoped_temp_dir_;
};

INSTANTIATE_TEST_SUITE_P(FuzzyMatchForProviders,
                         FileSearchProviderTest,
                         testing::Bool());

TEST_P(FileSearchProviderTest, SearchResultsMatchQuery) {
  WriteFile("file_1.txt");
  WriteFile("no_match.png");
  WriteFile("my_file_2.png");

  provider_->Start(u"file");
  Wait();

  EXPECT_THAT(LastResults(), UnorderedElementsAre(Title("file_1.txt"),
                                                  Title("my_file_2.png")));
}

TEST_P(FileSearchProviderTest, SearchIsCaseInsensitive) {
  WriteFile("FILE_1.png");
  WriteFile("FiLe_2.Png");

  provider_->Start(u"fIle");
  Wait();

  EXPECT_THAT(LastResults(),
              UnorderedElementsAre(Title("FILE_1.png"), Title("FiLe_2.Png")));
}

TEST_P(FileSearchProviderTest, SearchIsAccentAndCaseInsensitive) {
  WriteFile("FĪLE_1.png");
  WriteFile("FīLe_2.Png");

  provider_->Start(u"fīle");
  Wait();

  EXPECT_THAT(LastResults(),
              UnorderedElementsAre(Title("FĪLE_1.png"), Title("FīLe_2.Png")));
}

TEST_P(FileSearchProviderTest, SearchIsAccentInsensitive) {
  WriteFile("FILE_1.png");
  WriteFile("FiLe_2.Png");
  WriteFile("FĪLE_3.png");
  WriteFile("FīLe_4.Png");
  WriteFile("FiLË_5.png");
  WriteFile("FILê_6.Png");

  provider_->Start(u"file");
  Wait();

  EXPECT_THAT(LastResults(),
              UnorderedElementsAre(Title("FILE_1.png"), Title("FiLe_2.Png"),
                                   Title("FĪLE_3.png"), Title("FīLe_4.Png"),
                                   Title("FiLË_5.png"), Title("FILê_6.Png")));
}

TEST_P(FileSearchProviderTest, SearchIsAccentHonored) {
  WriteFile("FĪLE_1.png");
  WriteFile("FīLe_2.Png");
  WriteFile("file_3.png");

  provider_->Start(u"fīle");
  Wait();

  EXPECT_THAT(LastResults(),
              UnorderedElementsAre(Title("FĪLE_1.png"), Title("FīLe_2.Png")));
}

TEST_P(FileSearchProviderTest, SearchDirectories) {
  CreateDirectory("my_folder");

  provider_->Start(u"my_folder");
  Wait();

  EXPECT_THAT(LastResults(), UnorderedElementsAre(Title("my_folder")));
}

TEST_P(FileSearchProviderTest, ResultMetadataTest) {
  WriteFile("file.txt");

  provider_->Start(u"file");
  Wait();

  ASSERT_TRUE(LastResults().size() == 1u);
  const auto& result = LastResults()[0];
  EXPECT_EQ(result->result_type(), ash::AppListSearchResultType::kFileSearch);
  EXPECT_EQ(result->display_type(), ash::SearchResultDisplayType::kList);
}

TEST_P(FileSearchProviderTest, RecentlyAccessedFilesHaveHigherRelevance) {
  WriteFile("file.txt");
  WriteFile("file.png");
  WriteFile("file.pdf");

  // Set the access times of all files to be different.
  const base::Time time = base::Time::Now();
  const base::Time earlier_time = time - base::Days(5);
  const base::Time earliest_time = time - base::Days(10);
  TouchFile(Path("file.txt"), time, time);
  TouchFile(Path("file.png"), earliest_time, time);
  TouchFile(Path("file.pdf"), earlier_time, time);

  provider_->Start(u"file");
  Wait();

  ASSERT_TRUE(LastResults().size() == 3u);

  // Sort the results by descending relevance.
  std::vector<ChromeSearchResult*> results;
  for (const auto& result : LastResults()) {
    results.push_back(result.get());
  }
  std::sort(results.begin(), results.end(),
            [](const ChromeSearchResult* a, const ChromeSearchResult* b) {
              return a->relevance() > b->relevance();
            });
  ASSERT_TRUE(results[0]->relevance() > results[1]->relevance());
  ASSERT_TRUE(results[1]->relevance() > results[2]->relevance());

  // Most recently accessed files should be at the front.
  EXPECT_THAT(results, ElementsAre(Title("file.txt"), Title("file.pdf"),
                                   Title("file.png")));
}

TEST_P(FileSearchProviderTest, HighScoringFilesHaveScoreInRightRange) {
  // Make two identically named files with different access times.
  const base::Time time = base::Time::Now();
  const base::Time earlier_time = time - base::Days(5);
  CreateDirectory("dir");
  WriteFile("dir/file");
  WriteFile("file");
  TouchFile(Path("dir/file"), time, time);
  TouchFile(Path("file"), earlier_time, time);

  // Match them perfectly, so both score 1.0.
  provider_->Start(u"file");
  Wait();

  ASSERT_EQ(LastResults().size(), 2u);

  // Sort the results by descending relevance.
  std::vector<ChromeSearchResult*> results;
  for (const auto& result : LastResults()) {
    results.push_back(result.get());
  }
  std::sort(results.begin(), results.end(),
            [](const ChromeSearchResult* a, const ChromeSearchResult* b) {
              return a->relevance() > b->relevance();
            });
  // The scores should be properly in order and not exceed 1.0.
  EXPECT_GT(results[0]->relevance(), results[1]->relevance());
  EXPECT_LE(results[0]->relevance(), 1.0);
}

TEST_P(FileSearchProviderTest, ResultsNotReturnedAfterClearingSearch) {
  // Make two identically named files with different access times.
  const base::Time time = base::Time::Now();
  const base::Time earlier_time = time - base::Days(5);
  CreateDirectory("dir");
  WriteFile("file");
  TouchFile(Path("file"), earlier_time, time);

  // Start search, and cancel it before the provider has had a chance to return
  // results.
  provider_->Start(u"file");

  provider_->StopQuery();
  Wait();

  EXPECT_EQ(LastResults().size(), 0u);
}

class FileSearchProviderTrashTest : public FileSearchProviderTest {
 public:
  FileSearchProviderTrashTest() = default;

  FileSearchProviderTrashTest(const FileSearchProviderTrashTest&) = delete;
  FileSearchProviderTrashTest& operator=(const FileSearchProviderTrashTest&) =
      delete;

  void SetUp() override {
    FileSearchProviderTest::SetUp();

    // Ensure the My files and Downloads mount points are appropriately mocked
    // to allow the trash locations to be parented at the test directory.
    storage::ExternalMountPoints::GetSystemInstance()->RegisterFileSystem(
        file_manager::util::GetDownloadsMountPointName(profile_.get()),
        storage::kFileSystemTypeLocal, storage::FileSystemMountOption(),
        scoped_temp_dir_.GetPath());

    ToggleTrash(true);
  }

  void ToggleTrash(bool enabled) {
    profile_->GetPrefs()->SetBoolean(ash::prefs::kFilesAppTrashEnabled,
                                     enabled);
  }
};

INSTANTIATE_TEST_SUITE_P(FuzzyMatchForProviders,
                         FileSearchProviderTrashTest,
                         testing::Values(true));

TEST_P(FileSearchProviderTrashTest, FilesInTrashAreIgnored) {
  using file_manager::trash::kTrashFolderName;
  CreateDirectory(kTrashFolderName);
  WriteFile("file");
  WriteFile(base::FilePath(kTrashFolderName).Append("trashed_file").value());

  provider_->Start(u"file");
  Wait();

  EXPECT_THAT(LastResults(), UnorderedElementsAre(Title("file")));
}

TEST_P(FileSearchProviderTrashTest, FilesInTrashArentIgnoredIfTrashDisabled) {
  using file_manager::trash::kTrashFolderName;

  ToggleTrash(false);

  CreateDirectory(kTrashFolderName);
  WriteFile("file");
  WriteFile(base::FilePath(kTrashFolderName).Append("trashed_file").value());

  provider_->Start(u"file");
  Wait();

  EXPECT_THAT(LastResults(),
              UnorderedElementsAre(Title("file"), Title("trashed_file")));
}

}  // namespace app_list::test
