// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/first_run/bookmark_importer.h"

#include <utility>

#include "base/json/json_reader.h"
#include "base/task/current_thread.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/test_future.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/first_run/first_run_internal.h"
#include "chrome/test/base/testing_profile.h"
#include "components/bookmarks/browser/base_bookmark_model_observer.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_node.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

using first_run::internal::FirstRunImportBookmarksResult;

namespace first_run {

namespace {

base::Value::Dict ParseJSONIfValid(std::string_view json) {
  std::optional<base::Value::Dict> parsed_json =
      base::JSONReader::ReadDict(json, base::JSON_PARSE_CHROMIUM_EXTENSIONS);
  if (!parsed_json.has_value()) {
    ADD_FAILURE() << "JSON parsing failed";
    return {};
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

class BookmarkDictImporterTest : public testing::Test {
 protected:
  BookmarkDictImporterTest() {
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

TEST_F(BookmarkDictImporterTest, SucceedsWithValidDict) {
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

  StartBookmarkImportFromDict(profile(), std::move(bookmarks_dict));
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

TEST_F(BookmarkDictImporterTest, FailsWithInvalidDict) {
  base::Value::Dict bookmarks_dict =
      ParseJSONIfValid(R"({"invalid_key": "invalid_value"})");

  base::HistogramTester histogram_tester;
  bookmarks::BookmarkModel* bookmark_model =
      BookmarkModelFactory::GetForBrowserContext(profile());

  StartBookmarkImportFromDict(profile(), std::move(bookmarks_dict));

  histogram_tester.ExpectUniqueSample(
      "FirstRun.ImportBookmarksDict",
      FirstRunImportBookmarksResult::kInvalidDict, 1);

  base::test::RunUntil([bookmark_model]() { return bookmark_model->loaded(); });
  const bookmarks::BookmarkNode* bar = bookmark_model->bookmark_bar_node();
  ASSERT_EQ(0u, bar->children().size());
}

TEST_F(BookmarkDictImporterTest, FailsIfBookmarkModelIsMissing) {
  base::Value::Dict bookmarks_dict = ParseJSONIfValid(
      R"(
        {
          "first_run_bookmarks": {
            "children": [
              {
                "name": "Google",
                "type": "url",
                "url": "https://www.google.com"
              }
            ]
          }
        }
      )");
  TestingProfile::Builder builder;

  // Do not install the BookmarkModelFactory.
  std::unique_ptr<TestingProfile> profile_without_bookmark_model =
      builder.Build();

  base::HistogramTester histogram_tester;
  StartBookmarkImportFromDict(profile_without_bookmark_model.get(),
                              std::move(bookmarks_dict));

  histogram_tester.ExpectUniqueSample(
      "FirstRun.ImportBookmarksDict",
      FirstRunImportBookmarksResult::kInvalidProfile, 1);
}

TEST_F(BookmarkDictImporterTest, SucceedsWithSomeMalformedNodes) {
  base::Value::Dict bookmarks_dict = ParseJSONIfValid(
      R"(
        {
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
        }
      )");
  base::HistogramTester histogram_tester;
  bookmarks::BookmarkModel* bookmark_model =
      BookmarkModelFactory::GetForBrowserContext(profile());

  base::test::TestFuture<void> future;
  BookmarkImportObserver observer(future.GetCallback());
  bookmark_model->AddObserver(&observer);

  StartBookmarkImportFromDict(profile(), std::move(bookmarks_dict));
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

TEST_F(BookmarkDictImporterTest, SucceedsWithMalformedFolders) {
  base::Value::Dict bookmarks_dict = ParseJSONIfValid(
      R"(
        {
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
        }
      )");
  base::HistogramTester histogram_tester;
  bookmarks::BookmarkModel* bookmark_model =
      BookmarkModelFactory::GetForBrowserContext(profile());

  base::test::TestFuture<void> future;
  BookmarkImportObserver observer(future.GetCallback());
  bookmark_model->AddObserver(&observer);

  StartBookmarkImportFromDict(profile(), std::move(bookmarks_dict));
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
