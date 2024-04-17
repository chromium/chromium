// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/bookmarks/managed/managed_bookmark_service.h"

#include <memory>
#include <utility>

#include "base/memory/raw_ptr.h"
#include "base/ranges/algorithm.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/bookmarks/chrome_bookmark_client.h"
#include "chrome/browser/bookmarks/managed_bookmark_service_factory.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/test/base/testing_profile.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_node.h"
#include "components/bookmarks/browser/bookmark_utils.h"
#include "components/bookmarks/common/bookmark_pref_names.h"
#include "components/bookmarks/test/bookmark_test_helpers.h"
#include "components/bookmarks/test/mock_bookmark_model_observer.h"
#include "components/strings/grit/components_strings.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"

using bookmarks::BookmarkModel;
using bookmarks::BookmarkNode;
using bookmarks::ManagedBookmarkService;
using testing::Mock;
using testing::_;

TEST(ManagedBookmarkServiceNoPolicyTest, EmptyManagedNode) {
  // Verifies that the managed node is empty and invisible when the policy is
  // not set.
  content::BrowserTaskEnvironment task_environment;
  TestingProfile::Builder profile_builder;
  profile_builder.AddTestingFactory(BookmarkModelFactory::GetInstance(),
                                    BookmarkModelFactory::GetDefaultFactory());
  profile_builder.AddTestingFactory(
      ManagedBookmarkServiceFactory::GetInstance(),
      ManagedBookmarkServiceFactory::GetDefaultFactory());
  std::unique_ptr<TestingProfile> profile = profile_builder.Build();

  // Make sure the policy isn't set.
  ASSERT_EQ(nullptr, profile->GetTestingPrefService()->GetManagedPref(
                         bookmarks::prefs::kManagedBookmarks));

  BookmarkModel* model =
      BookmarkModelFactory::GetForBrowserContext(profile.get());
  bookmarks::test::WaitForBookmarkModelToLoad(model);
  ManagedBookmarkService* managed =
      ManagedBookmarkServiceFactory::GetForProfile(profile.get());
  DCHECK(managed);

  ASSERT_TRUE(managed->managed_node());
  EXPECT_TRUE(managed->managed_node()->children().empty());
  EXPECT_FALSE(managed->managed_node()->IsVisible());
}

class ManagedBookmarkServiceTest : public testing::Test {
 public:
  ManagedBookmarkServiceTest() : managed_(nullptr), model_(nullptr) {}

  ManagedBookmarkServiceTest(const ManagedBookmarkServiceTest&) = delete;
  ManagedBookmarkServiceTest& operator=(const ManagedBookmarkServiceTest&) =
      delete;

  ~ManagedBookmarkServiceTest() override {}

  void SetUp() override {
    TestingProfile::Builder profile_builder;
    profile_builder.AddTestingFactory(
        BookmarkModelFactory::GetInstance(),
        BookmarkModelFactory::GetDefaultFactory());
    profile_builder.AddTestingFactory(
        ManagedBookmarkServiceFactory::GetInstance(),
        ManagedBookmarkServiceFactory::GetDefaultFactory());
    profile_ = profile_builder.Build();

    prefs_ = profile_->GetTestingPrefService();
    ASSERT_FALSE(prefs_->HasPrefPath(bookmarks::prefs::kManagedBookmarks));

    // TODO(crbug.com/40508978): Convert SetManagedPrefs to take a unique_ptr.
    prefs_->SetManagedPref(bookmarks::prefs::kManagedBookmarks,
                           base::Value(CreateTestTree()));

    // Create and load the bookmark model.
    model_ = BookmarkModelFactory::GetForBrowserContext(profile_.get());
    bookmarks::test::WaitForBookmarkModelToLoad(model_);
    model_->AddObserver(&observer_);
    managed_ = ManagedBookmarkServiceFactory::GetForProfile(profile_.get());
    DCHECK(managed_);

    // The managed node always exists.
    ASSERT_TRUE(managed_->managed_node());
    ASSERT_TRUE(managed_->managed_node()->parent() == model_->root_node());
    EXPECT_TRUE(
        model_->root_node()->GetIndexOf(managed_->managed_node()).has_value());
  }

  void TearDown() override { model_->RemoveObserver(&observer_); }

  static base::Value::Dict CreateBookmark(const std::string& title,
                                          const std::string& url) {
    EXPECT_TRUE(GURL(url).is_valid());
    base::Value::Dict dict;
    dict.Set("name", title);
    dict.Set("url", GURL(url).spec());
    return dict;
  }

  static base::Value::Dict CreateFolder(const std::string& title,
                                        base::Value::List children) {
    base::Value::Dict dict;
    dict.Set("name", title);
    dict.Set("children", std::move(children));
    return dict;
  }

