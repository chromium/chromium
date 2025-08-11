// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/first_run/first_run.h"

#include <utility>

#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/files/file_util.h"
#include "base/json/json_reader.h"
#include "base/path_service.h"
#include "base/task/current_thread.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_path_override.h"
#include "base/test/test_future.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/first_run/first_run_internal.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/installer/util/initial_preferences.h"
#include "chrome/test/base/testing_profile.h"
#include "components/bookmarks/browser/base_bookmark_model_observer.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_node.h"
#include "components/startup_metric_utils/browser/startup_metric_utils.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

using first_run::internal::FirstRunImportBookmarksResult;

namespace first_run {

namespace {

base::FilePath GetTestDataPath(const std::string& test_name) {
  return base::PathService::CheckedGet(chrome::DIR_TEST_DATA)
      .AppendASCII("first_run")
      .AppendASCII(test_name);
}

base::FilePath GetSentinelFilePath() {
  return base::PathService::CheckedGet(chrome::DIR_USER_DATA)
      .Append(chrome::kFirstRunSentinel);
}

base::Value::Dict ParseJSONIfValid(std::string_view json) {
  std::optional<base::Value::Dict> parsed_json =
      base::JSONReader::ReadDict(json);
  if (!parsed_json.has_value()) {
    ADD_FAILURE() << "JSON parsing failed";
  }
  return *std::move(parsed_json);
}

// An observer that waits for the extensive changes on the bookmark model to
// end, which signals the completion of the bookmark import.
class BookmarkImportObserver final
    : public bookmarks::BaseBookmarkModelObserver {
 public:
  explicit BookmarkImportObserver(base::OnceClosure quit_closure)
      : quit_closure_(std::move(quit_closure)) {}

  void ExtensiveBookmarkChangesEnded() override {
    std::move(quit_closure_).Run();
  }

  void BookmarkModelChanged() override {}

  void BookmarkModelBeingDeleted() override {
    if (quit_closure_) {
      ADD_FAILURE() << "Bookmark model deleted before import completed.";
      std::move(quit_closure_).Run();
    }
  }

 private:
  base::OnceClosure quit_closure_;
};

}  // namespace

class FirstRunTest : public testing::Test {
 protected:
  void TearDown() override {
    first_run::ResetCachedSentinelDataForTesting();
    Test::TearDown();
  }

