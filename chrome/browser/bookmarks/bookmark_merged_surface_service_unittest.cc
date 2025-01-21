// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/bookmarks/bookmark_merged_surface_service.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "base/values.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/bookmarks/bookmark_parent_folder_children.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_node.h"
#include "components/bookmarks/common/bookmark_pref_names.h"
#include "components/bookmarks/managed/managed_bookmark_service.h"
#include "components/bookmarks/test/bookmark_test_helpers.h"
#include "components/bookmarks/test/test_bookmark_client.h"
#include "components/sync/base/features.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

using PermanentFolderType = BookmarkParentFolder::PermanentFolderType;
using bookmarks::BookmarkNode;
using bookmarks::test::AddNodesFromModelString;
using bookmarks::test::ModelStringFromNode;
using testing::UnorderedElementsAre;

base::Value::List ConstructManagedBookmarks(size_t managed_bookmarks_size) {
  const GURL url("http://google.com/");
  base::Value::List bookmarks_list;
  for (size_t i = 0; i < managed_bookmarks_size; ++i) {
    base::Value::List folder_items;
    folder_items.Append(
        base::Value::Dict().Set("name", "Google").Set("url", url.spec()));
    bookmarks_list.Append(
        base::Value::Dict()
            .Set("name", "Bookmark folder " + base::NumberToString(i))
            .Set("children", std::move(folder_items)));
  }
  return bookmarks_list;
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

  void PerformMoveAction(Browser* browser,
                         const bookmarks::BookmarkNode* node,
                         const bookmarks::BookmarkNode* target_node,
                         size_t index) {
    model_->Move(node, target_node, index);
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

  base::test::ScopedFeatureList features_{
      syncer::kSyncEnableBookmarksInTransportMode};
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
                BookmarkParentFolder::FromFolderNode(folder_f1)),
            3u);
  const BookmarkNode* folder_f2 = folder_f1->children()[2].get();
  EXPECT_EQ(service().GetChildrenCount(
                BookmarkParentFolder::FromFolderNode(folder_f2)),
            1u);

  AddNodesFromModelString(&model(), model().other_node(), "1 2 3 ");
  EXPECT_EQ(service().GetChildrenCount(BookmarkParentFolder::OtherFolder()),
            3u);

  AddNodesFromModelString(&model(), model().mobile_node(), "4 5 6 ");
  EXPECT_EQ(service().GetChildrenCount(BookmarkParentFolder::MobileFolder()),
            3u);
}

TEST_F(BookmarkMergedSurfaceServiceTest, GetChildrenWithAccountNodes) {
  LoadBookmarkModel();
  model().CreateAccountPermanentFolders();
  const BookmarkNode* local_bb_node = model().bookmark_bar_node();
  const BookmarkNode* account_bb_node = model().account_bookmark_bar_node();
  ASSERT_TRUE(account_bb_node);

  AddNodesFromModelString(&model(), local_bb_node, "1 2 3 f1:[ 4 5 f2:[ 6 ] ]");
  AddNodesFromModelString(&model(), account_bb_node,
                          "7 8 9 f3:[ 10 11 f4:[ 12 ] ]");
  ASSERT_FALSE(local_bb_node->children().empty());
  ASSERT_FALSE(account_bb_node->children().empty());

  BookmarkParentFolder bb_folder(BookmarkParentFolder::BookmarkBarFolder());
  size_t expected_children_size =
      local_bb_node->children().size() + account_bb_node->children().size();
  EXPECT_EQ(service().GetChildrenCount(bb_folder), expected_children_size);

  BookmarkParentFolderChildren children = service().GetChildren(bb_folder);
  ASSERT_EQ(children.size(), expected_children_size);

  size_t index = 0;
  size_t account_bb_node_children_size = account_bb_node->children().size();
  while (index < expected_children_size) {
    const BookmarkNode* expected_bookmark_node =
        index < account_bb_node_children_size
            ? account_bb_node->children()[index].get()
            : local_bb_node->children()[index - account_bb_node_children_size]
                  .get();
    EXPECT_EQ(children[index++], expected_bookmark_node);
  }

  // Tests `GetNodeAtIndex()`.
  EXPECT_EQ(service().GetNodeAtIndex(bb_folder, 0u),
            account_bb_node->children()[0].get());
  EXPECT_EQ(
      service().GetNodeAtIndex(bb_folder, account_bb_node_children_size - 1u),
      account_bb_node->children().back().get());
  EXPECT_EQ(service().GetNodeAtIndex(bb_folder, account_bb_node_children_size),
            local_bb_node->children()[0].get());
  EXPECT_EQ(service().GetNodeAtIndex(bb_folder, expected_children_size - 1u),
            local_bb_node->children().back().get());

  // Tests `GetIndexOf()`.
  index = 0;
  for (const auto& node : account_bb_node->children()) {
    EXPECT_EQ(service().GetIndexOf(node.get()), index++);
  }
  for (const auto& node : local_bb_node->children()) {
    EXPECT_EQ(service().GetIndexOf(node.get()), index++);
  }
}