  static base::Value::List CreateTestTree() {
    base::Value::List folder;
    folder.Append(CreateFolder("Empty", base::Value::List()));
    folder.Append(CreateBookmark("Youtube", "http://youtube.com/"));

    base::Value::List list;
    list.Append(CreateBookmark("Google", "http://google.com/"));
    list.Append(CreateFolder("Folder", std::move(folder)));

    return list;
  }

  static base::Value::Dict CreateExpectedTree() {
    return CreateFolder(GetManagedFolderTitle(), CreateTestTree());
  }

  static std::string GetManagedFolderTitle() {
    return l10n_util::GetStringUTF8(
        IDS_BOOKMARK_BAR_MANAGED_FOLDER_DEFAULT_NAME);
  }

  static bool NodeMatchesValue(const BookmarkNode* node,
                               const base::Value::Dict& dict) {
    const std::string* const title = dict.FindString("name");
    if (!title || node->GetTitle() != base::UTF8ToUTF16(*title))
      return false;

    if (node->is_folder()) {
      const base::Value::List* children = dict.FindList("children");
      return children &&
             base::ranges::equal(
                 *children, node->children(),
                 [](const base::Value& child, const auto& child_node) {
                   return child.is_dict() &&
                          NodeMatchesValue(child_node.get(), child.GetDict());
                 });
    }
    if (!node->is_url())
      return false;
    const std::string* const url = dict.FindString("url");
    return url && node->url() == *url;
  }

  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestingProfile> profile_;
  raw_ptr<sync_preferences::TestingPrefServiceSyncable> prefs_;
  bookmarks::MockBookmarkModelObserver observer_;
  raw_ptr<ManagedBookmarkService> managed_;
  raw_ptr<BookmarkModel> model_;
};

TEST_F(ManagedBookmarkServiceTest, LoadInitial) {
  // Verifies that the initial load picks up the initial policy too.
  EXPECT_TRUE(model_->bookmark_bar_node()->children().empty());
  EXPECT_TRUE(model_->other_node()->children().empty());
  EXPECT_FALSE(managed_->managed_node()->children().empty());
  EXPECT_TRUE(managed_->managed_node()->IsVisible());

  base::Value::Dict expected = CreateExpectedTree();
  EXPECT_TRUE(NodeMatchesValue(managed_->managed_node(), expected));
}

TEST_F(ManagedBookmarkServiceTest, SwapNodes) {
  // Swap the Google bookmark with the Folder.
  base::Value::List updated = CreateTestTree();
  ASSERT_EQ(2u, updated.size());
  std::swap(updated[0], updated[1]);

  // These two nodes should just be swapped.
  const BookmarkNode* parent = managed_->managed_node();
  EXPECT_CALL(observer_, BookmarkNodeMoved(parent, 1, parent, 0));
  prefs_->SetManagedPref(
      bookmarks::prefs::kManagedBookmarks,
      base::Value::ToUniquePtrValue(base::Value(updated.Clone())));
  Mock::VerifyAndClearExpectations(&observer_);

  // Verify the final tree.
  base::Value::Dict expected =
      CreateFolder(GetManagedFolderTitle(), std::move(updated));
  EXPECT_TRUE(NodeMatchesValue(managed_->managed_node(), expected));
}

TEST_F(ManagedBookmarkServiceTest, RemoveNode) {
  // Remove the Folder.
  base::Value::List updated = CreateTestTree();
  ASSERT_EQ(2u, updated.size());
  updated.erase(updated.begin() + 1);

  const BookmarkNode* parent = managed_->managed_node();
  EXPECT_CALL(observer_, BookmarkNodeRemoved(parent, 1, _, _, _));
  prefs_->SetManagedPref(
      bookmarks::prefs::kManagedBookmarks,
      base::Value::ToUniquePtrValue(base::Value(updated.Clone())));
  Mock::VerifyAndClearExpectations(&observer_);

  // Verify the final tree.
  base::Value::Dict expected =
      CreateFolder(GetManagedFolderTitle(), std::move(updated));
  EXPECT_TRUE(NodeMatchesValue(managed_->managed_node(), expected));
}

TEST_F(ManagedBookmarkServiceTest, CreateNewNodes) {
  // Put all the nodes inside another folder.
  base::Value::List updated;
  updated.Append(CreateFolder("Container", CreateTestTree()));

  EXPECT_CALL(observer_, BookmarkNodeAdded(_, _, _)).Times(5);
  // The remaining nodes have been pushed to positions 1 and 2; they'll both be
  // removed when at position 1.
  const BookmarkNode* parent = managed_->managed_node();
  EXPECT_CALL(observer_, BookmarkNodeRemoved(parent, 1, _, _, _)).Times(2);
  prefs_->SetManagedPref(bookmarks::prefs::kManagedBookmarks,
                         base::Value(updated.Clone()));
  Mock::VerifyAndClearExpectations(&observer_);

  // Verify the final tree.
  base::Value::Dict expected =
      CreateFolder(GetManagedFolderTitle(), std::move(updated));
  EXPECT_TRUE(NodeMatchesValue(managed_->managed_node(), expected));
}

