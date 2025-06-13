// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/bookmarks/bookmark_merged_surface_service.h"

#include <memory>

#include "base/containers/to_vector.h"
#include "base/functional/bind.h"
#include "base/scoped_observation.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_ostream_operators.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "base/values.h"
#include "chrome/browser/bookmarks/bookmark_merged_surface_service_observer.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/bookmarks/bookmark_parent_folder_children.h"
#include "chrome/browser/bookmarks/bookmark_test_utils.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_node.h"
#include "components/bookmarks/managed/managed_bookmark_service.h"
#include "components/bookmarks/test/bookmark_test_helpers.h"
#include "components/bookmarks/test/test_bookmark_client.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace {

using PermanentFolderType = BookmarkParentFolder::PermanentFolderType;
using bookmarks::BookmarkNode;
using bookmarks::test::AddNodesFromModelString;
using bookmarks::test::ModelStringFromNode;
using testing::_;
using testing::InSequence;
using testing::Mock;
using testing::Pair;
using testing::UnorderedElementsAre;

// Matcher for comparing `BookmarkParentFolderChildren` children to a vector of
// BookmarkNodes.
MATCHER_P(HasOrderedChildren, expected_children, "") {
  if (arg.size() != expected_children.size()) {
    *result_listener << "Expected " << expected_children.size() << " children, "
                     << "but got " << arg.size();
    return false;
  }

  for (size_t i = 0; i < arg.size(); ++i) {
    const BookmarkNode* child = arg[i];
    const BookmarkNode* expected_child = expected_children[i];
    if (child != expected_child) {
      *result_listener << "Child at index " << i << " does not match. Expected "
                       << expected_child->GetTitledUrlNodeTitle()
                       << ", but got " << child->GetTitledUrlNodeTitle();
      return false;
    }
  }

  return true;
}

MATCHER_P(HasParent, parent, "") {
  const BookmarkNode* node = arg.as_non_permanent_folder();
  if (!node) {
    // `parent` can't be root.
    return false;
  }
  CHECK(node->parent());
  return BookmarkParentFolder::FromFolderNode(node->parent()) == parent;
}

class MockBookmarkMergedSurfaceServiceObserver
    : public BookmarkMergedSurfaceServiceObserver {
 public:
  MOCK_METHOD(void, BookmarkMergedSurfaceServiceLoaded, ());

  MOCK_METHOD(void, BookmarkMergedSurfaceServiceBeingDeleted, ());

  MOCK_METHOD(void,
              BookmarkNodeAdded,
              (const BookmarkParentFolder& parent, size_t index));

  MOCK_METHOD(void,
              BookmarkNodesRemoved,
              (const BookmarkParentFolder&,
               (const base::flat_set<const BookmarkNode*>&)));

  MOCK_METHOD(void,
              BookmarkNodeMoved,
              (const BookmarkParentFolder&,
               size_t,
               const BookmarkParentFolder&,
               size_t));

  MOCK_METHOD(void, BookmarkNodeChanged, (const bookmarks::BookmarkNode*));

  MOCK_METHOD(void,
              BookmarkNodeFaviconChanged,
              (const bookmarks::BookmarkNode*));

  MOCK_METHOD(void,
              BookmarkParentFolderChildrenReordered,
              (const BookmarkParentFolder&));

  MOCK_METHOD(void, BookmarkAllUserNodesRemoved, ());
};

class BookmarkMergedSurfaceServiceTest : public testing::Test {
 public:
  void CreateBookmarkMergedSurfaceServiceWithManaged(
      size_t managed_bookmarks_size) {
    CreateBookmarkMergedSurfaceService(true, managed_bookmarks_size);
  }

  void CreateBookmarkMergedSurfaceService(bool with_managed_node = false,
                                          size_t managed_bookmarks_size = 0) {
    std::unique_ptr<bookmarks::TestBookmarkClient> bookmark_client;
    if (with_managed_node) {
      CHECK(managed_bookmarks_size);
      managed_bookmark_service_ =
          CreateManagedBookmarkService(&prefs_, managed_bookmarks_size);
      bookmark_client = std::make_unique<TestBookmarkClientWithManagedService>(
          managed_bookmark_service_.get());
    } else {
      bookmark_client = std::make_unique<bookmarks::TestBookmarkClient>();
    }
    model_ =
        std::make_unique<bookmarks::BookmarkModel>(std::move(bookmark_client));
    service_ = std::make_unique<BookmarkMergedSurfaceService>(
        model_.get(), managed_bookmark_service_.get());
    service_->AddObserver(&mock_service_observer_);
    model_->LoadEmptyForTest();
    service_->LoadForTesting({});
  }

  ~BookmarkMergedSurfaceServiceTest() override {
    service_->RemoveObserver(&mock_service_observer_);
  }

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

  MockBookmarkMergedSurfaceServiceObserver& mock_service_observer() {
    return mock_service_observer_;
  }

 private:
  base::test::ScopedFeatureList features_{
      switches::kSyncEnableBookmarksInTransportMode};
  sync_preferences::TestingPrefServiceSyncable prefs_;
  std::unique_ptr<bookmarks::ManagedBookmarkService> managed_bookmark_service_;
  std::unique_ptr<bookmarks::BookmarkModel> model_;
  std::unique_ptr<BookmarkMergedSurfaceService> service_;
  testing::NiceMock<MockBookmarkMergedSurfaceServiceObserver>
      mock_service_observer_;
};

TEST_F(BookmarkMergedSurfaceServiceTest, IsNonDefaultOrderingTracked) {
  const size_t kManagedBookmarksSize = 5;
  CreateBookmarkMergedSurfaceServiceWithManaged(kManagedBookmarksSize);
  EXPECT_FALSE(service().IsNonDefaultOrderingTracked(
      BookmarkParentFolder::ManagedFolder()));

  AddNodesFromModelString(&model(), model().bookmark_bar_node(),
                          "1 2 3 f1:[ 4 5 ] ");

  // Non permanent node.
  BookmarkParentFolder non_permanent_folder(
      BookmarkParentFolder::FromFolderNode(
          model().bookmark_bar_node()->children()[3].get()));
  EXPECT_FALSE(service().IsNonDefaultOrderingTracked(non_permanent_folder));

  // No account nodes.
  EXPECT_FALSE(service().IsNonDefaultOrderingTracked(
      BookmarkParentFolder::BookmarkBarFolder()));

  // Account nodes with empty child nodes.
  model().CreateAccountPermanentFolders();
  EXPECT_FALSE(service().IsNonDefaultOrderingTracked(
      BookmarkParentFolder::BookmarkBarFolder()));

  // Default order.
  AddNodesFromModelString(&model(), model().account_bookmark_bar_node(),
                          "4 5 ");
  EXPECT_FALSE(service().IsNonDefaultOrderingTracked(
      BookmarkParentFolder::BookmarkBarFolder()));

  // Custom order.
  service().Move(model().bookmark_bar_node()->children()[0].get(),
                 BookmarkParentFolder::BookmarkBarFolder(), 0u,
                 /*browser=*/nullptr);
  EXPECT_TRUE(service().IsNonDefaultOrderingTracked(
      BookmarkParentFolder::BookmarkBarFolder()));
}

TEST_F(BookmarkMergedSurfaceServiceTest, GetChildrenCount) {
  const size_t kManagedBookmarksSize = 5;
  CreateBookmarkMergedSurfaceServiceWithManaged(kManagedBookmarksSize);
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
  CreateBookmarkMergedSurfaceService();
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
  CreateBookmarkMergedSurfaceService();
  EXPECT_EQ(service().GetChildrenCount(BookmarkParentFolder::ManagedFolder()),
            0u);
}