 private:
  base::ScopedPathOverride user_data_dir_override_{chrome::DIR_USER_DATA};
};

TEST_F(FirstRunTest, SetupInitialPrefsFromInstallPrefs_NoVariationsSeed) {
  installer::InitialPreferences install_prefs("{ }");
  EXPECT_TRUE(install_prefs.initial_dictionary().empty());

  EXPECT_TRUE(install_prefs.GetCompressedVariationsSeed().empty());
  EXPECT_TRUE(install_prefs.GetVariationsSeedSignature().empty());
}

TEST_F(FirstRunTest,
       SetupInitialPrefsFromInstallPrefs_VariationsSeedSignature) {
  installer::InitialPreferences install_prefs(
      "{\"variations_compressed_seed\":\"xyz\","
      " \"variations_seed_signature\":\"abc\"}");
  EXPECT_EQ(2U, install_prefs.initial_dictionary().size());

  EXPECT_EQ("xyz", install_prefs.GetCompressedVariationsSeed());
  EXPECT_EQ("abc", install_prefs.GetVariationsSeedSignature());
  // Variations prefs should have been extracted (removed) from the dictionary.
  EXPECT_TRUE(install_prefs.initial_dictionary().empty());
}

// No switches and no sentinel present. This is the standard case for first run.
TEST_F(FirstRunTest, DetermineFirstRunState_FirstRun) {
  internal::FirstRunState result =
      internal::DetermineFirstRunState(false, false, false);
  EXPECT_EQ(internal::FIRST_RUN_TRUE, result);
}

// Force switch is present, overriding both sentinel and suppress switch.
TEST_F(FirstRunTest, DetermineFirstRunState_ForceSwitch) {
  internal::FirstRunState result =
      internal::DetermineFirstRunState(true, true, true);
  EXPECT_EQ(internal::FIRST_RUN_TRUE, result);

  result = internal::DetermineFirstRunState(true, true, false);
  EXPECT_EQ(internal::FIRST_RUN_TRUE, result);

  result = internal::DetermineFirstRunState(false, true, true);
  EXPECT_EQ(internal::FIRST_RUN_TRUE, result);

  result = internal::DetermineFirstRunState(false, true, false);
  EXPECT_EQ(internal::FIRST_RUN_TRUE, result);
}

// No switches, but sentinel present. This is not a first run.
TEST_F(FirstRunTest, DetermineFirstRunState_NotFirstRun) {
  internal::FirstRunState result =
      internal::DetermineFirstRunState(true, false, false);
  EXPECT_EQ(internal::FIRST_RUN_FALSE, result);
}

// Suppress switch is present, overriding sentinel state.
TEST_F(FirstRunTest, DetermineFirstRunState_SuppressSwitch) {
  internal::FirstRunState result =
      internal::DetermineFirstRunState(false, false, true);
  EXPECT_EQ(internal::FIRST_RUN_FALSE, result);

  result = internal::DetermineFirstRunState(true, false, true);
  EXPECT_EQ(internal::FIRST_RUN_FALSE, result);
}

TEST_F(FirstRunTest, GetFirstRunSentinelCreationTime_Created) {
  base::HistogramTester histogram_tester;
  first_run::CreateSentinelIfNeeded();
  histogram_tester.ExpectUniqueSample(
      "FirstRun.Sentinel.Created",
      startup_metric_utils::FirstRunSentinelCreationResult::kSuccess, 1);

  // Gets the creation time of the first run sentinel.
  base::File::Info info;
  ASSERT_TRUE(base::GetFileInfo(GetSentinelFilePath(), &info));

  EXPECT_EQ(info.creation_time, first_run::GetFirstRunSentinelCreationTime());
}

TEST_F(FirstRunTest, GetFirstRunSentinelCreationTime_NotCreated) {
  base::File::Info info;
  ASSERT_FALSE(base::GetFileInfo(GetSentinelFilePath(), &info));

  EXPECT_EQ(
      0,
      first_run::GetFirstRunSentinelCreationTime().InSecondsFSinceUnixEpoch());
}

TEST_F(FirstRunTest, CreateSentinelIfNeeded) {
  {
    base::HistogramTester histogram_tester;
    EXPECT_FALSE(base::PathExists(GetSentinelFilePath()));
    EXPECT_TRUE(IsChromeFirstRun());

    first_run::CreateSentinelIfNeeded();

    histogram_tester.ExpectUniqueSample(
        "FirstRun.Sentinel.Created",
        startup_metric_utils::FirstRunSentinelCreationResult::kSuccess, 1);
  }

  {
    base::HistogramTester histogram_tester;
    EXPECT_TRUE(base::PathExists(GetSentinelFilePath()));
    EXPECT_TRUE(IsChromeFirstRun());

    first_run::CreateSentinelIfNeeded();

    // We are still considered in the first run, but we'll attempt a creation
    // even if the file exists.
    histogram_tester.ExpectUniqueSample(
        "FirstRun.Sentinel.Created",
        startup_metric_utils::FirstRunSentinelCreationResult::kFilePathExists,
        1);
  }

  first_run::ResetCachedSentinelDataForTesting();

  {
    base::HistogramTester histogram_tester;
    EXPECT_TRUE(base::PathExists(GetSentinelFilePath()));
    EXPECT_FALSE(IsChromeFirstRun());

    first_run::CreateSentinelIfNeeded();

    // The file already exists, and we identified that we are not in the first
    // run, the creation is not needed.
    histogram_tester.ExpectTotalCount("FirstRun.Sentinel.Created", 0);
  }
}

TEST_F(FirstRunTest, CreateSentinelIfNeeded_DoneEvenIfForced) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kForceFirstRun);

  {
    base::HistogramTester histogram_tester;
    EXPECT_FALSE(base::PathExists(GetSentinelFilePath()));
    EXPECT_TRUE(IsChromeFirstRun());

    first_run::CreateSentinelIfNeeded();

    histogram_tester.ExpectUniqueSample(
        "FirstRun.Sentinel.Created",
        startup_metric_utils::FirstRunSentinelCreationResult::kSuccess, 1);
  }

  {
    base::HistogramTester histogram_tester;
    EXPECT_TRUE(base::PathExists(GetSentinelFilePath()));
    EXPECT_TRUE(IsChromeFirstRun());

    first_run::CreateSentinelIfNeeded();

    // While the first run state is forced, we'll always attempt to create the
    // sentinel.
    histogram_tester.ExpectUniqueSample(
        "FirstRun.Sentinel.Created",
        startup_metric_utils::FirstRunSentinelCreationResult::kFilePathExists,
        1);
  }

  first_run::ResetCachedSentinelDataForTesting();

  {
    base::HistogramTester histogram_tester;
    EXPECT_TRUE(base::PathExists(GetSentinelFilePath()));
    EXPECT_TRUE(IsChromeFirstRun());

    first_run::CreateSentinelIfNeeded();

    // While the first run state is forced, we'll always attempt to create the
    // sentinel.
    histogram_tester.ExpectUniqueSample(
        "FirstRun.Sentinel.Created",
        startup_metric_utils::FirstRunSentinelCreationResult::kFilePathExists,
        1);
  }
}