TEST_F(BookmarkMergedSurfaceServiceTest, ManagedNodeNull) {
  LoadBookmarkModel();
  EXPECT_EQ(service().GetChildrenCount(BookmarkParentFolder::ManagedFolder()),
            0u);
}

TEST_F(BookmarkMergedSurfaceServiceTest, GetIndexOf) {
  LoadBookmarkModelWithManaged(/*managed_bookmarks_size=*/3);
  AddNodesFromModelString(&model(), model().bookmark_bar_node(),
                          "1 2 3 f1:[ 4 5 ] ");
  AddNodesFromModelString(&model(), model().other_node(), "6 7 8 ");

  const BookmarkNode* f1 = model().bookmark_bar_node()->children()[3].get();
  EXPECT_EQ(service().GetIndexOf(f1), 3u);
  EXPECT_EQ(service().GetIndexOf(f1->children()[1].get()), 1u);
  EXPECT_EQ(service().GetIndexOf(model().other_node()->children()[2].get()),
            2u);

  // Managed node.
  EXPECT_EQ(service().GetIndexOf(managed_node()->children()[1].get()), 1u);
}

TEST_F(BookmarkMergedSurfaceServiceTest, GetNodeAtIndex) {
  LoadBookmarkModel();
  AddNodesFromModelString(&model(), model().bookmark_bar_node(),
                          "1 2 3 f1:[ 4 5 ] ");
  AddNodesFromModelString(&model(), model().other_node(), "6 7 8 ");

  {
    BookmarkParentFolder folder = BookmarkParentFolder::BookmarkBarFolder();
    EXPECT_EQ(service().GetNodeAtIndex(folder, 0),
              model().bookmark_bar_node()->children()[0].get());
    EXPECT_EQ(service().GetNodeAtIndex(folder, 3),
              model().bookmark_bar_node()->children()[3].get());
  }

  {
    const BookmarkNode* f1 = model().bookmark_bar_node()->children()[3].get();
    BookmarkParentFolder folder = BookmarkParentFolder::FromFolderNode(f1);
    EXPECT_EQ(service().GetNodeAtIndex(folder, 0), f1->children()[0].get());
    EXPECT_EQ(service().GetNodeAtIndex(folder, 1), f1->children()[1].get());
  }

  {
    BookmarkParentFolder folder = BookmarkParentFolder::OtherFolder();
    EXPECT_EQ(service().GetNodeAtIndex(folder, 0),
              model().other_node()->children()[0].get());
    EXPECT_EQ(service().GetNodeAtIndex(folder, 2),
              model().other_node()->children()[2].get());
  }
}

TEST_F(BookmarkMergedSurfaceServiceTest, IsParentFolderManaged) {
  LoadBookmarkModelWithManaged(2);
  AddNodesFromModelString(&model(), model().bookmark_bar_node(), "f1:[ 4 5 ]");

  EXPECT_FALSE(service().IsParentFolderManaged(
      BookmarkParentFolder::BookmarkBarFolder()));
  EXPECT_FALSE(
      service().IsParentFolderManaged(BookmarkParentFolder::FromFolderNode(
          model().bookmark_bar_node()->children()[0].get())));

  EXPECT_TRUE(
      service().IsParentFolderManaged(BookmarkParentFolder::ManagedFolder()));
  const BookmarkNode* root_managed_node = managed_node();
  EXPECT_TRUE(
      service().IsParentFolderManaged(BookmarkParentFolder::FromFolderNode(
          root_managed_node->children()[0].get())));
}