TEST_F(BookmarkMergedSurfaceServiceTest, GetIndexOf) {
  CreateBookmarkMergedSurfaceServiceWithManaged(/*managed_bookmarks_size=*/3);
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
  CreateBookmarkMergedSurfaceService();
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
  CreateBookmarkMergedSurfaceServiceWithManaged(2);
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
  CreateBookmarkMergedSurfaceService();
  AddNodesFromModelString(&model(), model().bookmark_bar_node(), "f1:[ 4 5 ]");
  EXPECT_FALSE(
      service().IsParentFolderManaged(BookmarkParentFolder::FromFolderNode(
          model().bookmark_bar_node()->children()[0].get())));
}

TEST_F(BookmarkMergedSurfaceServiceTest, MoveToPermanentFolder) {
  CreateBookmarkMergedSurfaceService();
  AddNodesFromModelString(&model(), model().bookmark_bar_node(), "1 2 3 ");
  AddNodesFromModelString(&model(), model().other_node(), "4 5 6 ");

  EXPECT_CALL(
      mock_service_observer(),
      BookmarkNodeMoved(
          /*old_parent=*/BookmarkParentFolder::BookmarkBarFolder(),
          /*old_index=*/1u, /*new_parent=*/BookmarkParentFolder::OtherFolder(),
          /*new_index=*/1u));
  // Move node "2" in bookmark bar to be after "4" in other node.
  const BookmarkNode* node = model().bookmark_bar_node()->children()[1].get();
  service().Move(node, BookmarkParentFolder::OtherFolder(), 1,
                 /*browser=*/nullptr);
  EXPECT_EQ(ModelStringFromNode(model().bookmark_bar_node()), "1 3 ");
  EXPECT_EQ(ModelStringFromNode(model().other_node()), "4 2 5 6 ");
}