TEST_F(FirstRunTest, CreateSentinelIfNeeded_SkippedIfSuppressed) {
  base::HistogramTester histogram_tester;
  base::CommandLine::ForCurrentProcess()->AppendSwitch(switches::kNoFirstRun);

  first_run::CreateSentinelIfNeeded();

  histogram_tester.ExpectTotalCount("FirstRun.Sentinel.Created", 0);
  EXPECT_FALSE(base::PathExists(GetSentinelFilePath()));
  EXPECT_FALSE(IsChromeFirstRun());
}

#if BUILDFLAG(IS_POSIX)  // This test relies on Posix file permissions.
TEST_F(FirstRunTest, CreateSentinelIfNeeded_FileSystemError) {
  base::HistogramTester histogram_tester;

  // Make the user data dir read-only so the sentinel can't be written.
  // Note: the test fixture registers an override to a temp dir for the
  // scope of each test, the below is not as destructive as it seems.
  auto path = base::PathService::CheckedGet(chrome::DIR_USER_DATA);
  ASSERT_TRUE(SetPosixFilePermissions(
      path, DirectoryExists(path) ? (S_IRUSR | S_IXUSR) : S_IRUSR));

  first_run::CreateSentinelIfNeeded();

  histogram_tester.ExpectUniqueSample(
      "FirstRun.Sentinel.Created",
      startup_metric_utils::FirstRunSentinelCreationResult::kFileSystemError,
      1);
  EXPECT_FALSE(base::PathExists(GetSentinelFilePath()));

  EXPECT_TRUE(IsChromeFirstRun());  // This is still a first run.
}
#endif

// This test, and the one below, require customizing the path that the initial
// prefs code will search. On non-Mac platforms that path is derived from
// PathService, but on macOS it instead comes from the system analog of
// PathService (NSSearchPathForDirectoriesInDomains), which we don't have an
// analogous scoped override for.
//
// TODO(ellyjones): Add a scoped override for
// NSSearchPathForDirectoriesInDomains, then re-enable these on macOS.

#if BUILDFLAG(IS_MAC)
#define MAYBE_InitialPrefsUsedIfReadable DISABLED_InitialPrefsUsedIfReadable
#else
#define MAYBE_InitialPrefsUsedIfReadable InitialPrefsUsedIfReadable
#endif

TEST_F(FirstRunTest, MAYBE_InitialPrefsUsedIfReadable) {
  base::ScopedPathOverride override(base::DIR_EXE, GetTestDataPath("initial"));
  std::unique_ptr<installer::InitialPreferences> prefs =
      first_run::LoadInitialPrefs();
  ASSERT_TRUE(prefs);
  EXPECT_EQ(prefs->GetFirstRunTabs()[0], "https://www.chromium.org/initial");
}

#if BUILDFLAG(IS_MAC)
#define MAYBE_LegacyInitialPrefsUsedIfNewFileIsNotPresent \
  DISABLED_LegacyInitialPrefsUsedIfNewFileIsNotPresent
#else
#define MAYBE_LegacyInitialPrefsUsedIfNewFileIsNotPresent \
  LegacyInitialPrefsUsedIfNewFileIsNotPresent
#endif

