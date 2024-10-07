// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/bookmarks/bookmark_merged_surface_service.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/strings/string_number_conversions.h"
#include "base/values.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_node.h"
#include "components/bookmarks/common/bookmark_pref_names.h"
#include "components/bookmarks/managed/managed_bookmark_service.h"
#include "components/bookmarks/test/bookmark_test_helpers.h"
#include "components/bookmarks/test/test_bookmark_client.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

using PermanentFolderType = BookmarkParentFolder::PermanentFolderType;
using bookmarks::BookmarkNode;
using bookmarks::test::AddNodesFromModelString;
using bookmarks::test::ModelStringFromNode;

base::Value::List ConstructManagedBookmarks(size_t managed_bookmarks_size) {
  const GURL url("http://google.com/");
  base::Value::List bookamrks_list;
  for (size_t i = 0; i < managed_bookmarks_size; ++i) {
    bookamrks_list.Append(
        base::Value::Dict()
            .Set("name", "Bookmark " + base::NumberToString(i))
            .Set("url", url.spec()));
  }
  return bookamrks_list;
}

class TestBookmarkClientWithManagedService
    : public bookmarks::TestBookmarkClient {
 public:
  explicit TestBookmarkClientWithManagedService(
      bookmarks::ManagedBookmarkService* managed_bookmark_service)
      : managed_bookmark_service_(managed_bookmark_service) {
    CHECK(managed_bookmark_service);
  }

  // BookmarkClient:
  void Init(bookmarks::BookmarkModel* model) override {
    managed_bookmark_service_->BookmarkModelCreated(model);
  }
  bookmarks::LoadManagedNodeCallback GetLoadManagedNodeCallback() override {
    return managed_bookmark_service_->GetLoadManagedNodeCallback();
  }
  bool CanSetPermanentNodeTitle(const BookmarkNode* permanent_node) override {
    return managed_bookmark_service_->CanSetPermanentNodeTitle(permanent_node);
  }
  bool IsNodeManaged(const BookmarkNode* node) override {
    return managed_bookmark_service_->IsNodeManaged(node);
  }

 private:
  const raw_ptr<bookmarks::ManagedBookmarkService> managed_bookmark_service_;
};

class BookmarkMergedSurfaceServiceTest : public testing::Test {
 public:
  void LoadBookmarkModelWithManaged(size_t managed_bookmarks_size) {
    LoadBookmarkModel(true, managed_bookmarks_size);
  }

  void LoadBookmarkModel(bool with_managed_node = false,
                         size_t managed_bookmarks_size = 0) {
    std::unique_ptr<bookmarks::TestBookmarkClient> bookmark_client;
    if (with_managed_node) {
      CHECK(managed_bookmarks_size);
      managed_bookmark_service_ =
          CreateManagedBookmarkService(managed_bookmarks_size);
      bookmark_client = std::make_unique<TestBookmarkClientWithManagedService>(
          managed_bookmark_service_.get());
    } else {
      bookmark_client = std::make_unique<bookmarks::TestBookmarkClient>();
    }
    model_ =
        std::make_unique<bookmarks::BookmarkModel>(std::move(bookmark_client));
    model_->LoadEmptyForTest();
    service_ = std::make_unique<BookmarkMergedSurfaceService>(
        model_.get(), managed_bookmark_service_.get());
  }

  ~BookmarkMergedSurfaceServiceTest() override = default;

  BookmarkMergedSurfaceService& service() { return *service_; }
  bookmarks::BookmarkModel& model() { return *model_; }
  const BookmarkNode* managed_node() const {
    return managed_bookmark_service_->managed_node();
  }