TEST_F(BookmarkMergedSurfaceServiceTest,
       IsParentFolderManagedNoManagedService) {
  LoadBookmarkModel();
  AddNodesFromModelString(&model(), model().bookmark_bar_node(), "f1:[ 4 5 ]");
  EXPECT_FALSE(
      service().IsParentFolderManaged(BookmarkParentFolder::FromFolderNode(
          model().bookmark_bar_node()->children()[0].get())));
}

TEST_F(BookmarkMergedSurfaceServiceTest, MoveToPermanentFolder) {
  LoadBookmarkModel();
  AddNodesFromModelString(&model(), model().bookmark_bar_node(), "1 2 3 ");
  AddNodesFromModelString(&model(), model().other_node(), "4 5 6 ");

  // Move node "2" in bookmark bar to be after "4" in other node.
  const BookmarkNode* node = model().bookmark_bar_node()->children()[1].get();
  service().Move(node, BookmarkParentFolder::OtherFolder(), 1,
                 /*browser=*/nullptr);
  EXPECT_EQ(ModelStringFromNode(model().bookmark_bar_node()), "1 3 ");
  EXPECT_EQ(ModelStringFromNode(model().other_node()), "4 2 5 6 ");
}

TEST_F(BookmarkMergedSurfaceServiceTest,
       MoveToPermanentFolderWithAccountNodes) {
  LoadBookmarkModel();
  model().CreateAccountPermanentFolders();

  AddNodesFromModelString(&model(), model().bookmark_bar_node(), "1 2 3 ");
  AddNodesFromModelString(&model(), model().other_node(), "4 5 6 ");

  AddNodesFromModelString(&model(), model().account_bookmark_bar_node(),
                          "A1 A2 A3 ");
  AddNodesFromModelString(&model(), model().account_other_node(), "A4 A5 A6 ");

  // Move from local bookmark bar to local other node.
  // Move node "2" in bookmark bar to be after "4" in other node.
  const BookmarkNode* node = model().bookmark_bar_node()->children()[1].get();
  service().Move(node, BookmarkParentFolder::OtherFolder(), 1,
                 /*browser=*/nullptr);
  EXPECT_EQ(ModelStringFromNode(model().bookmark_bar_node()), "1 3 ");
  EXPECT_EQ(ModelStringFromNode(model().other_node()), "2 4 5 6 ");
  EXPECT_EQ(service().GetIndexOf(node), 1u);

  // Move within bookmark bar.
  node = model().bookmark_bar_node()->children()[1].get();
  service().Move(node, BookmarkParentFolder::BookmarkBarFolder(), 1,
                 /*browser=*/nullptr);
  EXPECT_EQ(ModelStringFromNode(model().bookmark_bar_node()), "3 1 ");
  EXPECT_EQ(ModelStringFromNode(model().account_bookmark_bar_node()),
            "A1 A2 A3 ");
  // Expected order: "A1 3 A2 A3 1 "
  EXPECT_EQ(service().GetIndexOf(node), 1u);

  // Move from account other node to account bookmark bar.
  node = model().account_other_node()->children()[0].get();
  service().Move(node, BookmarkParentFolder::BookmarkBarFolder(), 2,
                 /*browser=*/nullptr);
  EXPECT_EQ(ModelStringFromNode(model().bookmark_bar_node()), "3 1 ");
  EXPECT_EQ(ModelStringFromNode(model().account_bookmark_bar_node()),
            "A1 A4 A2 A3 ");
  EXPECT_EQ(ModelStringFromNode(model().account_other_node()), "A5 A6 ");
  // Expected order: "A1 3 A4 A2 A3 1 "
  EXPECT_EQ(service().GetIndexOf(node), 2u);
  EXPECT_EQ(
      service().GetNodeAtIndex(BookmarkParentFolder::BookmarkBarFolder(), 1u),
      model().bookmark_bar_node()->children()[0].get());
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
                 BookmarkParentFolder::FromFolderNode(destination), 1,
                 /*browser=*/nullptr);

  EXPECT_EQ(ModelStringFromNode(model().bookmark_bar_node()),
            "1 2 3 f1:[ 4 8 5 ] ");
  EXPECT_EQ(ModelStringFromNode(model().other_node()), "6 7 ");
}

