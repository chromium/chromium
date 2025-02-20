// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/bookmarks/bookmark_merged_surface_ordering_storage.h"

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/json/json_reader.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_file_util.h"
#include "base/values.h"
#include "chrome/browser/bookmarks/bookmark_merged_surface_service.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_node.h"
#include "components/bookmarks/test/bookmark_test_helpers.h"
#include "components/bookmarks/test/test_bookmark_client.h"
#include "components/sync/base/features.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

using bookmarks::BookmarkModel;
using bookmarks::BookmarkNode;
using bookmarks::TestBookmarkClient;
using bookmarks::test::AddNodesFromModelString;

base::FilePath GetTestOrderingBookmarksFileNameInNewTempDir() {
  const base::FilePath temp_dir = base::CreateUniqueTempDirectoryScopedToTest();
  return temp_dir.Append(
      FILE_PATH_LITERAL("TestBookmarksMergedSurfaceOrdering"));
}

std::optional<base::Value::Dict> ReadFileToDict(
    const base::FilePath& file_path) {
  std::string file_content;
  if (!base::ReadFileToString(file_path, &file_content)) {
    return std::nullopt;
  }
  return base::JSONReader::ReadDict(file_content);
}

class BookmarkMergedSurfaceOrderingStorageTest : public testing::Test {
 public:
  BookmarkMergedSurfaceOrderingStorageTest()
      : model_(TestBookmarkClient::CreateModel()),
        service_(model_.get(), /*managed_bookmark_service=*/nullptr) {}

  const BookmarkNode* CreateURLNode(const BookmarkNode* parent,
                                    const std::u16string& title,
                                    size_t index) {
    return model_->AddURL(parent, index, title, GURL("http://foo.com"));
  }

  BookmarkModel& model() { return *model_.get(); }

  BookmarkMergedSurfaceService& service() { return service_; }

  base::test::TaskEnvironment& task_environment() { return task_environment_; }

 private:
  base::test::ScopedFeatureList features{
      syncer::kSyncEnableBookmarksInTransportMode};
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  const std::unique_ptr<BookmarkModel> model_;
  BookmarkMergedSurfaceService service_;
};