TEST_F(FirstRunTest, MAYBE_LegacyInitialPrefsUsedIfNewFileIsNotPresent) {
  base::ScopedPathOverride override(base::DIR_EXE, GetTestDataPath("legacy"));
  std::unique_ptr<installer::InitialPreferences> prefs =
      first_run::LoadInitialPrefs();

  ASSERT_TRUE(prefs);
  EXPECT_EQ(prefs->GetFirstRunTabs()[0], "https://www.chromium.org/legacy");
}

class FirstRunBookmarkImportTest : public testing::Test {
 protected:
  FirstRunBookmarkImportTest() {
    TestingProfile::Builder builder;
    builder.AddTestingFactory(BookmarkModelFactory::GetInstance(),
                              BookmarkModelFactory::GetDefaultFactory());
    profile_ = builder.Build();
  }

  TestingProfile* profile() { return profile_.get(); }

 private:
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestingProfile> profile_;
};

TEST_F(FirstRunBookmarkImportTest, SucceedsWithValidDict) {
  base::Value::Dict bookmarks_dict = ParseJSONIfValid(
      R"(
        {
          "first_run_bookmarks": {
            "children": [
              {
                "name": "Google",
                "type": "url",
                "url": "https://www.google.com"
              },
              {
                "name": "My Folder",
                "type": "folder",
                "children": [
                  {
                    "name": "YouTube",
                    "type": "url",
                    "url": "https://www.youtube.com"
                  }
                ]
              }
            ]
          }
        }
        )");
  base::HistogramTester histogram_tester;
  bookmarks::BookmarkModel* bookmark_model =
      BookmarkModelFactory::GetForBrowserContext(profile());

  base::test::TestFuture<void> future;
  BookmarkImportObserver observer(future.GetCallback());
  bookmark_model->AddObserver(&observer);

  StartBookmarksImportFromDict(profile(), std::move(bookmarks_dict));
  ASSERT_TRUE(future.Wait());

  bookmark_model->RemoveObserver(&observer);

  const bookmarks::BookmarkNode* bar = bookmark_model->bookmark_bar_node();
  ASSERT_EQ(2u, bar->children().size());

  const bookmarks::BookmarkNode* node1 = bar->children()[0].get();
  EXPECT_EQ(u"Google", node1->GetTitle());
  EXPECT_EQ(GURL("https://www.google.com"), node1->url());

  const bookmarks::BookmarkNode* node2 = bar->children()[1].get();
  EXPECT_EQ(u"My Folder", node2->GetTitle());
  ASSERT_TRUE(node2->is_folder());
  ASSERT_EQ(1u, node2->children().size());

  const bookmarks::BookmarkNode* node3 = node2->children()[0].get();
  EXPECT_EQ(u"YouTube", node3->GetTitle());
  EXPECT_EQ(GURL("https://www.youtube.com"), node3->url());

  histogram_tester.ExpectUniqueSample("FirstRun.ImportBookmarksDict",
                                      FirstRunImportBookmarksResult::kSuccess,
                                      1);
}

TEST_F(FirstRunBookmarkImportTest, FailsWithInvalidDict) {
  base::Value::Dict bookmarks_dict =
      ParseJSONIfValid(R"({"invalid_key": "invalid_value"})");

  base::HistogramTester histogram_tester;
  bookmarks::BookmarkModel* bookmark_model =
      BookmarkModelFactory::GetForBrowserContext(profile());

  StartBookmarksImportFromDict(profile(), std::move(bookmarks_dict));

  // Since BookmarkModel::BeginExtensiveChanges/EndExtensiveChanges will not be
  // triggered.
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return histogram_tester.GetBucketCount(
               "FirstRun.ImportBookmarksDict",
               FirstRunImportBookmarksResult::kInvalidDict) > 0;
  })) << "Timeout waiting for import to fail";

  const bookmarks::BookmarkNode* bar = bookmark_model->bookmark_bar_node();
  ASSERT_EQ(0u, bar->children().size());

  histogram_tester.ExpectUniqueSample(
      "FirstRun.ImportBookmarksDict",
      FirstRunImportBookmarksResult::kInvalidDict, 1);
}