 private:
  std::unique_ptr<bookmarks::ManagedBookmarkService>
  CreateManagedBookmarkService(size_t managed_bookmarks_size) {
    prefs_.registry()->RegisterListPref(bookmarks::prefs::kManagedBookmarks);
    prefs_.registry()->RegisterStringPref(
        bookmarks::prefs::kManagedBookmarksFolderName, std::string());

    prefs_.SetString(bookmarks::prefs::kManagedBookmarksFolderName, "Managed");
    prefs_.SetManagedPref(bookmarks::prefs::kManagedBookmarks,
                          ConstructManagedBookmarks(managed_bookmarks_size));

    return std::make_unique<bookmarks::ManagedBookmarkService>(
        &prefs_, base::BindRepeating(
                     []() -> std::string { return "managedDomain.com"; }));
  }

  sync_preferences::TestingPrefServiceSyncable prefs_;
  std::unique_ptr<bookmarks::ManagedBookmarkService> managed_bookmark_service_;
  std::unique_ptr<bookmarks::BookmarkModel> model_;
  std::unique_ptr<BookmarkMergedSurfaceService> service_;
};

TEST_F(BookmarkMergedSurfaceServiceTest, GetChildrenCount) {
  const size_t kManagedBookmarksSize = 5;
  LoadBookmarkModelWithManaged(kManagedBookmarksSize);
  EXPECT_EQ(
      service().GetChildrenCount(BookmarkParentFolder::BookmarkBarFolder()),
      0u);
  EXPECT_EQ(service().GetChildrenCount(BookmarkParentFolder::OtherFolder()),
            0u);
  EXPECT_EQ(service().GetChildrenCount(BookmarkParentFolder::MobileFolder()),
            0u);
  EXPECT_EQ(service().GetChildrenCount(BookmarkParentFolder::ManagedFolder()),
            kManagedBookmarksSize);

  AddNodesFromModelString(&model(), model().bookmark_bar_node(),
                          "1 2 3 f1:[ 4 5 f2:[ 6 ] ]");
  EXPECT_EQ(
      service().GetChildrenCount(BookmarkParentFolder::BookmarkBarFolder()),
      4u);
  const BookmarkNode* folder_f1 =
      model().bookmark_bar_node()->children()[3].get();
  EXPECT_EQ(service().GetChildrenCount(
                BookmarkParentFolder::FromNonPermanentNode(folder_f1)),
            3u);
  const BookmarkNode* folder_f2 = folder_f1->children()[2].get();
  EXPECT_EQ(service().GetChildrenCount(
                BookmarkParentFolder::FromNonPermanentNode(folder_f2)),
            1u);

  AddNodesFromModelString(&model(), model().other_node(), "1 2 3 ");
  EXPECT_EQ(service().GetChildrenCount(BookmarkParentFolder::OtherFolder()),
            3u);

  AddNodesFromModelString(&model(), model().mobile_node(), "4 5 6 ");
  EXPECT_EQ(service().GetChildrenCount(BookmarkParentFolder::MobileFolder()),
            3u);
}

TEST_F(BookmarkMergedSurfaceServiceTest, ManagedNodeNull) {
  LoadBookmarkModel();
  EXPECT_EQ(service().GetChildrenCount(BookmarkParentFolder::ManagedFolder()),
            0u);
}

TEST_F(BookmarkMergedSurfaceServiceTest, IsPermanentNodeOfType) {
  LoadBookmarkModelWithManaged(/*managed_bookmarks_size=*/1);

  EXPECT_TRUE(service().IsPermanentNodeOfType(
      model().bookmark_bar_node(), PermanentFolderType::kBookmarkBarNode));
  EXPECT_TRUE(service().IsPermanentNodeOfType(model().other_node(),
                                              PermanentFolderType::kOtherNode));
  EXPECT_TRUE(BookmarkMergedSurfaceService::IsPermanentNodeOfType(
      model().mobile_node(), PermanentFolderType::kMobileNode));
  EXPECT_TRUE(BookmarkMergedSurfaceService::IsPermanentNodeOfType(
      managed_node(), PermanentFolderType::kManagedNode));

  EXPECT_FALSE(BookmarkMergedSurfaceService::IsPermanentNodeOfType(
      model().other_node(), PermanentFolderType::kMobileNode));

  AddNodesFromModelString(&model(), model().other_node(), "1 2 3 ");
  EXPECT_EQ(model().other_node()->children().size(), 3u);
  for (const auto& node : model().other_node()->children()) {
    EXPECT_FALSE(BookmarkMergedSurfaceService::IsPermanentNodeOfType(
        node.get(), PermanentFolderType::kOtherNode));
  }
}