TEST_F(BookmarkMergedSurfaceServiceTest, MoveFromAccountToLocalStorage) {
  LoadBookmarkModel();
  model().CreateAccountPermanentFolders();

  AddNodesFromModelString(&model(), model().bookmark_bar_node(), "f1:[ 1 2 ] ");
  AddNodesFromModelString(&model(), model().account_bookmark_bar_node(),
                          "A1 A2 A3 ");

  const BookmarkNode* node =
      model().account_bookmark_bar_node()->children()[0].get();
  const BookmarkNode* destination =
      model().bookmark_bar_node()->children()[0].get();

  // Bypass the move storage dialog and directly move the bookmark.
  base::MockCallback<
      BookmarkMergedSurfaceService::ShowMoveStorageDialogCallback>
      move_storage_callback;
  service().SetShowMoveStorageDialogCallbackForTesting(
      move_storage_callback.Get());
  EXPECT_CALL(move_storage_callback, Run(testing::_, node, destination, 1))
      .WillOnce(testing::Invoke(
          this, &BookmarkMergedSurfaceServiceTest::PerformMoveAction));

  service().Move(node, BookmarkParentFolder::FromFolderNode(destination), 1,
                 /*browser=*/nullptr);

  EXPECT_EQ(ModelStringFromNode(model().bookmark_bar_node()), "f1:[ 1 A1 2 ] ");
  EXPECT_EQ(ModelStringFromNode(model().account_bookmark_bar_node()), "A2 A3 ");
}

TEST_F(BookmarkMergedSurfaceServiceTest, MoveFromLocalToAccountStorage) {
  LoadBookmarkModel();
  model().CreateAccountPermanentFolders();

  AddNodesFromModelString(&model(), model().bookmark_bar_node(), "1 2 3 ");
  AddNodesFromModelString(&model(), model().account_bookmark_bar_node(),
                          "f1:[ A1 A2 ] ");

  const BookmarkNode* node = model().bookmark_bar_node()->children()[0].get();
  const BookmarkNode* destination =
      model().account_bookmark_bar_node()->children()[0].get();

  // Bypass the move storage dialog and directly move the bookmark.
  base::MockCallback<
      BookmarkMergedSurfaceService::ShowMoveStorageDialogCallback>
      move_storage_callback;
  service().SetShowMoveStorageDialogCallbackForTesting(
      move_storage_callback.Get());
  EXPECT_CALL(move_storage_callback, Run(testing::_, node, destination, 1))
      .WillOnce(testing::Invoke(
          this, &BookmarkMergedSurfaceServiceTest::PerformMoveAction));

  service().Move(node, BookmarkParentFolder::FromFolderNode(destination), 1,
                 /*browser=*/nullptr);

  EXPECT_EQ(ModelStringFromNode(model().bookmark_bar_node()), "2 3 ");
  EXPECT_EQ(ModelStringFromNode(model().account_bookmark_bar_node()),
            "f1:[ A1 1 A2 ] ");
}

TEST_F(BookmarkMergedSurfaceServiceTest, CopyBookmarkNodeData) {
  LoadBookmarkModel();
  AddNodesFromModelString(&model(), model().bookmark_bar_node(),
                          "1 2 3 f1:[ 4 5 ] ");
  AddNodesFromModelString(&model(), model().other_node(), "6 7 8 f2:[ 9 ] ");

  // Copy node "f1" to "f2".
  const BookmarkNode* node_to_copy =
      model().bookmark_bar_node()->children()[3].get();
  const bookmarks::BookmarkNodeData::Element node_data(node_to_copy);
  const BookmarkNode* destination = model().other_node()->children()[3].get();
  service().AddNodesAsCopiesOfNodeData(
      {node_data}, BookmarkParentFolder::FromFolderNode(destination), 1);

  EXPECT_EQ(ModelStringFromNode(model().bookmark_bar_node()),
            "1 2 3 f1:[ 4 5 ] ");
  EXPECT_EQ(ModelStringFromNode(model().other_node()),
            "6 7 8 f2:[ 9 f1:[ 4 5 ] ] ");
}