TEST_F(BookmarkMergedSurfaceOrderingStorageTest, NonDefaultOrderingTracked) {
  const base::FilePath ordering_file_path =
      GetTestOrderingBookmarksFileNameInNewTempDir();
  ASSERT_FALSE(base::PathExists(ordering_file_path));

  BookmarkMergedSurfaceOrderingStorage storage(&service(), ordering_file_path);
  model().CreateAccountPermanentFolders();
  base::Value::Dict expected;
  {
    // Populate bookmark bar nodes.
    const BookmarkNode* bb_1 =
        CreateURLNode(model().account_bookmark_bar_node(), u"1", 0);
    const BookmarkNode* bb_2 =
        model().AddFolder(model().account_bookmark_bar_node(), 1, u"2");
    CreateURLNode(bb_2, u"3", 0);
    const BookmarkNode* local_bb_1 =
        CreateURLNode(model().bookmark_bar_node(), u"a", 0);
    const BookmarkNode* local_bb_2 =
        CreateURLNode(model().bookmark_bar_node(), u"b", 1);
    // Ensure custom order.
    service().Move(bb_1, BookmarkParentFolder::BookmarkBarFolder(), 4u,
                   /*browser=*/nullptr);
    ASSERT_TRUE(service().IsNonDefaultOrderingTracked(
        BookmarkParentFolder::BookmarkBarFolder()));

    expected.Set(
        BookmarkMergedSurfaceOrderingStorage::kBookmarkBarFolderNameKey,
        base::Value::List()
            .Append(base::NumberToString(bb_2->id()))
            .Append(base::NumberToString(local_bb_1->id()))
            .Append(base::NumberToString(local_bb_2->id()))
            .Append(base::NumberToString(bb_1->id())));

    storage.ScheduleSave();
    task_environment().FastForwardUntilNoTasksRemain();

    EXPECT_TRUE(base::PathExists(ordering_file_path));
    std::optional<base::Value::Dict> file_content =
        ReadFileToDict(ordering_file_path);
    ASSERT_TRUE(file_content.has_value());
    EXPECT_EQ(file_content.value(), expected);
  }

  {
    // Populate other nodes.
    const BookmarkNode* other_1 =
        CreateURLNode(model().account_other_node(), u"1", 0);
    const BookmarkNode* other_2 =
        CreateURLNode(model().account_other_node(), u"2", 1);
    const BookmarkNode* local_other_1 =
        CreateURLNode(model().other_node(), u"a", 0);
    const BookmarkNode* local_other_2 =
        CreateURLNode(model().other_node(), u"b", 1);
    // Ensure custom order.
    service().Move(local_other_2, BookmarkParentFolder::OtherFolder(), 0u,
                   /*browser=*/nullptr);
    ASSERT_TRUE(service().IsNonDefaultOrderingTracked(
        BookmarkParentFolder::OtherFolder()));

    expected.Set(
        BookmarkMergedSurfaceOrderingStorage::kOtherBookmarkFolderNameKey,
        base::Value::List()
            .Append(base::NumberToString(local_other_2->id()))
            .Append(base::NumberToString(other_1->id()))
            .Append(base::NumberToString(other_2->id()))
            .Append(base::NumberToString(local_other_1->id())));

    storage.ScheduleSave();
    task_environment().FastForwardUntilNoTasksRemain();

    EXPECT_TRUE(base::PathExists(ordering_file_path));
    std::optional<base::Value::Dict> file_content =
        ReadFileToDict(ordering_file_path);
    ASSERT_TRUE(file_content.has_value());
    EXPECT_EQ(file_content.value().size(), 2u);
    EXPECT_EQ(file_content.value(), expected);
  }

  {
    // Populate mobile nodes.
    const BookmarkNode* mobile_1 =
        CreateURLNode(model().account_mobile_node(), u"1", 0);
    const BookmarkNode* mobile_2 =
        CreateURLNode(model().account_mobile_node(), u"2", 1);
    const BookmarkNode* local_mobile_1 =
        CreateURLNode(model().mobile_node(), u"a", 0);
    const BookmarkNode* local_mobile_2 =
        CreateURLNode(model().mobile_node(), u"b", 1);
    // Ensure custom order.
    service().Move(mobile_2, BookmarkParentFolder::MobileFolder(), 3u,
                   /*browser=*/nullptr);
    ASSERT_TRUE(service().IsNonDefaultOrderingTracked(
        BookmarkParentFolder::MobileFolder()));

    expected.Set(BookmarkMergedSurfaceOrderingStorage::kMobileFolderNameKey,
                 base::Value::List()
                     .Append(base::NumberToString(mobile_1->id()))
                     .Append(base::NumberToString(local_mobile_1->id()))
                     .Append(base::NumberToString(mobile_2->id()))
                     .Append(base::NumberToString(local_mobile_2->id())));

    storage.ScheduleSave();
    task_environment().FastForwardUntilNoTasksRemain();

    EXPECT_TRUE(base::PathExists(ordering_file_path));
    std::optional<base::Value::Dict> file_content =
        ReadFileToDict(ordering_file_path);
    ASSERT_TRUE(file_content.has_value());
    EXPECT_EQ(file_content.value().size(), 3u);
    EXPECT_EQ(file_content.value(), expected);
  }

  // Reset Mobile to default order.
  service().Move(model().account_mobile_node()->children()[1].get(),
                 BookmarkParentFolder::MobileFolder(), 1u,
                 /*browser=*/nullptr);
  ASSERT_FALSE(service().IsNonDefaultOrderingTracked(
      BookmarkParentFolder::MobileFolder()));
  expected.Remove(BookmarkMergedSurfaceOrderingStorage::kMobileFolderNameKey);

  storage.ScheduleSave();
  task_environment().FastForwardUntilNoTasksRemain();

  EXPECT_TRUE(base::PathExists(ordering_file_path));
  std::optional<base::Value::Dict> file_content =
      ReadFileToDict(ordering_file_path);
  ASSERT_TRUE(file_content.has_value());
  EXPECT_EQ(file_content.value().size(), 2u);
  EXPECT_EQ(file_content.value(), expected);
}

TEST_F(BookmarkMergedSurfaceOrderingStorageTest, DefaultOrdering) {
  const base::FilePath ordering_file_path =
      GetTestOrderingBookmarksFileNameInNewTempDir();
  ASSERT_FALSE(base::PathExists(ordering_file_path));

  model().CreateAccountPermanentFolders();
  AddNodesFromModelString(&model(), model().bookmark_bar_node(), "1 2 3 ");
  AddNodesFromModelString(&model(), model().other_node(), "a b c ");
  AddNodesFromModelString(&model(), model().mobile_node(), "K L M ");
  AddNodesFromModelString(&model(), model().account_bookmark_bar_node(),
                          "7 8 ");
  AddNodesFromModelString(&model(), model().account_other_node(), "d e ");
  AddNodesFromModelString(&model(), model().account_mobile_node(), "X Y ");
  ASSERT_FALSE(service().IsNonDefaultOrderingTracked(
      BookmarkParentFolder::BookmarkBarFolder()));
  ASSERT_FALSE(service().IsNonDefaultOrderingTracked(
      BookmarkParentFolder::OtherFolder()));
  ASSERT_FALSE(service().IsNonDefaultOrderingTracked(
      BookmarkParentFolder::MobileFolder()));

  BookmarkMergedSurfaceOrderingStorage storage(&service(), ordering_file_path);
  storage.ScheduleSave();
  task_environment().FastForwardUntilNoTasksRemain();

  EXPECT_TRUE(base::PathExists(ordering_file_path));

  std::optional<base::Value::Dict> file_content =
      ReadFileToDict(ordering_file_path);
  ASSERT_TRUE(file_content.has_value());
  EXPECT_TRUE(file_content->empty());
}