TEST_F(BookmarkMergedSurfaceServiceTest, MoveToPermanentFolder) {
  LoadBookmarkModel();
  AddNodesFromModelString(&model(), model().bookmark_bar_node(), "1 2 3 ");
  AddNodesFromModelString(&model(), model().other_node(), "4 5 6 ");

  // Move node "2" in bookmark bar to be after "4" in other node.
  const BookmarkNode* node = model().bookmark_bar_node()->children()[1].get();
  service().Move(node, BookmarkParentFolder::OtherFolder(), 1);
  EXPECT_EQ(ModelStringFromNode(model().bookmark_bar_node()), "1 3 ");
  EXPECT_EQ(ModelStringFromNode(model().other_node()), "4 2 5 6 ");
}

TEST_F(BookmarkMergedSurfaceServiceTest, MoveToBookmarkNode) {
  LoadBookmarkModel();
  AddNodesFromModelString(&model(), model().bookmark_bar_node(),
                          "1 2 3 f1:[ 4 5 ] ");
  AddNodesFromModelString(&model(), model().other_node(), "6 7 8 ");

  // Move node "8" from other node to node "f1" after "4".
  const BookmarkNode* node_to_move = model().other_node()->children()[2].get();
  const BookmarkNode* destination =
      model().bookmark_bar_node()->children()[3].get();
  service().Move(node_to_move,
                 BookmarkParentFolder::FromNonPermanentNode(destination), 1);

  EXPECT_EQ(ModelStringFromNode(model().bookmark_bar_node()),
            "1 2 3 f1:[ 4 8 5 ] ");
  EXPECT_EQ(ModelStringFromNode(model().other_node()), "6 7 ");
}

// Tests for `BookmarkParentFolder`

TEST(BookmarkParentFolder, FromPermanentFolderType) {
  const size_t kPermanentFolderTypes = 4;
  std::array<BookmarkParentFolder, kPermanentFolderTypes> bookmark_folder = {
      BookmarkParentFolder::BookmarkBarFolder(),
      BookmarkParentFolder::OtherFolder(), BookmarkParentFolder::MobileFolder(),
      BookmarkParentFolder::ManagedFolder()};

  std::array<PermanentFolderType, kPermanentFolderTypes> folder_type = {
      PermanentFolderType::kBookmarkBarNode, PermanentFolderType::kOtherNode,
      PermanentFolderType::kMobileNode, PermanentFolderType::kManagedNode};

  for (size_t i = 0; i < kPermanentFolderTypes; i++) {
    EXPECT_FALSE(bookmark_folder[i].HoldsNonPermanentFolder());
    EXPECT_FALSE(bookmark_folder[i].as_non_permanent_folder());

    ASSERT_TRUE(bookmark_folder[i].as_permanent_folder());
    EXPECT_EQ(*bookmark_folder[i].as_permanent_folder(), folder_type[i]);
  }
}

TEST(BookmarkParentFolder, FromNonPermanentBookmarkFolder) {
  BookmarkNode node(2, base::Uuid::GenerateRandomV4(), GURL());
  ASSERT_TRUE(node.is_folder());

  BookmarkParentFolder folder =
      BookmarkParentFolder::FromNonPermanentNode(&node);
  EXPECT_TRUE(folder.HoldsNonPermanentFolder());
  EXPECT_EQ(folder.as_non_permanent_folder(), &node);
  EXPECT_FALSE(folder.as_permanent_folder());
}

}  // namespace