TEST_F(BookmarkMergedSurfaceServiceTest, CopyBookmarkNodeDataMultipleNodes) {
  LoadBookmarkModel();
  AddNodesFromModelString(&model(), model().bookmark_bar_node(),
                          "1 2 3 f1:[ 4 5 ] ");
  AddNodesFromModelString(&model(), model().other_node(), "6 7 8 f2:[ 9 ] ");

  // Copy nodes "2" and "3" to "f2".
  const BookmarkNode* n1 = model().bookmark_bar_node()->children()[1].get();
  const BookmarkNode* n2 = model().bookmark_bar_node()->children()[2].get();
  std::vector<bookmarks::BookmarkNodeData::Element> nodes_data;
  nodes_data.emplace_back(n1);
  nodes_data.emplace_back(n2);

  const BookmarkNode* destination = model().other_node()->children()[3].get();
  service().AddNodesAsCopiesOfNodeData(
      nodes_data, BookmarkParentFolder::FromFolderNode(destination), 0);

  EXPECT_EQ(ModelStringFromNode(model().bookmark_bar_node()),
            "1 2 3 f1:[ 4 5 ] ");
  EXPECT_EQ(ModelStringFromNode(model().other_node()), "6 7 8 f2:[ 2 3 9 ] ");
}

TEST_F(BookmarkMergedSurfaceServiceTest, CopyBookmarkNodeToPermanentFolder) {
  LoadBookmarkModel();
  AddNodesFromModelString(&model(), model().bookmark_bar_node(), "1 2 3 ");
  AddNodesFromModelString(&model(), model().other_node(), "6 7 8 f2:[ 9 ] ");

  // Copy node "7" and "8" to bookmark bar.
  const BookmarkNode* n1 = model().other_node()->children()[1].get();
  const BookmarkNode* n2 = model().other_node()->children()[2].get();
  std::vector<bookmarks::BookmarkNodeData::Element> nodes_data;
  nodes_data.emplace_back(n1);
  nodes_data.emplace_back(n2);

  const BookmarkParentFolder bb_folder(
      BookmarkParentFolder::BookmarkBarFolder());
  service().AddNodesAsCopiesOfNodeData(nodes_data, bb_folder, 1);

  EXPECT_EQ(ModelStringFromNode(model().bookmark_bar_node()), "1 7 8 2 3 ");
  EXPECT_EQ(service().GetNodeAtIndex(bb_folder, 1),
            model().bookmark_bar_node()->children()[1].get());
  EXPECT_EQ(service().GetNodeAtIndex(bb_folder, 2),
            model().bookmark_bar_node()->children()[2].get());

  // New nodes are added to the account node.
  model().CreateAccountPermanentFolders();
  nodes_data.clear();
  nodes_data.emplace_back(model().other_node()->children()[0].get());
  nodes_data.emplace_back(model().other_node()->children()[3].get());
  // Copy node "6" and "F2" to bookmark bar.
  service().AddNodesAsCopiesOfNodeData(nodes_data, bb_folder, 2);
  EXPECT_EQ(ModelStringFromNode(model().account_bookmark_bar_node()),
            "6 f2:[ 9 ] ");
  EXPECT_EQ(service().GetNodeAtIndex(bb_folder, 2),
            model().account_bookmark_bar_node()->children()[0].get());
  EXPECT_EQ(service().GetNodeAtIndex(bb_folder, 3),
            model().account_bookmark_bar_node()->children()[1].get());
}

TEST_F(BookmarkMergedSurfaceServiceTest,
       GetUnderlyingNodesForNonPermanentNode) {
  LoadBookmarkModel();
  AddNodesFromModelString(&model(), model().bookmark_bar_node(),
                          "1 2 3 f1:[ 4 5 ] ");
  const BookmarkNode* node = model().bookmark_bar_node()->children()[3].get();
  BookmarkParentFolder folder = BookmarkParentFolder::FromFolderNode(node);
  EXPECT_THAT(service().GetUnderlyingNodes(folder), UnorderedElementsAre(node));
}