TEST_F(FirstRunBookmarkImportTest, FailsIfBookmarkModelIsMissing) {
  TestingProfile::Builder builder;
  // Do not install the BookmarkModelFactory.
  std::unique_ptr<TestingProfile> profile_without_bookmark_model =
      builder.Build();

  base::HistogramTester histogram_tester;
  StartBookmarksImportFromDict(profile_without_bookmark_model.get(),
                               base::Value::Dict());

  histogram_tester.ExpectUniqueSample(
      "FirstRun.ImportBookmarksDict",
      FirstRunImportBookmarksResult::kInvalidProfile, 1);
}

TEST_F(FirstRunBookmarkImportTest, SucceedsWithSomeMalformedNodes) {
  base::Value::Dict bookmarks_dict = ParseJSONIfValid(
      R"({
        "first_run_bookmarks": {
          "children": [
            {
              "name": "Invalid",
              "type": "url",
              "url": "invalid-url"
            },
            {
              "name": "Valid",
              "type": "url",
              "url": "https://www.validurl.com"
            },
            {
              "name": "Missing Type",
              "url": "https://www.example.com"
            },
            {
              "type": "url",
              "url": "https://www.example.com"
            },
            {
              "name": "Empty Url",
              "type": "url"
            },
            {
              "name": 123,
              "type": "url",
              "url": "https://www.invalidnamewithnumbers.com"
            },
            {
              "name": "Incorrect type for type",
              "type": 123,
              "url": "https://www.example.com"
            },
            {
              "name": "Incorrect type for url",
              "type": "url",
              "url": 123
            }
          ]
        }
      })");
  base::HistogramTester histogram_tester;
  bookmarks::BookmarkModel* bookmark_model =
      BookmarkModelFactory::GetForBrowserContext(profile());

  base::test::TestFuture<void> future;
  BookmarkImportObserver observer(future.GetCallback());
  bookmark_model->AddObserver(&observer);

  StartBookmarksImportFromDict(profile(), std::move(bookmarks_dict));
  ASSERT_TRUE(future.Wait());

  bookmark_model->RemoveObserver(&observer);

  const bookmarks::BookmarkNode* bar = bookmark_model->bookmark_bar_node();
  ASSERT_EQ(1u, bar->children().size());
  const bookmarks::BookmarkNode* node2 = bar->children()[0].get();
  EXPECT_EQ(u"Valid", node2->GetTitle());
  EXPECT_EQ(GURL("https://www.validurl.com"), node2->url());

  histogram_tester.ExpectUniqueSample("FirstRun.ImportBookmarksDict",
                                      FirstRunImportBookmarksResult::kSuccess,
                                      1);
}

TEST_F(FirstRunBookmarkImportTest, SucceedsWithMalformedFolders) {
  base::Value::Dict bookmarks_dict = ParseJSONIfValid(
      R"({
        "first_run_bookmarks": {
          "children": [
            {
              "name": "Malformed Folder 1",
              "type": "folder"
            },
            {
              "name": "Malformed Folder 2",
              "type": "folder",
              "children": "not-a-list"
            }
          ]
        }
      })");
  base::HistogramTester histogram_tester;
  bookmarks::BookmarkModel* bookmark_model =
      BookmarkModelFactory::GetForBrowserContext(profile());

  base::test::TestFuture<void> future;
  BookmarkImportObserver observer(future.GetCallback());
  bookmark_model->AddObserver(&observer);

  StartBookmarksImportFromDict(profile(), std::move(bookmarks_dict));
  ASSERT_TRUE(future.Wait());

  bookmark_model->RemoveObserver(&observer);

  const bookmarks::BookmarkNode* bar = bookmark_model->bookmark_bar_node();
  ASSERT_EQ(2u, bar->children().size());
  const bookmarks::BookmarkNode* node1 = bar->children()[0].get();
  EXPECT_EQ(u"Malformed Folder 1", node1->GetTitle());
  EXPECT_TRUE(node1->is_folder());
  EXPECT_EQ(0u, node1->children().size());
  const bookmarks::BookmarkNode* node2 = bar->children()[1].get();
  EXPECT_EQ(u"Malformed Folder 2", node2->GetTitle());
  EXPECT_TRUE(node2->is_folder());
  EXPECT_EQ(0u, node2->children().size());

  histogram_tester.ExpectUniqueSample("FirstRun.ImportBookmarksDict",
                                      FirstRunImportBookmarksResult::kSuccess,
                                      1);
}

}  // namespace first_run