TEST_F(BookmarkMergedSurfaceOrderingStorageTest, NoAccountNodes) {
  const base::FilePath ordering_file_path =
      GetTestOrderingBookmarksFileNameInNewTempDir();
  ASSERT_FALSE(base::PathExists(ordering_file_path));
  BookmarkMergedSurfaceOrderingStorage storage(&service(), ordering_file_path);

  AddNodesFromModelString(&model(), model().bookmark_bar_node(), "1 2 3 ");
  AddNodesFromModelString(&model(), model().other_node(), "a b c ");
  AddNodesFromModelString(&model(), model().mobile_node(), "K L M ");
  ASSERT_FALSE(service().IsNonDefaultOrderingTracked(
      BookmarkParentFolder::BookmarkBarFolder()));
  ASSERT_FALSE(service().IsNonDefaultOrderingTracked(
      BookmarkParentFolder::OtherFolder()));
  ASSERT_FALSE(service().IsNonDefaultOrderingTracked(
      BookmarkParentFolder::MobileFolder()));

  storage.ScheduleSave();
  task_environment().FastForwardUntilNoTasksRemain();

  EXPECT_TRUE(base::PathExists(ordering_file_path));

  std::optional<base::Value::Dict> file_content =
      ReadFileToDict(ordering_file_path);
  ASSERT_TRUE(file_content.has_value());
  EXPECT_TRUE(file_content->empty());
}

TEST_F(BookmarkMergedSurfaceOrderingStorageTest,
       ShouldSaveFileToDiskAfterDelay) {
  const base::FilePath ordering_file_path =
      GetTestOrderingBookmarksFileNameInNewTempDir();
  BookmarkMergedSurfaceOrderingStorage storage(&service(), ordering_file_path);

  ASSERT_FALSE(storage.HasScheduledSaveForTesting());
  ASSERT_FALSE(base::PathExists(ordering_file_path));

  storage.ScheduleSave();
  EXPECT_TRUE(storage.HasScheduledSaveForTesting());
  EXPECT_FALSE(base::PathExists(ordering_file_path));

  // Advance clock until immediately before saving takes place.
  task_environment().FastForwardBy(
      BookmarkMergedSurfaceOrderingStorage::kSaveDelay -
      base::Milliseconds(10));
  EXPECT_TRUE(storage.HasScheduledSaveForTesting());
  EXPECT_FALSE(base::PathExists(ordering_file_path));

  // Advance clock past the saving moment.
  task_environment().FastForwardBy(base::Milliseconds(20));
  EXPECT_FALSE(storage.HasScheduledSaveForTesting());
  EXPECT_TRUE(base::PathExists(ordering_file_path));
}

TEST(BookmarkMergedSurfaceOrderingStorageShutdownTest,
     ShouldSaveFileDespiteShutdownWhileScheduled) {
  const base::FilePath ordering_file_path =
      GetTestOrderingBookmarksFileNameInNewTempDir();
  std::unique_ptr<BookmarkModel> model(TestBookmarkClient::CreateModel());
  BookmarkMergedSurfaceService service(model.get(),
                                       /*managed_bookmark_service=*/nullptr);
  {
    base::test::TaskEnvironment task_environment{
        base::test::TaskEnvironment::TimeSource::MOCK_TIME};
    BookmarkMergedSurfaceOrderingStorage storage(&service, ordering_file_path);

    storage.ScheduleSave();
    ASSERT_TRUE(storage.HasScheduledSaveForTesting());
    ASSERT_FALSE(base::PathExists(ordering_file_path));
  }

  // `TaskEnvironment` and `BookmarkMergedSurfaceOrderingStorage` have been
  // destroyed, mimic-ing a browser shutdown.
  EXPECT_TRUE(base::PathExists(ordering_file_path));
  std::optional<base::Value::Dict> file_content =
      ReadFileToDict(ordering_file_path);
  EXPECT_TRUE(file_content.has_value());
}

}  // namespace