TEST_F(ManagedBookmarkServiceTest, RemoveAllUserBookmarks) {
  // Remove the policy.
  const BookmarkNode* parent = managed_->managed_node();
  EXPECT_CALL(observer_, BookmarkNodeRemoved(parent, 0, _, _, _)).Times(2);
  prefs_->RemoveManagedPref(bookmarks::prefs::kManagedBookmarks);
  Mock::VerifyAndClearExpectations(&observer_);

  EXPECT_TRUE(managed_->managed_node()->children().empty());
  EXPECT_FALSE(managed_->managed_node()->IsVisible());
}

TEST_F(ManagedBookmarkServiceTest, IsDescendantOfManagedNode) {
  EXPECT_FALSE(
      bookmarks::IsDescendantOf(model_->root_node(), managed_->managed_node()));
  EXPECT_FALSE(bookmarks::IsDescendantOf(model_->bookmark_bar_node(),
                                         managed_->managed_node()));
  EXPECT_FALSE(bookmarks::IsDescendantOf(model_->other_node(),
                                         managed_->managed_node()));
  EXPECT_FALSE(bookmarks::IsDescendantOf(model_->mobile_node(),
                                         managed_->managed_node()));
  EXPECT_TRUE(bookmarks::IsDescendantOf(managed_->managed_node(),
                                        managed_->managed_node()));

  const BookmarkNode* parent = managed_->managed_node();
  ASSERT_EQ(2u, parent->children().size());
  EXPECT_TRUE(bookmarks::IsDescendantOf(parent->children()[0].get(),
                                        managed_->managed_node()));
  EXPECT_TRUE(bookmarks::IsDescendantOf(parent->children()[1].get(),
                                        managed_->managed_node()));

  parent = parent->children()[1].get();
  ASSERT_EQ(2u, parent->children().size());
  EXPECT_TRUE(bookmarks::IsDescendantOf(parent->children()[0].get(),
                                        managed_->managed_node()));
  EXPECT_TRUE(bookmarks::IsDescendantOf(parent->children()[1].get(),
                                        managed_->managed_node()));
}

TEST_F(ManagedBookmarkServiceTest, RemoveAllDoesntRemoveManaged) {
  EXPECT_EQ(2u, managed_->managed_node()->children().size());

  EXPECT_CALL(observer_, BookmarkNodeAdded(model_->bookmark_bar_node(), 0, _));
  EXPECT_CALL(observer_, BookmarkNodeAdded(model_->bookmark_bar_node(), 1, _));
  model_->AddURL(model_->bookmark_bar_node(), 0, u"Test",
                 GURL("http://google.com/"));
  model_->AddFolder(model_->bookmark_bar_node(), 1, u"Test Folder");
  EXPECT_EQ(2u, model_->bookmark_bar_node()->children().size());
  Mock::VerifyAndClearExpectations(&observer_);

  EXPECT_CALL(observer_, BookmarkAllUserNodesRemoved(_, _));
  model_->RemoveAllUserBookmarks(FROM_HERE);
  EXPECT_EQ(2u, managed_->managed_node()->children().size());
  EXPECT_EQ(0u, model_->bookmark_bar_node()->children().size());
  Mock::VerifyAndClearExpectations(&observer_);
}

TEST_F(ManagedBookmarkServiceTest, HasDescendantsOfManagedNode) {
  const BookmarkNode* user_node = model_->AddURL(
      model_->other_node(), 0, u"foo bar", GURL("http://www.google.com"));
  const BookmarkNode* managed_node =
      managed_->managed_node()->children().front().get();
  ASSERT_TRUE(managed_node);

  std::vector<raw_ptr<const BookmarkNode, VectorExperimental>> nodes;
  EXPECT_FALSE(bookmarks::HasDescendantsOf(nodes, managed_->managed_node()));
  nodes.push_back(user_node);
  EXPECT_FALSE(bookmarks::HasDescendantsOf(nodes, managed_->managed_node()));
  nodes.push_back(managed_node);
  EXPECT_TRUE(bookmarks::HasDescendantsOf(nodes, managed_->managed_node()));
}

TEST_F(ManagedBookmarkServiceTest, GetManagedBookmarksManager) {
  // Not managed profile
  profile_->set_profile_name("user@google.com");
  EXPECT_TRUE(
      ManagedBookmarkServiceFactory::GetManagedBookmarksManager(profile_.get())
          .empty());

  // Managed profile
  profile_->GetProfilePolicyConnector()->OverrideIsManagedForTesting(true);
  EXPECT_EQ("google.com",
            ManagedBookmarkServiceFactory::GetManagedBookmarksManager(
                profile_.get()));
}