TEST_F(BookmarkMergedSurfaceServiceTest,
       GetUnderlyingNodesManagedPermanentNode) {
  LoadBookmarkModelWithManaged(/*managed_bookmarks_size=*/2);
  {
    BookmarkParentFolder folder = BookmarkParentFolder::ManagedFolder();
    EXPECT_THAT(service().GetUnderlyingNodes(folder),
                UnorderedElementsAre(managed_node()));
  }

  {
    // Non permanent managed node.
    const BookmarkNode* node = managed_node()->children()[0].get();
    BookmarkParentFolder folder = BookmarkParentFolder::FromFolderNode(node);
    EXPECT_THAT(service().GetUnderlyingNodes(folder),
                UnorderedElementsAre(node));
  }
}

TEST_F(BookmarkMergedSurfaceServiceTest, GetUnderlyingNodesPermanentNode) {
  LoadBookmarkModel();
  {
    BookmarkParentFolder folder = BookmarkParentFolder::BookmarkBarFolder();
    EXPECT_THAT(service().GetUnderlyingNodes(folder),
                UnorderedElementsAre(model().bookmark_bar_node()));
  }
  {
    BookmarkParentFolder folder = BookmarkParentFolder::OtherFolder();
    EXPECT_THAT(service().GetUnderlyingNodes(folder),
                UnorderedElementsAre(model().other_node()));
  }
  {
    BookmarkParentFolder folder = BookmarkParentFolder::MobileFolder();
    EXPECT_THAT(service().GetUnderlyingNodes(folder),
                UnorderedElementsAre(model().mobile_node()));
  }
}

TEST_F(BookmarkMergedSurfaceServiceTest,
       GetDefaultParentForNewNodesForNonPermanentNode) {
  LoadBookmarkModel();
  AddNodesFromModelString(&model(), model().bookmark_bar_node(),
                          "1 2 3 f1:[ 4 5 ] ");
  const BookmarkNode* node = model().bookmark_bar_node()->children()[3].get();
  BookmarkParentFolder folder = BookmarkParentFolder::FromFolderNode(node);
  EXPECT_EQ(service().GetDefaultParentForNewNodes(folder), node);
}

TEST_F(BookmarkMergedSurfaceServiceTest,
       GetDefaultParentForNewNodesForPermanentNode) {
  LoadBookmarkModel();
  BookmarkParentFolder folder = BookmarkParentFolder::BookmarkBarFolder();
  EXPECT_EQ(service().GetDefaultParentForNewNodes(folder),
            model().bookmark_bar_node());

  model().CreateAccountPermanentFolders();
  EXPECT_EQ(service().GetDefaultParentForNewNodes(folder),
            model().account_bookmark_bar_node());
}

// Tests for `BookmarkParentFolder`

TEST(BookmarkParentFolderTest, FromPermanentFolderType) {
  {
    BookmarkParentFolder folder = BookmarkParentFolder::BookmarkBarFolder();
    EXPECT_FALSE(folder.HoldsNonPermanentFolder());
    EXPECT_FALSE(folder.as_non_permanent_folder());

    ASSERT_TRUE(folder.as_permanent_folder());
    EXPECT_EQ(*folder.as_permanent_folder(),
              PermanentFolderType::kBookmarkBarNode);
  }
  {
    BookmarkParentFolder folder = BookmarkParentFolder::OtherFolder();
    EXPECT_FALSE(folder.HoldsNonPermanentFolder());
    EXPECT_FALSE(folder.as_non_permanent_folder());

    ASSERT_TRUE(folder.as_permanent_folder());
    EXPECT_EQ(*folder.as_permanent_folder(), PermanentFolderType::kOtherNode);
  }
  {
    BookmarkParentFolder folder = BookmarkParentFolder::MobileFolder();
    EXPECT_FALSE(folder.HoldsNonPermanentFolder());
    EXPECT_FALSE(folder.as_non_permanent_folder());

    ASSERT_TRUE(folder.as_permanent_folder());
    EXPECT_EQ(*folder.as_permanent_folder(), PermanentFolderType::kMobileNode);
  }
  {
    BookmarkParentFolder folder = BookmarkParentFolder::ManagedFolder();
    EXPECT_FALSE(folder.HoldsNonPermanentFolder());
    EXPECT_FALSE(folder.as_non_permanent_folder());

    ASSERT_TRUE(folder.as_permanent_folder());
    EXPECT_EQ(*folder.as_permanent_folder(), PermanentFolderType::kManagedNode);
  }
}