TEST_F(BookmarkMergedSurfaceServiceTest,
       MoveToPermanentFolderWithAccountNodes) {
  CreateBookmarkMergedSurfaceService();
  model().CreateAccountPermanentFolders();

  AddNodesFromModelString(&model(), model().bookmark_bar_node(), "1 2 3 ");
  AddNodesFromModelString(&model(), model().other_node(), "4 5 6 ");

  AddNodesFromModelString(&model(), model().account_bookmark_bar_node(),
                          "A1 A2 A3 ");
  AddNodesFromModelString(&model(), model().account_other_node(), "A4 A5 A6 ");

  InSequence s;
  // Move from local bookmark bar to local other node.
  // Move node "2" in bookmark bar to be after "4" in other node.
  const BookmarkNode* node = model().bookmark_bar_node()->children()[1].get();
  EXPECT_CALL(
      mock_service_observer(),
      BookmarkNodeMoved(
          /*old_parent=*/BookmarkParentFolder::BookmarkBarFolder(),
          /*old_index=*/4u, /*new_parent=*/BookmarkParentFolder::OtherFolder(),
          /*new_index=*/1u));
  service().Move(node, BookmarkParentFolder::OtherFolder(), 1,
                 /*browser=*/nullptr);
  EXPECT_EQ(ModelStringFromNode(model().bookmark_bar_node()), "1 3 ");
  EXPECT_EQ(ModelStringFromNode(model().other_node()), "2 4 5 6 ");
  EXPECT_EQ(service().GetIndexOf(node), 1u);

  // Move within bookmark bar.
  EXPECT_CALL(mock_service_observer(),
              BookmarkNodeMoved(
                  /*old_parent=*/BookmarkParentFolder::BookmarkBarFolder(),
                  /*old_index=*/4u,
                  /*new_parent=*/BookmarkParentFolder::BookmarkBarFolder(),
                  /*new_index=*/1u));
  node = model().bookmark_bar_node()->children()[1].get();
  service().Move(node, BookmarkParentFolder::BookmarkBarFolder(), 1,
                 /*browser=*/nullptr);
  EXPECT_EQ(ModelStringFromNode(model().bookmark_bar_node()), "3 1 ");
  EXPECT_EQ(ModelStringFromNode(model().account_bookmark_bar_node()),
            "A1 A2 A3 ");
  // Expected order: "A1 3 A2 A3 1 "
  EXPECT_EQ(service().GetIndexOf(node), 1u);

  // Move from account other node to account bookmark bar.
  EXPECT_CALL(
      mock_service_observer(),
      BookmarkNodeMoved(
          /*old_parent=*/BookmarkParentFolder::OtherFolder(), /*old_index=*/0u,
          /*new_parent=*/BookmarkParentFolder::BookmarkBarFolder(),
          /*new_index=*/2u));
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

TEST_F(BookmarkMergedSurfaceServiceTest, MoveIsNoOpWithAccountNodes) {
  CreateBookmarkMergedSurfaceService();
  model().CreateAccountPermanentFolders();

  AddNodesFromModelString(&model(), model().bookmark_bar_node(), "1 2 3 ");
  AddNodesFromModelString(&model(), model().account_bookmark_bar_node(),
                          "A1 A2 A3 ");

  EXPECT_CALL(mock_service_observer(), BookmarkNodeMoved(_, _, _, _)).Times(0);

  const BookmarkParentFolder bb_folder(
      BookmarkParentFolder::BookmarkBarFolder());
  service().Move(service().GetNodeAtIndex(bb_folder, 0), bb_folder, 0,
                 /*browser=*/nullptr);
  service().Move(service().GetNodeAtIndex(bb_folder, 0), bb_folder, 1,
                 /*browser=*/nullptr);
}

TEST_F(BookmarkMergedSurfaceServiceTest, MoveIsNoOpWithoutAccountNodes) {
  CreateBookmarkMergedSurfaceService();

  AddNodesFromModelString(&model(), model().bookmark_bar_node(), "1 2 3 ");
  EXPECT_CALL(mock_service_observer(), BookmarkNodeMoved(_, _, _, _)).Times(0);

  const BookmarkParentFolder bb_folder(
      BookmarkParentFolder::BookmarkBarFolder());
  service().Move(service().GetNodeAtIndex(bb_folder, 1), bb_folder, 1,
                 /*browser=*/nullptr);
  service().Move(service().GetNodeAtIndex(bb_folder, 1), bb_folder, 2,
                 /*browser=*/nullptr);
}

TEST_F(BookmarkMergedSurfaceServiceTest, MoveWithinBookmarkModelOnlyRequired) {
  CreateBookmarkMergedSurfaceService();
  model().CreateAccountPermanentFolders();

  AddNodesFromModelString(&model(), model().bookmark_bar_node(), "1 2 3 ");
  AddNodesFromModelString(&model(), model().account_bookmark_bar_node(),
                          "A1 A2 A3 ");

  const BookmarkParentFolder bb_folder(
      BookmarkParentFolder::BookmarkBarFolder());
  EXPECT_CALL(mock_service_observer(),
              BookmarkNodeMoved(/*old_parent=*/bb_folder, /*old_index=*/0,
                                /*new_parent=*/bb_folder, /*new_index=*/1));
  EXPECT_CALL(mock_service_observer(),
              BookmarkNodeMoved(/*old_parent=*/bb_folder, /*old_index=*/2,
                                /*new_parent=*/bb_folder, /*new_index=*/0));

  service().Move(service().GetNodeAtIndex(bb_folder, 0), bb_folder, 2,
                 /*browser=*/nullptr);
  service().Move(service().GetNodeAtIndex(bb_folder, 2), bb_folder, 0,
                 /*browser=*/nullptr);
}

TEST_F(BookmarkMergedSurfaceServiceTest, ScopedMoveChangeIsReset) {
  CreateBookmarkMergedSurfaceService();
  model().CreateAccountPermanentFolders();

  AddNodesFromModelString(&model(), model().bookmark_bar_node(), "1 2 3 ");
  AddNodesFromModelString(&model(), model().account_bookmark_bar_node(),
                          "A1 A2 A3 ");

  const BookmarkParentFolder bb_folder(
      BookmarkParentFolder::BookmarkBarFolder());
  EXPECT_CALL(mock_service_observer(),
              BookmarkNodeMoved(/*old_parent=*/bb_folder, /*old_index=*/0,
                                /*new_parent=*/bb_folder, /*new_index=*/1));
  service().Move(service().GetNodeAtIndex(bb_folder, 0), bb_folder, 2,
                 /*browser=*/nullptr);

  // Move in bookmark model.
  EXPECT_CALL(mock_service_observer(),
              BookmarkNodeMoved(/*old_parent=*/bb_folder, /*old_index=*/4,
                                /*new_parent=*/bb_folder, /*new_index=*/5));
  model().Move(model().bookmark_bar_node()->children()[1].get(),
               model().bookmark_bar_node(), 3u);
}

TEST_F(BookmarkMergedSurfaceServiceTest, MoveToBookmarkNode) {
  CreateBookmarkMergedSurfaceService();
  AddNodesFromModelString(&model(), model().bookmark_bar_node(),
                          "1 2 3 f1:[ 4 5 ] ");
  AddNodesFromModelString(&model(), model().other_node(), "6 7 8 ");

  // Move node "8" from other node to node "f1" after "4".
  const BookmarkNode* node_to_move = model().other_node()->children()[2].get();
  const BookmarkParentFolder destination = BookmarkParentFolder::FromFolderNode(
      model().bookmark_bar_node()->children()[3].get());
  EXPECT_CALL(
      mock_service_observer(),
      BookmarkNodeMoved(/*old_parent=*/BookmarkParentFolder::OtherFolder(),
                        /*old_index=*/2u,
                        /*new_parent=*/destination, /*new_index=*/1u));
  service().Move(node_to_move, destination, 1,
                 /*browser=*/nullptr);

  EXPECT_EQ(ModelStringFromNode(model().bookmark_bar_node()),
            "1 2 3 f1:[ 4 8 5 ] ");
  EXPECT_EQ(ModelStringFromNode(model().other_node()), "6 7 ");
}

TEST_F(BookmarkMergedSurfaceServiceTest, MoveFromAccountToLocalStorage) {
  CreateBookmarkMergedSurfaceService();
  model().CreateAccountPermanentFolders();

  AddNodesFromModelString(&model(), model().bookmark_bar_node(), "f1:[ 1 2 ] ");
  AddNodesFromModelString(&model(), model().account_bookmark_bar_node(),
                          "A1 A2 A3 ");

  const BookmarkNode* node =
      model().account_bookmark_bar_node()->children()[0].get();
  const BookmarkNode* destination =
      model().bookmark_bar_node()->children()[0].get();
  const BookmarkParentFolder destination_folder(
      BookmarkParentFolder::FromFolderNode(destination));

  // Bypass the move storage dialog and directly move the bookmark.
  base::MockCallback<
      BookmarkMergedSurfaceService::ShowMoveStorageDialogCallback>
      move_storage_callback;
  service().SetShowMoveStorageDialogCallbackForTesting(
      move_storage_callback.Get());
  EXPECT_CALL(move_storage_callback, Run(testing::_, node, destination, 1))
      .WillOnce(testing::Invoke(
          this, &BookmarkMergedSurfaceServiceTest::PerformMoveAction));
  EXPECT_CALL(mock_service_observer(),
              BookmarkNodeMoved(
                  /*old_parent=*/BookmarkParentFolder::BookmarkBarFolder(),
                  /*old_index=*/0u,
                  /*new_parent=*/destination_folder, /*new_index=*/1u));

  service().Move(node, destination_folder, 1, /*browser=*/nullptr);

  EXPECT_EQ(ModelStringFromNode(model().bookmark_bar_node()), "f1:[ 1 A1 2 ] ");
  EXPECT_EQ(ModelStringFromNode(model().account_bookmark_bar_node()), "A2 A3 ");
}

TEST_F(BookmarkMergedSurfaceServiceTest, MoveFromLocalToAccountStorage) {
  CreateBookmarkMergedSurfaceService();
  model().CreateAccountPermanentFolders();

  AddNodesFromModelString(&model(), model().bookmark_bar_node(), "1 2 3 ");
  AddNodesFromModelString(&model(), model().account_bookmark_bar_node(),
                          "f1:[ A1 A2 ] ");

  const BookmarkNode* node = model().bookmark_bar_node()->children()[0].get();
  const BookmarkNode* destination =
      model().account_bookmark_bar_node()->children()[0].get();
  const BookmarkParentFolder destination_folder(
      BookmarkParentFolder::FromFolderNode(destination));

  // Bypass the move storage dialog and directly move the bookmark.
  base::MockCallback<
      BookmarkMergedSurfaceService::ShowMoveStorageDialogCallback>
      move_storage_callback;
  service().SetShowMoveStorageDialogCallbackForTesting(
      move_storage_callback.Get());
  EXPECT_CALL(move_storage_callback, Run(testing::_, node, destination, 1))
      .WillOnce(testing::Invoke(
          this, &BookmarkMergedSurfaceServiceTest::PerformMoveAction));
  EXPECT_CALL(mock_service_observer(),
              BookmarkNodeMoved(
                  /*old_parent=*/BookmarkParentFolder::BookmarkBarFolder(),
                  /*old_index=*/1u,
                  /*new_parent=*/destination_folder, /*new_index=*/1u));

  service().Move(node, destination_folder, 1, /*browser=*/nullptr);

  EXPECT_EQ(ModelStringFromNode(model().bookmark_bar_node()), "2 3 ");
  EXPECT_EQ(ModelStringFromNode(model().account_bookmark_bar_node()),
            "f1:[ A1 1 A2 ] ");
}

TEST_F(BookmarkMergedSurfaceServiceTest, CopyBookmarkNodeData) {
  CreateBookmarkMergedSurfaceService();
  AddNodesFromModelString(&model(), model().bookmark_bar_node(),
                          "1 2 3 f1:[ 4 5 ] ");
  AddNodesFromModelString(&model(), model().other_node(), "6 7 8 f2:[ 9 ] ");

  // Copy node "f1" to "f2".
  const BookmarkNode* node_to_copy =
      model().bookmark_bar_node()->children()[3].get();
  const bookmarks::BookmarkNodeData::Element node_data(node_to_copy);
  const BookmarkParentFolder destination = BookmarkParentFolder::FromFolderNode(
      model().other_node()->children()[3].get());

  // Register second observer.
  MockBookmarkMergedSurfaceServiceObserver mock_service_observer2;
  base::ScopedObservation<BookmarkMergedSurfaceService,
                          BookmarkMergedSurfaceServiceObserver>
      obs2_{&mock_service_observer2};
  obs2_.Observe(&service());

  InSequence sequence;
  EXPECT_CALL(mock_service_observer(), BookmarkNodeAdded(destination, 1u));
  EXPECT_CALL(mock_service_observer2, BookmarkNodeAdded(destination, 1u));

  EXPECT_CALL(mock_service_observer(),
              BookmarkNodeAdded(HasParent(destination), 0u));
  EXPECT_CALL(mock_service_observer2,
              BookmarkNodeAdded(HasParent(destination), 0u));

  EXPECT_CALL(mock_service_observer(),
              BookmarkNodeAdded(HasParent(destination), 1u));
  EXPECT_CALL(mock_service_observer2,
              BookmarkNodeAdded(HasParent(destination), 1u));

  service().AddNodesAsCopiesOfNodeData({node_data}, destination, 1);

  EXPECT_EQ(ModelStringFromNode(model().bookmark_bar_node()),
            "1 2 3 f1:[ 4 5 ] ");
  EXPECT_EQ(ModelStringFromNode(model().other_node()),
            "6 7 8 f2:[ 9 f1:[ 4 5 ] ] ");
}

TEST_F(BookmarkMergedSurfaceServiceTest, CopyBookmarkNodeDataMultipleNodes) {
  CreateBookmarkMergedSurfaceService();
  AddNodesFromModelString(&model(), model().bookmark_bar_node(),
                          "1 2 3 f1:[ 4 5 ] ");
  AddNodesFromModelString(&model(), model().other_node(), "6 7 8 f2:[ 9 ] ");

  // Copy nodes "2" and "3" to "f2".
  const BookmarkNode* n1 = model().bookmark_bar_node()->children()[1].get();
  const BookmarkNode* n2 = model().bookmark_bar_node()->children()[2].get();
  std::vector<bookmarks::BookmarkNodeData::Element> nodes_data;
  nodes_data.emplace_back(n1);
  nodes_data.emplace_back(n2);

  const BookmarkParentFolder destination = BookmarkParentFolder::FromFolderNode(
      model().other_node()->children()[3].get());

  InSequence sequence;
  EXPECT_CALL(mock_service_observer(), BookmarkNodeAdded(destination, 0u));
  EXPECT_CALL(mock_service_observer(), BookmarkNodeAdded(destination, 1u));

  service().AddNodesAsCopiesOfNodeData(nodes_data, destination, 0);

  EXPECT_EQ(ModelStringFromNode(model().bookmark_bar_node()),
            "1 2 3 f1:[ 4 5 ] ");
  EXPECT_EQ(ModelStringFromNode(model().other_node()), "6 7 8 f2:[ 2 3 9 ] ");
}

TEST_F(BookmarkMergedSurfaceServiceTest, CopyBookmarkNodeToPermanentFolder) {
  CreateBookmarkMergedSurfaceService();
  AddNodesFromModelString(&model(), model().bookmark_bar_node(), "1 2 3 ");
  AddNodesFromModelString(&model(), model().other_node(),
                          "6 7 8 f2:[ 9 f3:[ 1 ] 2 ] ");

  // Copy node "7" and "8" to bookmark bar.
  std::vector<bookmarks::BookmarkNodeData::Element> nodes_data;
  nodes_data.emplace_back(model().other_node()->children()[1].get());
  nodes_data.emplace_back(model().other_node()->children()[2].get());
  const BookmarkParentFolder bb_folder(
      BookmarkParentFolder::BookmarkBarFolder());

  {
    InSequence sequence;
    EXPECT_CALL(mock_service_observer(), BookmarkNodeAdded(bb_folder, 1u));
    EXPECT_CALL(mock_service_observer(), BookmarkNodeAdded(bb_folder, 2u));

    service().AddNodesAsCopiesOfNodeData(nodes_data, bb_folder, 1);

    EXPECT_EQ(ModelStringFromNode(model().bookmark_bar_node()), "1 7 8 2 3 ");
    EXPECT_EQ(service().GetNodeAtIndex(bb_folder, 1),
              model().bookmark_bar_node()->children()[1].get());
    EXPECT_EQ(service().GetNodeAtIndex(bb_folder, 2),
              model().bookmark_bar_node()->children()[2].get());
  }

  // New nodes are added to the account node.
  model().CreateAccountPermanentFolders();
  // Copy node "6" and "F2" to bookmark bar.
  nodes_data.clear();
  nodes_data.emplace_back(model().other_node()->children()[0].get());
  nodes_data.emplace_back(model().other_node()->children()[3].get());
  {
    InSequence sequence;
    // Register second observer.
    MockBookmarkMergedSurfaceServiceObserver mock_service_observer2;
    base::ScopedObservation<BookmarkMergedSurfaceService,
                            BookmarkMergedSurfaceServiceObserver>
        obs2_{&mock_service_observer2};
    obs2_.Observe(&service());

    EXPECT_CALL(mock_service_observer(), BookmarkNodeAdded(bb_folder, 2u));
    EXPECT_CALL(mock_service_observer2, BookmarkNodeAdded(bb_folder, 2u));

    EXPECT_CALL(mock_service_observer(), BookmarkNodeAdded(bb_folder, 3u));
    EXPECT_CALL(mock_service_observer2, BookmarkNodeAdded(bb_folder, 3u));

    EXPECT_CALL(mock_service_observer(),
                BookmarkNodeAdded(HasParent(bb_folder), 0u));
    EXPECT_CALL(mock_service_observer2,
                BookmarkNodeAdded(HasParent(bb_folder), 0u));

    EXPECT_CALL(mock_service_observer(),
                BookmarkNodeAdded(HasParent(bb_folder), 1u));
    EXPECT_CALL(mock_service_observer2,
                BookmarkNodeAdded(HasParent(bb_folder), 1u));

    BookmarkParentFolder obs1_expected_f3 = bb_folder;
    BookmarkParentFolder obs2_expected_f3 = bb_folder;
    EXPECT_CALL(mock_service_observer(), BookmarkNodeAdded(_, 0u))
        .WillOnce(testing::SaveArg<0>(&obs1_expected_f3));
    EXPECT_CALL(mock_service_observer2, BookmarkNodeAdded(_, 0u))
        .WillOnce(testing::SaveArg<0>(&obs2_expected_f3));

    EXPECT_CALL(mock_service_observer(),
                BookmarkNodeAdded(HasParent(bb_folder), 2u));
    EXPECT_CALL(mock_service_observer2,
                BookmarkNodeAdded(HasParent(bb_folder), 2u));

    service().AddNodesAsCopiesOfNodeData(nodes_data, bb_folder, 2);

    EXPECT_EQ(ModelStringFromNode(model().account_bookmark_bar_node()),
              "6 f2:[ 9 f3:[ 1 ] 2 ] ");
    EXPECT_EQ(service().GetNodeAtIndex(bb_folder, 2),
              model().account_bookmark_bar_node()->children()[0].get());
    EXPECT_EQ(service().GetNodeAtIndex(bb_folder, 3),
              model().account_bookmark_bar_node()->children()[1].get());
    EXPECT_EQ(obs1_expected_f3.as_non_permanent_folder(),
              model()
                  .account_bookmark_bar_node()
                  ->children()[1]
                  ->children()[1]
                  .get());
    EXPECT_EQ(obs1_expected_f3, obs2_expected_f3);
  }
}

TEST_F(BookmarkMergedSurfaceServiceTest, ScopedAddNewNodesReset) {
  CreateBookmarkMergedSurfaceService();
  AddNodesFromModelString(&model(), model().bookmark_bar_node(), "1 2 3 ");
  AddNodesFromModelString(&model(), model().other_node(), "6 7 8 f2:[ 9 ] ");

  // Copy node "7" to bookmark bar.
  bookmarks::BookmarkNodeData::Element node_data(
      model().other_node()->children()[1].get());
  const BookmarkParentFolder bb_folder(
      BookmarkParentFolder::BookmarkBarFolder());
  InSequence sequence;
  EXPECT_CALL(mock_service_observer(), BookmarkNodeAdded(bb_folder, 1u));
  service().AddNodesAsCopiesOfNodeData({node_data}, bb_folder, 1);

  EXPECT_EQ(ModelStringFromNode(model().bookmark_bar_node()), "1 7 2 3 ");
  EXPECT_EQ(service().GetNodeAtIndex(bb_folder, 1),
            model().bookmark_bar_node()->children()[1].get());

  // Copy node "1" to "f2".
  const BookmarkNode* node_to_copy =
      model().bookmark_bar_node()->children()[0].get();
  const BookmarkParentFolder destination = BookmarkParentFolder::FromFolderNode(
      model().other_node()->children()[3].get());

  EXPECT_CALL(mock_service_observer(), BookmarkNodeAdded(destination, 1u));

  service().AddNodesAsCopiesOfNodeData(
      {bookmarks::BookmarkNodeData::Element(node_to_copy)}, destination, 1);
  EXPECT_EQ(ModelStringFromNode(model().other_node()), "6 7 8 f2:[ 9 1 ] ");
}

TEST_F(BookmarkMergedSurfaceServiceTest,
       GetUnderlyingNodesForNonPermanentNode) {
  CreateBookmarkMergedSurfaceService();
  AddNodesFromModelString(&model(), model().bookmark_bar_node(),
                          "1 2 3 f1:[ 4 5 ] ");
  const BookmarkNode* node = model().bookmark_bar_node()->children()[3].get();
  BookmarkParentFolder folder = BookmarkParentFolder::FromFolderNode(node);
  EXPECT_THAT(service().GetUnderlyingNodes(folder), UnorderedElementsAre(node));
}

TEST_F(BookmarkMergedSurfaceServiceTest,
       GetUnderlyingNodesManagedPermanentNode) {
  CreateBookmarkMergedSurfaceServiceWithManaged(/*managed_bookmarks_size=*/2);
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
  CreateBookmarkMergedSurfaceService();
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
  CreateBookmarkMergedSurfaceService();
  AddNodesFromModelString(&model(), model().bookmark_bar_node(),
                          "1 2 3 f1:[ 4 5 ] ");
  const BookmarkNode* node = model().bookmark_bar_node()->children()[3].get();
  BookmarkParentFolder folder = BookmarkParentFolder::FromFolderNode(node);
  EXPECT_EQ(service().GetDefaultParentForNewNodes(folder), node);
}

TEST_F(BookmarkMergedSurfaceServiceTest,
       GetDefaultParentForNewNodesForPermanentNode) {
  CreateBookmarkMergedSurfaceService();
  BookmarkParentFolder folder = BookmarkParentFolder::BookmarkBarFolder();
  EXPECT_EQ(service().GetDefaultParentForNewNodes(folder),
            model().bookmark_bar_node());

  model().CreateAccountPermanentFolders();
  EXPECT_EQ(service().GetDefaultParentForNewNodes(folder),
            model().account_bookmark_bar_node());
}

TEST_F(BookmarkMergedSurfaceServiceTest,
       GetParentForManagedNodeForPermanentManagedNode) {
  CreateBookmarkMergedSurfaceServiceWithManaged(/*managed_bookmarks_size=*/3);
  BookmarkParentFolder managed_folder = BookmarkParentFolder::ManagedFolder();
  EXPECT_EQ(service().GetParentForManagedNode(managed_folder), managed_node());
}

TEST_F(BookmarkMergedSurfaceServiceTest,
       GetParentForManagedNodeForNonPermanentNode) {
  CreateBookmarkMergedSurfaceServiceWithManaged(/*managed_bookmarks_size=*/1);
  AddNodesFromModelString(&model(), managed_node(), "f1:[ f11:[ 4 ] 5 ] ");
  // Fetch second node to get the folder since a node was already added at
  // creation.
  const BookmarkNode* managed_folder =
      managed_node()->children()[1].get()->children()[0].get();
  ASSERT_TRUE(managed_folder->is_folder());
  ASSERT_FALSE(managed_folder->parent()->is_permanent_node());
  BookmarkParentFolder folder =
      BookmarkParentFolder::FromFolderNode(managed_folder);
  EXPECT_EQ(service().GetParentForManagedNode(folder), managed_folder);
}

TEST_F(BookmarkMergedSurfaceServiceTest, BookmarkNodeAdded) {
  CreateBookmarkMergedSurfaceService();
  model().CreateAccountPermanentFolders();
  const BookmarkNode* local_bb_node = model().bookmark_bar_node();
  const BookmarkNode* account_bb_node = model().account_bookmark_bar_node();
  ASSERT_TRUE(account_bb_node);

  BookmarkParentFolder bb_folder = BookmarkParentFolder::BookmarkBarFolder();
  const GURL kUrl("http://foo.com");
  EXPECT_CALL(mock_service_observer(), BookmarkNodeAdded(bb_folder, 0));
  const BookmarkNode* new_node = model().AddURL(local_bb_node, 0, u"L1", kUrl);
  EXPECT_EQ(service().GetNodeAtIndex(bb_folder, 0u), new_node);

  // Add node to the account.
  EXPECT_CALL(mock_service_observer(), BookmarkNodeAdded(bb_folder, 0));
  new_node = model().AddURL(account_bb_node, 0, u"A1", kUrl);
  EXPECT_EQ(service().GetNodeAtIndex(bb_folder, 0u), new_node);

  // Add folder to `local_bb_node`.
  EXPECT_CALL(mock_service_observer(), BookmarkNodeAdded(bb_folder, 2u));
  const BookmarkNode* folder_node =
      model().AddFolder(local_bb_node, 1, u"title");
  EXPECT_EQ(service().GetNodeAtIndex(bb_folder, 2u), folder_node);

  // Add another account node.
  EXPECT_CALL(mock_service_observer(), BookmarkNodeAdded(bb_folder, 1));
  new_node = model().AddURL(account_bb_node, 1, u"A2", kUrl);
  EXPECT_EQ(service().GetNodeAtIndex(bb_folder, 1u), new_node);

  EXPECT_THAT(service().GetChildren(bb_folder),
              HasOrderedChildren(std::vector<const BookmarkNode*>{
                  account_bb_node->children()[0].get(),
                  account_bb_node->children()[1].get(),
                  local_bb_node->children()[0].get(),
                  local_bb_node->children()[1].get()}));

  // Add new node to `folder_node`.
  BookmarkParentFolder folder(
      BookmarkParentFolder::FromFolderNode(folder_node));
  EXPECT_CALL(mock_service_observer(), BookmarkNodeAdded(folder, 0));
  new_node = model().AddURL(folder_node, 0, u"1", kUrl);
  EXPECT_EQ(service().GetNodeAtIndex(folder, 0u), new_node);
}

TEST_F(BookmarkMergedSurfaceServiceTest, BookmarkNodeRemovedOrderingTracked) {
  CreateBookmarkMergedSurfaceService();
  model().CreateAccountPermanentFolders();
  const BookmarkNode* local_bb_node = model().bookmark_bar_node();
  const BookmarkNode* account_bb_node = model().account_bookmark_bar_node();
  ASSERT_TRUE(account_bb_node);

  BookmarkParentFolder bb_folder = BookmarkParentFolder::BookmarkBarFolder();
  AddNodesFromModelString(&model(), local_bb_node, "1 2 3 f1:[ 4 5 ] ");
  AddNodesFromModelString(&model(), account_bb_node, "7 8 9 f3:[ 10 11 ] ");

  const auto& local_children = local_bb_node->children();
  const auto& account_children = account_bb_node->children();
  std::vector<const BookmarkNode*> expected_children{
      account_children[0].get(), account_children[1].get(),
      account_children[2].get(), account_children[3].get(),
      local_children[0].get(),   local_children[1].get(),
      local_children[2].get(),   local_children[3].get()};
  EXPECT_THAT(service().GetChildren(bb_folder),
              HasOrderedChildren(expected_children));

  const BookmarkNode* node_to_remove = local_children[2].get();
  size_t across_storage_index = service().GetIndexOf(node_to_remove);
  ASSERT_EQ(across_storage_index, 6u);

  EXPECT_CALL(
      mock_service_observer(),
      BookmarkNodesRemoved(bb_folder, UnorderedElementsAre(node_to_remove)));
  expected_children.erase(expected_children.cbegin() + across_storage_index);
  model().Remove(node_to_remove, bookmarks::metrics::BookmarkEditSource::kOther,
                 FROM_HERE);
  Mock::VerifyAndClearExpectations(&mock_service_observer());
  EXPECT_THAT(service().GetChildren(bb_folder),
              HasOrderedChildren(expected_children));

  node_to_remove = account_children[1].get();
  across_storage_index = service().GetIndexOf(node_to_remove);
  ASSERT_EQ(across_storage_index, 1u);
  EXPECT_CALL(
      mock_service_observer(),
      BookmarkNodesRemoved(bb_folder, UnorderedElementsAre(node_to_remove)));
  expected_children.erase(expected_children.cbegin() + across_storage_index);
  model().Remove(node_to_remove, bookmarks::metrics::BookmarkEditSource::kOther,
                 FROM_HERE);
  EXPECT_THAT(service().GetChildren(bb_folder),
              HasOrderedChildren(expected_children));
}

TEST_F(BookmarkMergedSurfaceServiceTest, BookmarkNodeRemovedCustomOrder) {
  CreateBookmarkMergedSurfaceService();
  model().CreateAccountPermanentFolders();
  const BookmarkNode* local_bb_node = model().bookmark_bar_node();
  const BookmarkNode* account_bb_node = model().account_bookmark_bar_node();
  ASSERT_TRUE(account_bb_node);

  BookmarkParentFolder bb_folder = BookmarkParentFolder::BookmarkBarFolder();
  AddNodesFromModelString(&model(), local_bb_node, "1 2 3 f1:[ 4 5 ] ");
  AddNodesFromModelString(&model(), account_bb_node, "7 8 9 f3:[ 10 11 ] ");
  const auto& local_children = local_bb_node->children();
  const auto& account_children = account_bb_node->children();
  std::vector<const BookmarkNode*> expected_children{
      account_children[0].get(), account_children[1].get(),
      local_children[0].get(),   local_children[1].get(),
      account_children[2].get(), local_children[2].get(),
      account_children[3].get(), local_children[3].get()};
  for (size_t i = 0; i < expected_children.size(); i++) {
    service().Move(expected_children[i], bb_folder, i, /*browser=*/nullptr);
  }
  EXPECT_THAT(service().GetChildren(bb_folder),
              HasOrderedChildren(expected_children));

  const size_t index_node_to_remove = 1u;
  EXPECT_CALL(mock_service_observer(),
              BookmarkNodesRemoved(
                  bb_folder, UnorderedElementsAre(account_children[1].get())));
  expected_children.erase(expected_children.cbegin() + index_node_to_remove);
  model().Remove(account_children[1].get(),
                 bookmarks::metrics::BookmarkEditSource::kOther, FROM_HERE);
  EXPECT_THAT(service().GetChildren(bb_folder),
              HasOrderedChildren(expected_children));
}

TEST_F(BookmarkMergedSurfaceServiceTest, BookmarkNodeRemovedNonTrackedNode) {
  CreateBookmarkMergedSurfaceService();
  AddNodesFromModelString(&model(), model().bookmark_bar_node(),
                          "1 2 3 f1:[ 4 5 ] ");

  // Remove node "4".
  const size_t index = 0;
  const BookmarkNode* parent_node =
      model().bookmark_bar_node()->children()[3].get();
  const BookmarkNode* node_to_remove = parent_node->children()[index].get();

  EXPECT_CALL(
      mock_service_observer(),
      BookmarkNodesRemoved(BookmarkParentFolder::FromFolderNode(parent_node),
                           UnorderedElementsAre(node_to_remove)));
  model().Remove(node_to_remove, bookmarks::metrics::BookmarkEditSource::kOther,
                 FROM_HERE);
}

TEST_F(BookmarkMergedSurfaceServiceTest,
       BookmarkNodeRemovedAccountNodeWithChildNodes) {
  CreateBookmarkMergedSurfaceService();
  model().CreateAccountPermanentFolders();

  BookmarkParentFolder bb_folder = BookmarkParentFolder::BookmarkBarFolder();
  AddNodesFromModelString(&model(), model().bookmark_bar_node(),
                          "1 2 3 f1:[ 4 5 ] ");
  AddNodesFromModelString(&model(), model().account_bookmark_bar_node(),
                          "7 8 9 f3:[ 10 11 ] ");
  const auto& account_child_nodes =
      model().account_bookmark_bar_node()->children();
  EXPECT_CALL(
      mock_service_observer(),
      BookmarkNodesRemoved(bb_folder,
                           UnorderedElementsAre(account_child_nodes[0].get(),
                                                account_child_nodes[1].get(),
                                                account_child_nodes[2].get(),
                                                account_child_nodes[3].get())));
  model().RemoveAccountPermanentFolders();
}

TEST_F(BookmarkMergedSurfaceServiceTest,
       BookmarkNodeRemovedAccountNodeCustomOrder) {
  CreateBookmarkMergedSurfaceService();
  model().CreateAccountPermanentFolders();
  BookmarkParentFolder bb_folder = BookmarkParentFolder::BookmarkBarFolder();
  AddNodesFromModelString(&model(), model().bookmark_bar_node(),
                          "1 2 3 f1:[ 4 5 ] ");
  AddNodesFromModelString(&model(), model().account_bookmark_bar_node(),
                          "7 8 9 f3:[ 10 11 ] ");
  const auto& local_children = model().bookmark_bar_node()->children();
  const auto& account_children =
      model().account_bookmark_bar_node()->children();
  std::vector<const BookmarkNode*> expected_children{
      account_children[0].get(), account_children[1].get(),
      local_children[0].get(),   local_children[1].get(),
      account_children[2].get(), local_children[2].get(),
      account_children[3].get(), local_children[3].get()};
  for (size_t i = 0; i < expected_children.size(); i++) {
    service().Move(expected_children[i], bb_folder, i, /*browser=*/nullptr);
  }
  EXPECT_THAT(service().GetChildren(bb_folder),
              HasOrderedChildren(expected_children));
  EXPECT_CALL(mock_service_observer(),
              BookmarkNodesRemoved(
                  bb_folder, UnorderedElementsAre(
                                 expected_children[0], expected_children[1],
                                 expected_children[4], expected_children[6])));

  expected_children.clear();
  model().RemoveAccountPermanentFolders();

  EXPECT_THAT(service().GetChildren(bb_folder),
              HasOrderedChildren(std::vector<const BookmarkNode*>{
                  local_children[0].get(), local_children[1].get(),
                  local_children[2].get(), local_children[3].get()}));
}

TEST_F(BookmarkMergedSurfaceServiceTest,
       BookmarkNodeRemovedAccountNodeWithNoChildren) {
  CreateBookmarkMergedSurfaceService();
  model().CreateAccountPermanentFolders();
  BookmarkParentFolder bb_folder = BookmarkParentFolder::BookmarkBarFolder();
  AddNodesFromModelString(&model(), model().bookmark_bar_node(),
                          "1 2 3 f1:[ 4 5 ] ");
  EXPECT_CALL(mock_service_observer(),
              BookmarkNodesRemoved(testing::_, testing::_))
      .Times(0);

  model().RemoveAccountPermanentFolders();
}

TEST_F(BookmarkMergedSurfaceServiceTest,
       BookmarkNodeRemovedAccountNodeOrderingNotTracked) {
  CreateBookmarkMergedSurfaceService();
  model().CreateAccountPermanentFolders();
  BookmarkParentFolder bb_folder = BookmarkParentFolder::BookmarkBarFolder();
  AddNodesFromModelString(&model(), model().account_bookmark_bar_node(),
                          "1 2 3 ");
  const auto& account_child_nodes =
      model().account_bookmark_bar_node()->children();
  EXPECT_CALL(
      mock_service_observer(),
      BookmarkNodesRemoved(bb_folder,
                           UnorderedElementsAre(account_child_nodes[0].get(),
                                                account_child_nodes[1].get(),
                                                account_child_nodes[2].get())));
  model().RemoveAccountPermanentFolders();
  EXPECT_FALSE(service().GetChildrenCount(bb_folder));
}

TEST_F(BookmarkMergedSurfaceServiceTest,
       BookmarkNodeMovedBetweenPermanentFolders) {
  CreateBookmarkMergedSurfaceService();
  model().CreateAccountPermanentFolders();
  AddNodesFromModelString(&model(), model().bookmark_bar_node(),
                          "1 2 3 f1:[ 4 5 ] ");
  AddNodesFromModelString(&model(), model().account_bookmark_bar_node(),
                          "7 8 9 f3:[ 10 11 ] ");

  AddNodesFromModelString(&model(), model().other_node(), "A B C ");
  AddNodesFromModelString(&model(), model().account_other_node(), "D E F ");

  BookmarkParentFolder bb_folder = BookmarkParentFolder::BookmarkBarFolder();
  const size_t bb_folder_size = service().GetChildrenCount(bb_folder);
  BookmarkParentFolder other_folder = BookmarkParentFolder::OtherFolder();
  const size_t other_folder_size = service().GetChildrenCount(other_folder);

  // Move from bookmark bar `f1` to other node after `B`.
  const BookmarkNode* f1 = model().bookmark_bar_node()->children()[3].get();
  const size_t old_index = service().GetIndexOf(f1);
  ASSERT_EQ(old_index, 7u);

  const size_t expected_new_index = 5u;
  ASSERT_EQ(service().GetIndexOf(model().other_node()->children()[1].get()),
            expected_new_index - 1);
  EXPECT_CALL(
      mock_service_observer(),
      BookmarkNodeMoved(/*old_parent=*/bb_folder, old_index,
                        /*new_parent=*/other_folder, expected_new_index));
  model().Move(f1, model().other_node(), /*index*/ 2u);
  EXPECT_EQ(service().GetChildrenCount(bb_folder), bb_folder_size - 1);
  EXPECT_EQ(service().GetChildrenCount(other_folder), other_folder_size + 1);
  EXPECT_EQ(service().GetIndexOf(f1), expected_new_index);
}

TEST_F(BookmarkMergedSurfaceServiceTest,
       BookmarkNodeMovedWithinSamePermanentFolder) {
  CreateBookmarkMergedSurfaceService();
  model().CreateAccountPermanentFolders();
  AddNodesFromModelString(&model(), model().bookmark_bar_node(),
                          "1 2 3 f1:[ 4 5 ] ");
  AddNodesFromModelString(&model(), model().account_bookmark_bar_node(),
                          "7 8 9 f3:[ 10 11 ] ");

  BookmarkParentFolder bb_folder = BookmarkParentFolder::BookmarkBarFolder();

  // Move `f1` to `account_bookmark_bar_node()` at the end.
  const BookmarkNode* f1 = model().bookmark_bar_node()->children()[3].get();
  size_t old_index = service().GetIndexOf(f1);
  ASSERT_EQ(old_index, 7u);

  size_t expected_new_index = 4u;
  ASSERT_EQ(service().GetIndexOf(
                model().account_bookmark_bar_node()->children()[3].get()),
            expected_new_index - 1);
  EXPECT_CALL(mock_service_observer(),
              BookmarkNodeMoved(/*old_parent=*/bb_folder, old_index,
                                /*new_parent=*/bb_folder, expected_new_index));
  model().Move(f1, model().account_bookmark_bar_node(),
               /*index*/ 4u);
  Mock::VerifyAndClearExpectations(&mock_service_observer());
  EXPECT_EQ(service().GetIndexOf(f1), expected_new_index);

  // Move within same parent folder.
  const BookmarkNode* node =
      model().account_bookmark_bar_node()->children()[1].get();
  old_index = service().GetIndexOf(node);
  expected_new_index = 2u;
  EXPECT_CALL(mock_service_observer(),
              BookmarkNodeMoved(/*old_parent=*/bb_folder, old_index,
                                /*new_parent=*/bb_folder, expected_new_index));
  model().Move(node, model().account_bookmark_bar_node(), 3u);
  EXPECT_EQ(service().GetIndexOf(node), expected_new_index);
}

TEST_F(BookmarkMergedSurfaceServiceTest,
       BookmarkNodeMovedFromPermanentFolderToAFolder) {
  CreateBookmarkMergedSurfaceService();
  model().CreateAccountPermanentFolders();
  AddNodesFromModelString(&model(), model().bookmark_bar_node(),
                          "1 2 3 f1:[ 4 5 ] ");
  AddNodesFromModelString(&model(), model().account_bookmark_bar_node(),
                          "7 8 9 f3:[ 10 11 ] ");

  BookmarkParentFolder bb_folder = BookmarkParentFolder::BookmarkBarFolder();
  const size_t bb_folder_size = service().GetChildrenCount(bb_folder);

  // Move `3` to `account_bookmark_bar_node()` at the end.
  const BookmarkNode* node = model().bookmark_bar_node()->children()[2].get();
  const BookmarkNode* f3 =
      model().account_bookmark_bar_node()->children()[3].get();
  const size_t old_index = service().GetIndexOf(node);
  ASSERT_EQ(old_index, 6u);

  const size_t expected_new_index = 1u;
  EXPECT_CALL(
      mock_service_observer(),
      BookmarkNodeMoved(/*old_parent=*/bb_folder, old_index,
                        /*new_parent=*/BookmarkParentFolder::FromFolderNode(f3),
                        expected_new_index));
  model().Move(node, f3, expected_new_index);
  EXPECT_EQ(service().GetChildrenCount(bb_folder), bb_folder_size - 1);
  EXPECT_EQ(service().GetIndexOf(node), expected_new_index);
}

TEST_F(BookmarkMergedSurfaceServiceTest,
       BookmarkNodeMovedFromFolderToPermanentFolder) {
  CreateBookmarkMergedSurfaceService();
  model().CreateAccountPermanentFolders();
  AddNodesFromModelString(&model(), model().bookmark_bar_node(),
                          "1 2 3 f1:[ 4 5 ] ");
  AddNodesFromModelString(&model(), model().account_bookmark_bar_node(),
                          "7 8 9 f3:[ 10 11 ] ");

  BookmarkParentFolder bb_folder = BookmarkParentFolder::BookmarkBarFolder();
  const size_t bb_folder_size = service().GetChildrenCount(bb_folder);

  // Move `4` from f1 to `account_bookmark_bar_node()` at index 1.
  const BookmarkNode* old_parent =
      model().bookmark_bar_node()->children()[3].get();
  const BookmarkNode* node = old_parent->children()[0].get();
  const size_t old_index = service().GetIndexOf(node);
  ASSERT_EQ(old_index, 0u);

  const size_t expected_new_index = 4u;
  EXPECT_CALL(
      mock_service_observer(),
      BookmarkNodeMoved(
          /*old_parent=*/BookmarkParentFolder::FromFolderNode(old_parent),
          old_index, /*new_parent=*/bb_folder, expected_new_index));
  model().Move(node, model().account_bookmark_bar_node(), expected_new_index);
  EXPECT_EQ(service().GetChildrenCount(bb_folder), bb_folder_size + 1);
  EXPECT_EQ(service().GetIndexOf(node), expected_new_index);
}

TEST_F(BookmarkMergedSurfaceServiceTest, BookmarkNodeChanged) {
  CreateBookmarkMergedSurfaceService();
  AddNodesFromModelString(&model(), model().bookmark_bar_node(),
                          "1 2 3 f1:[ 4 5 ] ");
  const std::u16string kOriginalTitle(u"foo");
  const GURL kUrl("http://url.com");
  const BookmarkNode* node = model().bookmark_bar_node()->children()[0].get();

  const std::u16string kNewTitle(u"goo");
  EXPECT_CALL(mock_service_observer(), BookmarkNodeChanged(node));
  model().SetTitle(node, kNewTitle,
                   bookmarks::metrics::BookmarkEditSource::kOther);
  EXPECT_EQ(node->GetTitle(), kNewTitle);
}

TEST_F(BookmarkMergedSurfaceServiceTest, BookmarkNodeFaviconChanged) {
  CreateBookmarkMergedSurfaceService();
  const BookmarkNode* bookmark_bar_node = model().bookmark_bar_node();
  const std::u16string kTitle(u"foo");
  const GURL kPageURL("http://www.google.com");

  const BookmarkNode* node =
      model().AddURL(bookmark_bar_node, 0, kTitle, kPageURL);

  std::set<GURL> changed_page_urls;
  changed_page_urls.insert(kPageURL);
  EXPECT_CALL(mock_service_observer(), BookmarkNodeFaviconChanged(node));
  model().OnFaviconsChanged(changed_page_urls, GURL());
}

TEST_F(BookmarkMergedSurfaceServiceTest,
       BookmarkParentFolderChildrenReordered) {
  CreateBookmarkMergedSurfaceService();
  AddNodesFromModelString(&model(), model().bookmark_bar_node(), "A B C D ");
  const BookmarkNode* parent = model().bookmark_bar_node();

  // Reorder bar node's bookmarks in reverse order.
  std::vector<const BookmarkNode*> new_order = {
      parent->children()[3].get(),
      parent->children()[2].get(),
      parent->children()[1].get(),
      parent->children()[0].get(),
  };

  EXPECT_CALL(mock_service_observer(),
              BookmarkParentFolderChildrenReordered(
                  BookmarkParentFolder::BookmarkBarFolder()));
  model().ReorderChildren(parent, new_order);
}

TEST_F(BookmarkMergedSurfaceServiceTest, BookmarkAllUserNodesRemoved) {
  CreateBookmarkMergedSurfaceService();
  model().CreateAccountPermanentFolders();
  AddNodesFromModelString(&model(), model().bookmark_bar_node(),
                          "1 2 3 f1:[ 4 5 ] ");
  AddNodesFromModelString(&model(), model().account_bookmark_bar_node(),
                          "7 8 9 f3:[ 10 11 ] ");
  ASSERT_EQ(
      service().GetChildrenCount(BookmarkParentFolder::BookmarkBarFolder()),
      8u);

  EXPECT_CALL(mock_service_observer(), BookmarkAllUserNodesRemoved);
  model().RemoveAllUserBookmarks(FROM_HERE);

  EXPECT_EQ(
      service().GetChildrenCount(BookmarkParentFolder::BookmarkBarFolder()),
      0u);
}

TEST(BookmarkMergedSurfaceServiceLoadingTest, ModelLoadedFirst) {
  std::unique_ptr<bookmarks::BookmarkModel> model =
      std::make_unique<bookmarks::BookmarkModel>(
          std::make_unique<bookmarks::TestBookmarkClient>());
  BookmarkMergedSurfaceService service(model.get(),
                                       /*managed_bookmark_service=*/nullptr);
  MockBookmarkMergedSurfaceServiceObserver mock_observer;
  base::ScopedObservation<BookmarkMergedSurfaceService,
                          BookmarkMergedSurfaceServiceObserver>
      scoped_mock_observer{&mock_observer};
  scoped_mock_observer.Observe(&service);

  EXPECT_CALL(mock_observer, BookmarkMergedSurfaceServiceLoaded()).Times(0);
  EXPECT_CALL(mock_observer, BookmarkNodeAdded).Times(0);
  model->LoadEmptyForTest();
  // Notifications ignored while service is not loaded yet.
  AddNodesFromModelString(model.get(), model->other_node(), "1 2 3 ");
  Mock::VerifyAndClearExpectations(&mock_observer);

  EXPECT_CALL(mock_observer, BookmarkMergedSurfaceServiceLoaded()).Times(1);
  service.LoadForTesting({});

  // Verify service is observing the bookmark model.
  EXPECT_CALL(mock_observer,
              BookmarkNodeAdded(BookmarkParentFolder::BookmarkBarFolder(), _))
      .Times(3);
  AddNodesFromModelString(model.get(), model->bookmark_bar_node(), "1 2 3 ");
}

TEST(BookmarkMergedSurfaceServiceLoadingTest, ServiceLoadedFirst) {
  std::unique_ptr<bookmarks::BookmarkModel> model =
      std::make_unique<bookmarks::BookmarkModel>(
          std::make_unique<bookmarks::TestBookmarkClient>());
  BookmarkMergedSurfaceService service(model.get(),
                                       /*managed_bookmark_service=*/nullptr);
  MockBookmarkMergedSurfaceServiceObserver mock_observer;
  base::ScopedObservation<BookmarkMergedSurfaceService,
                          BookmarkMergedSurfaceServiceObserver>
      scoped_mock_observer{&mock_observer};
  scoped_mock_observer.Observe(&service);

  EXPECT_CALL(mock_observer, BookmarkMergedSurfaceServiceLoaded()).Times(0);
  service.LoadForTesting({});
  Mock::VerifyAndClearExpectations(&mock_observer);

  EXPECT_CALL(mock_observer, BookmarkMergedSurfaceServiceLoaded()).Times(1);
  model->LoadEmptyForTest();

  // Verify service is observing the bookmark model.
  EXPECT_CALL(mock_observer,
              BookmarkNodeAdded(BookmarkParentFolder::BookmarkBarFolder(), _))
      .Times(3);
  AddNodesFromModelString(model.get(), model->bookmark_bar_node(), "1 2 3 ");
}

TEST(BookmarkMergedSurfaceServiceLoadingTest, IdsReassigned) {
  std::unique_ptr<bookmarks::BookmarkModel> model =
      std::make_unique<bookmarks::BookmarkModel>(
          std::make_unique<bookmarks::TestBookmarkClient>());
  BookmarkMergedSurfaceService service(model.get(),
                                       /*managed_bookmark_service=*/nullptr);
  MockBookmarkMergedSurfaceServiceObserver mock_observer;
  base::ScopedObservation<BookmarkMergedSurfaceService,
                          BookmarkMergedSurfaceServiceObserver>
      scoped_mock_observer{&mock_observer};
  scoped_mock_observer.Observe(&service);

  EXPECT_CALL(mock_observer, BookmarkMergedSurfaceServiceLoaded()).Times(1);
  service.BookmarkModelLoaded(/*ids_reassigned=*/true);
  model->LoadEmptyForTest();
}

TEST(BookmarkMergedSurfaceServiceLoadingTest, CustomOrder) {
  base::test::ScopedFeatureList features{
      switches::kSyncEnableBookmarksInTransportMode};
  std::unique_ptr<bookmarks::BookmarkModel> model =
      std::make_unique<bookmarks::BookmarkModel>(
          std::make_unique<bookmarks::TestBookmarkClient>());
  BookmarkMergedSurfaceService service(model.get(),
                                       /*managed_bookmark_service=*/nullptr);
  MockBookmarkMergedSurfaceServiceObserver mock_observer;
  base::ScopedObservation<BookmarkMergedSurfaceService,
                          BookmarkMergedSurfaceServiceObserver>
      scoped_mock_observer{&mock_observer};
  scoped_mock_observer.Observe(&service);

  model->LoadEmptyForTest();
  model->CreateAccountPermanentFolders();
  AddNodesFromModelString(model.get(), model->bookmark_bar_node(), "1 ");
  AddNodesFromModelString(model.get(), model->account_bookmark_bar_node(),
                          "4 5 ");

  EXPECT_CALL(mock_observer, BookmarkMergedSurfaceServiceLoaded()).Times(1);
  service.LoadForTesting(
      {{PermanentFolderType::kBookmarkBarNode,
        {model->bookmark_bar_node()->children()[0]->id(),
         model->account_bookmark_bar_node()->children()[0]->id(),
         model->account_bookmark_bar_node()->children()[1]->id()}}});

  EXPECT_EQ(
      service.GetNodeAtIndex(BookmarkParentFolder::BookmarkBarFolder(), 0),
      model->bookmark_bar_node()->children()[0].get());
  EXPECT_EQ(
      service.GetNodeAtIndex(BookmarkParentFolder::BookmarkBarFolder(), 1),
      model->account_bookmark_bar_node()->children()[0].get());
}

}  // namespace