TEST(BookmarkParentFolderTest, FromFolderNode) {
  base::test::ScopedFeatureList features{
      syncer::kSyncEnableBookmarksInTransportMode};
  auto client = std::make_unique<bookmarks::TestBookmarkClient>();
  bookmarks::BookmarkNode* managed_node = client->EnableManagedNode();
  std::unique_ptr<bookmarks::BookmarkModel> model =
      bookmarks::TestBookmarkClient::CreateModelWithClient(std::move(client));
  model->CreateAccountPermanentFolders();
  const BookmarkNode* folder_node =
      model->AddFolder(model->bookmark_bar_node(), 0, u"folder");

  EXPECT_EQ(BookmarkParentFolder::FromFolderNode(model->bookmark_bar_node()),
            BookmarkParentFolder::BookmarkBarFolder());
  EXPECT_EQ(BookmarkParentFolder::FromFolderNode(model->other_node()),
            BookmarkParentFolder::OtherFolder());
  EXPECT_EQ(BookmarkParentFolder::FromFolderNode(model->mobile_node()),
            BookmarkParentFolder::MobileFolder());
  EXPECT_EQ(BookmarkParentFolder::FromFolderNode(managed_node),
            BookmarkParentFolder::ManagedFolder());

  EXPECT_EQ(
      BookmarkParentFolder::FromFolderNode(model->account_bookmark_bar_node()),
      BookmarkParentFolder::BookmarkBarFolder());
  EXPECT_EQ(BookmarkParentFolder::FromFolderNode(model->account_other_node()),
            BookmarkParentFolder::OtherFolder());
  EXPECT_EQ(BookmarkParentFolder::FromFolderNode(model->account_mobile_node()),
            BookmarkParentFolder::MobileFolder());

  BookmarkParentFolder folder =
      BookmarkParentFolder::FromFolderNode(folder_node);
  EXPECT_TRUE(folder.HoldsNonPermanentFolder());
  EXPECT_EQ(folder.as_non_permanent_folder(), folder_node);
  EXPECT_FALSE(folder.as_permanent_folder());
}

TEST(BookmarkParentFolderTest, HasDirectChildNode) {
  std::unique_ptr<bookmarks::BookmarkModel> model =
      std::make_unique<bookmarks::BookmarkModel>(
          std::make_unique<bookmarks::TestBookmarkClient>());
  model->LoadEmptyForTest();
  AddNodesFromModelString(model.get(), model->bookmark_bar_node(),
                          "1 2 3 f1:[ 4 5 ] ");
  AddNodesFromModelString(model.get(), model->other_node(), "6 7 8 ");

  const BookmarkNode* f1 = model->bookmark_bar_node()->children()[3].get();
  {
    BookmarkParentFolder folder = BookmarkParentFolder::BookmarkBarFolder();
    for (const auto& node : model->bookmark_bar_node()->children()) {
      EXPECT_TRUE(folder.HasDirectChildNode(node.get()));
    }
    EXPECT_FALSE(folder.HasDirectChildNode(f1->children()[0].get()));
    EXPECT_FALSE(folder.HasDirectChildNode(model->other_node()));
    EXPECT_FALSE(
        folder.HasDirectChildNode(model->other_node()->children()[1].get()));
  }

  {
    BookmarkParentFolder folder = BookmarkParentFolder::FromFolderNode(f1);
    for (const auto& node : f1->children()) {
      EXPECT_TRUE(folder.HasDirectChildNode(node.get()));
    }
    EXPECT_FALSE(folder.HasDirectChildNode(model->bookmark_bar_node()));
    EXPECT_FALSE(folder.HasDirectChildNode(model->other_node()));
    EXPECT_FALSE(folder.HasDirectChildNode(f1));
  }
}

}  // namespace
