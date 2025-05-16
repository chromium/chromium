// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/bookmarks/bookmarks_api.h"

#include "base/memory/raw_ptr.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/extensions/bookmarks/bookmarks_error_constants.h"
#include "chrome/browser/extensions/bookmarks/bookmarks_features.h"
#include "chrome/browser/extensions/bookmarks/bookmarks_helpers.h"
#include "chrome/browser/extensions/extension_service_test_base.h"
#include "chrome/common/extensions/api/bookmarks.h"
#include "chrome/test/base/testing_profile.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_node.h"
#include "components/bookmarks/test/bookmark_test_helpers.h"
#include "components/signin/public/base/signin_switches.h"
#include "extensions/browser/api_test_utils.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/event_router_factory.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/test_event_router_observer.h"
#include "extensions/buildflags/buildflags.h"

static_assert(BUILDFLAG(ENABLE_EXTENSIONS_CORE));

using ::testing::Eq;
using ::testing::ExplainMatchResult;
using ::testing::Pointwise;

namespace extensions {

// Matches a `BookmarkTreeNode` against a `BookmarkNode`.
MATCHER(MatchesBookmarkNode, "") {
  const extensions::api::bookmarks::BookmarkTreeNode& bookmark_tree_node =
      std::get<0>(arg);
  const bookmarks::BookmarkNode& bookmark_node = *std::get<1>(arg);

  return ExplainMatchResult(Eq(base::NumberToString(bookmark_node.id())),
                            bookmark_tree_node.id, result_listener) &&
         ExplainMatchResult(
             Eq(base::NumberToString(bookmark_node.parent()->id())),
             bookmark_tree_node.parent_id, result_listener) &&
         ExplainMatchResult(Eq(bookmark_node.GetTitle()),
                            base::UTF8ToUTF16(bookmark_tree_node.title),
                            result_listener) &&
         ExplainMatchResult(Eq(bookmark_node.url().spec()),
                            bookmark_tree_node.url.value_or(""),
                            result_listener);
}

// Matches a `base::Value::List` of `BookmarkTreeNode`s against the provided
// `BookmarkNode`s.
MATCHER_P(ResultMatchesNodes, nodes, "") {
  std::vector<extensions::api::bookmarks::BookmarkTreeNode>
      result_bookmark_tree_nodes;
  std::ranges::transform(
      arg.GetList(), std::back_inserter(result_bookmark_tree_nodes),
      [](const base::Value& value) {
        return extensions::api::bookmarks::BookmarkTreeNode::FromValue(value)
            .value();
      });
  return ExplainMatchResult(Pointwise(MatchesBookmarkNode(), nodes),
                            result_bookmark_tree_nodes, result_listener);
}

class BookmarksApiUnittest : public ExtensionServiceTestBase {
 public:
  BookmarksApiUnittest() = default;
  BookmarksApiUnittest(const BookmarksApiUnittest&) = delete;
  BookmarksApiUnittest& operator=(const BookmarksApiUnittest&) = delete;

  void SetUp() override {
    ExtensionServiceTestBase::SetUp();

    ExtensionServiceInitParams params;
    params.enable_bookmark_model = true;
    InitializeExtensionService(std::move(params));

    model_ = BookmarkModelFactory::GetForBrowserContext(profile());
    bookmarks::test::WaitForBookmarkModelToLoad(model_);

    folder_node_ =
        model_->AddFolder(default_shown_permanent_node(), 0, u"Empty folder");
    subfolder_node_ = model_->AddFolder(folder_node_, 0, u"Empty subfolder");
    url_node_ = model_->AddURL(default_shown_permanent_node(), 0, u"URL", url_);
    folder_node_id_ = base::NumberToString(folder_node_->id());
  }

  // Returns a permanent node (folder) that's shown if it's empty. On desktop
  // Android, it's "Mobile bookmarks"; on other desktop platforms, it's "Other
  // bookmarks".
  const bookmarks::BookmarkNode* default_shown_permanent_node() const {
#if BUILDFLAG(IS_ANDROID)
    return model_->mobile_node();
#else
    return model_->other_node();
#endif  // BUILDFLAG(IS_ANDROID)
  }

  // Returns a permanent node (folder) that's hidden if it's empty. On desktop
  // Android, it's "Other bookmarks"; on other desktop platforms, it's "Mobile
  // bookmarks".
  const bookmarks::BookmarkNode* default_hidden_permanent_node() const {
#if BUILDFLAG(IS_ANDROID)
    return model_->other_node();
#else
    return model_->mobile_node();
#endif  // BUILDFLAG(IS_ANDROID)
  }

  raw_ptr<bookmarks::BookmarkModel> model() const { return model_; }
  const bookmarks::BookmarkNode* folder_node() const { return folder_node_; }
  std::string folder_node_id() const { return folder_node_id_; }
  const bookmarks::BookmarkNode* subfolder_node() const {
    return subfolder_node_;
  }
  const bookmarks::BookmarkNode* url_node() const { return url_node_; }
  const GURL url() const { return url_; }

 private:
  raw_ptr<bookmarks::BookmarkModel> model_ = nullptr;
  raw_ptr<const bookmarks::BookmarkNode> folder_node_ = nullptr;
  raw_ptr<const bookmarks::BookmarkNode> subfolder_node_ = nullptr;
  raw_ptr<const bookmarks::BookmarkNode> url_node_ = nullptr;
  std::string folder_node_id_;
  const GURL url_ = GURL("https://example.org");
};

// Tests that running updating a bookmark folder's url does not succeed.
// Regression test for https://crbug.com/818395.
TEST_F(BookmarksApiUnittest, Update) {
  auto update_function = base::MakeRefCounted<BookmarksUpdateFunction>();
  ASSERT_EQ(R"(Can't set URL of a bookmark folder.)",
            api_test_utils::RunFunctionAndReturnError(
                update_function.get(),
                absl::StrFormat(R"(["%s", {"url": "https://example.com"}])",
                                folder_node_id().c_str()),
                profile()));
}

// Tests that attempting to create a bookmark with no parent folder specified
// succeeds when only local/syncable bookmarks are available.
TEST_F(BookmarksApiUnittest, Create_NoParentLocalOnly) {
  auto create_function = base::MakeRefCounted<BookmarksCreateFunction>();
  base::Value result =
      extensions::api_test_utils::RunFunctionAndReturnSingleResult(
          create_function.get(), R"([{"title": "New folder"}])", profile())
          .value();
  api::bookmarks::BookmarkTreeNode result_node =
      extensions::api::bookmarks::BookmarkTreeNode::FromValue(result).value();

  // The new folder should be added as the last child of the default shown
  // permanent node.
  EXPECT_EQ(result_node.parent_id,
            base::NumberToString(default_shown_permanent_node()->id()));
  EXPECT_EQ(result_node.index,
            default_shown_permanent_node()->children().size() - 1);
}

// Tests that attempting to create a bookmark with no parent folder specified
// succeeds and uses the account bookmarks folder when the user is signed in
// with bookmarks in transport mode.
TEST_F(BookmarksApiUnittest, Create_NoParentAccount) {
  base::test::ScopedFeatureList scoped_feature_list{
      switches::kSyncEnableBookmarksInTransportMode};
  model()->CreateAccountPermanentFolders();

  auto create_function = base::MakeRefCounted<BookmarksCreateFunction>();
  base::Value result =
      extensions::api_test_utils::RunFunctionAndReturnSingleResult(
          create_function.get(), R"([{"title": "New folder"}])", profile())
          .value();
  api::bookmarks::BookmarkTreeNode result_node =
      extensions::api::bookmarks::BookmarkTreeNode::FromValue(result).value();

#if BUILDFLAG(IS_ANDROID)
  // For Android, The new folder should be added as the last child of the
  // account mobile node.
  EXPECT_EQ(result_node.parent_id,
            base::NumberToString(model()->account_mobile_node()->id()));
  EXPECT_EQ(result_node.index,
            model()->account_mobile_node()->children().size() - 1);
#else
  // For desktop, The new folder should be added as the last child of the
  // account other node.
  EXPECT_EQ(result_node.parent_id,
            base::NumberToString(model()->account_other_node()->id()));
  EXPECT_EQ(result_node.index,
            model()->account_other_node()->children().size() - 1);
#endif  // BUILDFLAG(IS_ANDROID)
}

// Tests creating a bookmark with a valid parent specified.
TEST_F(BookmarksApiUnittest, Create_ValidParent) {
  auto create_function = base::MakeRefCounted<BookmarksCreateFunction>();
  base::Value result =
      extensions::api_test_utils::RunFunctionAndReturnSingleResult(
          create_function.get(),
          absl::StrFormat(R"([{"parentId": "%lu", "title": "New folder"}])",
                          folder_node()->id()),
          profile())
          .value();
  api::bookmarks::BookmarkTreeNode result_node =
      extensions::api::bookmarks::BookmarkTreeNode::FromValue(result).value();

  // The new folder should be added as the last child of the parent folder.
  EXPECT_EQ(result_node.parent_id, folder_node_id());
  EXPECT_EQ(result_node.index, folder_node()->children().size() - 1);
}

// Tests creating a bookmark with a valid parent specified.
TEST_F(BookmarksApiUnittest,
       Create_SucceedsInLocalParentWhenBothLocalAndAccountBookmarksExist) {
  base::test::ScopedFeatureList scoped_feature_list{
      switches::kSyncEnableBookmarksInTransportMode};
  model()->CreateAccountPermanentFolders();

  auto create_function = base::MakeRefCounted<BookmarksCreateFunction>();
  base::Value result =
      extensions::api_test_utils::RunFunctionAndReturnSingleResult(
          create_function.get(),
          absl::StrFormat(R"([{"parentId": "%lu", "title": "New folder"}])",
                          folder_node()->id()),
          profile())
          .value();
  api::bookmarks::BookmarkTreeNode result_node =
      extensions::api::bookmarks::BookmarkTreeNode::FromValue(result).value();

  // The new folder should be added as the last child of the parent folder.
  EXPECT_EQ(result_node.parent_id, folder_node_id());
  EXPECT_EQ(result_node.index, folder_node()->children().size() - 1);
}

// Tests that attempting to creating a bookmark with a non-folder parent does
// not add the bookmark to that parent.
// Regression test for https://crbug.com/1441071.
TEST_F(BookmarksApiUnittest, Create_NonFolderParent) {
  auto create_function = base::MakeRefCounted<BookmarksCreateFunction>();
  std::string error = api_test_utils::RunFunctionAndReturnError(
      create_function.get(),
      absl::StrFormat(R"([{"parentId": "%d"}])", url_node()->id()), profile());
  ASSERT_EQ("Parameter 'parentId' does not specify a folder.", error);

  const bookmarks::BookmarkNode* url_node =
      model()->GetMostRecentlyAddedUserNodeForURL(url());
  ASSERT_TRUE(url_node->children().empty());
}

// Tests that attempting to create a bookmark with a parent that is not
// visible fails.
TEST_F(BookmarksApiUnittest, Create_NonExistentParent) {
  auto create_function = base::MakeRefCounted<BookmarksCreateFunction>();
  std::string error = api_test_utils::RunFunctionAndReturnError(
      create_function.get(), R"([{"parentId": "1234"}])", profile());

  EXPECT_EQ(error, bookmarks_errors::kNoParentError);
}

// Tests that attempting to create a bookmark with a parent that is not
// visible fails.
TEST_F(BookmarksApiUnittest, Create_NonVisibleParentNoVisibilityEnforcement) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(
      kEnforceBookmarkVisibilityOnExtensionsAPI);

  auto create_function = base::MakeRefCounted<BookmarksCreateFunction>();
  base::Value result =
      extensions::api_test_utils::RunFunctionAndReturnSingleResult(
          create_function.get(),
          absl::StrFormat(R"([{"parentId": "%lu", "title": "New folder"}])",
                          default_hidden_permanent_node()->id()),
          profile())
          .value();
  api::bookmarks::BookmarkTreeNode result_node =
      extensions::api::bookmarks::BookmarkTreeNode::FromValue(result).value();

  // The new folder should be added as the last child of the parent folder.
  EXPECT_EQ(result_node.parent_id,
            base::NumberToString(default_hidden_permanent_node()->id()));
  EXPECT_EQ(result_node.index, folder_node()->children().size() - 1);

  // The default hidden permanent node is now visible, as it no longer empty.
  ASSERT_TRUE(default_hidden_permanent_node()->IsVisible());
}

TEST_F(BookmarksApiUnittest, Create_NonVisibleParent) {
  // The default hidden permanent node is not visible, because it is empty.
  ASSERT_FALSE(default_hidden_permanent_node()->IsVisible());

  auto create_function = base::MakeRefCounted<BookmarksCreateFunction>();
  std::string error = api_test_utils::RunFunctionAndReturnError(
      create_function.get(),
      absl::StrFormat(R"([{"parentId": "%lu", "title": "New folder"}])",
                      default_hidden_permanent_node()->id()),
      profile());

  EXPECT_EQ(error, bookmarks_errors::kNoParentError);
}

TEST_F(BookmarksApiUnittest,
       Get_SucceedsForLocalPermanentFolderWhenNoAccountFolders) {
  auto get_function = base::MakeRefCounted<BookmarksGetFunction>();
  base::Value result =
      extensions::api_test_utils::RunFunctionAndReturnSingleResult(
          get_function.get(),
          absl::StrFormat(R"(["%lu"])", default_shown_permanent_node()->id()),
          profile())
          .value();

  std::vector<const bookmarks::BookmarkNode*> expected_nodes = {
      default_shown_permanent_node()};
  EXPECT_THAT(result, ResultMatchesNodes(expected_nodes));
}

TEST_F(BookmarksApiUnittest,
       Get_SucceedsForNonEmptyLocalPermanentFolderWhenAccountFolders) {
  base::test::ScopedFeatureList scoped_feature_list{
      switches::kSyncEnableBookmarksInTransportMode};
  model()->CreateAccountPermanentFolders();

  auto get_function = base::MakeRefCounted<BookmarksGetFunction>();
  base::Value result =
      extensions::api_test_utils::RunFunctionAndReturnSingleResult(
          get_function.get(),
          absl::StrFormat(R"(["%lu"])", default_shown_permanent_node()->id()),
          profile())
          .value();

  std::vector<const bookmarks::BookmarkNode*> expected_nodes = {
      default_shown_permanent_node()};
  EXPECT_THAT(result, ResultMatchesNodes(expected_nodes));
}

TEST_F(BookmarksApiUnittest,
       Get_ReturnsEmptyForNonVisibleFolderNoVisibilityEnforcement) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(
      kEnforceBookmarkVisibilityOnExtensionsAPI);

  // The default hidden permanent node is not visible, because it is empty.
  ASSERT_FALSE(default_hidden_permanent_node()->IsVisible());

  auto get_function = base::MakeRefCounted<BookmarksGetFunction>();
  base::Value result =
      extensions::api_test_utils::RunFunctionAndReturnSingleResult(
          get_function.get(),
          absl::StrFormat(R"(["%lu"])", default_hidden_permanent_node()->id()),
          profile())
          .value();

  EXPECT_THAT(result.GetList(), ::testing::IsEmpty());
}

TEST_F(BookmarksApiUnittest, Get_ReturnsErrorForNonVisibleFolder) {
  // The default hidden permanent node is not visible, because it is empty.
  ASSERT_FALSE(default_hidden_permanent_node()->IsVisible());

  auto get_function = base::MakeRefCounted<BookmarksGetFunction>();
  std::string error = api_test_utils::RunFunctionAndReturnError(
      get_function.get(),
      absl::StrFormat(R"(["%lu"])", default_hidden_permanent_node()->id()),
      profile());

  EXPECT_EQ(error, extensions::bookmarks_errors::kNoNodeError);
}

TEST_F(BookmarksApiUnittest, Get_FailsForNonExistentId) {
  auto get_function = base::MakeRefCounted<BookmarksGetFunction>();
  std::string error = api_test_utils::RunFunctionAndReturnError(
      get_function.get(), R"(["1233456"])", profile());

  EXPECT_EQ(error, extensions::bookmarks_errors::kNoNodeError);
}

TEST_F(BookmarksApiUnittest,
       GetChildren_ReturnsEmptyForNonVisibleFolderNoVisibilityEnforcement) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(
      kEnforceBookmarkVisibilityOnExtensionsAPI);

  // The default hidden permanent node is not visible, because it is empty.
  ASSERT_FALSE(default_hidden_permanent_node()->IsVisible());

  auto get_function = base::MakeRefCounted<BookmarksGetChildrenFunction>();
  base::Value result =
      extensions::api_test_utils::RunFunctionAndReturnSingleResult(
          get_function.get(),
          absl::StrFormat(R"(["%lu"])", default_hidden_permanent_node()->id()),
          profile())
          .value();

  EXPECT_THAT(result.GetList(), ::testing::IsEmpty());
}

TEST_F(BookmarksApiUnittest, GetChildren_ReturnsErrorForNonVisibleFolder) {
  // The default hidden permanent node is not visible, because it is empty.
  ASSERT_FALSE(default_hidden_permanent_node()->IsVisible());

  auto get_function = base::MakeRefCounted<BookmarksGetChildrenFunction>();
  std::string error = api_test_utils::RunFunctionAndReturnError(
      get_function.get(),
      absl::StrFormat(R"(["%lu"])", default_hidden_permanent_node()->id()),
      profile());

  EXPECT_EQ(error, extensions::bookmarks_errors::kNoNodeError);
}

TEST_F(BookmarksApiUnittest, GetChildren_FailsForNonExistentId) {
  auto get_function = base::MakeRefCounted<BookmarksGetChildrenFunction>();
  std::string error = api_test_utils::RunFunctionAndReturnError(
      get_function.get(), R"(["1233456"])", profile());

  EXPECT_EQ(error, extensions::bookmarks_errors::kNoNodeError);
}

TEST_F(BookmarksApiUnittest,
       GetSubTree_ReturnsEmptyForNonVisibleFolderNoVisibilityEnforcement) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(
      kEnforceBookmarkVisibilityOnExtensionsAPI);

  // The default hidden permanent node is not visible, because it is empty.
  ASSERT_FALSE(default_hidden_permanent_node()->IsVisible());

  auto get_function = base::MakeRefCounted<BookmarksGetSubTreeFunction>();
  base::Value result =
      extensions::api_test_utils::RunFunctionAndReturnSingleResult(
          get_function.get(),
          absl::StrFormat(R"(["%lu"])", default_hidden_permanent_node()->id()),
          profile())
          .value();

  EXPECT_THAT(result.GetList(), ::testing::IsEmpty());
}

TEST_F(BookmarksApiUnittest, GetSubTree_ReturnsErrorForNonVisibleFolder) {
  // The default hidden permanent node is not visible, because it is empty.
  ASSERT_FALSE(default_hidden_permanent_node()->IsVisible());

  auto get_function = base::MakeRefCounted<BookmarksGetSubTreeFunction>();
  std::string error = api_test_utils::RunFunctionAndReturnError(
      get_function.get(),
      absl::StrFormat(R"(["%lu"])", default_hidden_permanent_node()->id()),
      profile());

  EXPECT_EQ(error, extensions::bookmarks_errors::kNoNodeError);
}

TEST_F(BookmarksApiUnittest, GetSubTree_FailsForNonExistentId) {
  auto get_function = base::MakeRefCounted<BookmarksGetSubTreeFunction>();
  std::string error = api_test_utils::RunFunctionAndReturnError(
      get_function.get(), R"(["1233456"])", profile());

  EXPECT_EQ(error, extensions::bookmarks_errors::kNoNodeError);
}

TEST_F(BookmarksApiUnittest, Search_MatchesTitle) {
  auto function = base::MakeRefCounted<BookmarksSearchFunction>();
  base::Value result =
      extensions::api_test_utils::RunFunctionAndReturnSingleResult(
          function.get(), R"([{"title": "Empty folder"}])", profile())
          .value();

  std::vector<const bookmarks::BookmarkNode*> expected_nodes = {folder_node()};
  EXPECT_THAT(result, ResultMatchesNodes(expected_nodes));
}

TEST_F(BookmarksApiUnittest, Search_NonVisibleFolderNotReturned) {
  // Set the title of the folder node to a fixed value.
  model()->SetTitle(default_hidden_permanent_node(), u"Mobile Bookmarks",
                    bookmarks::metrics::BookmarkEditSource::kOther);
  ASSERT_FALSE(default_hidden_permanent_node()->IsVisible());

  auto function = base::MakeRefCounted<BookmarksSearchFunction>();
  base::Value result =
      extensions::api_test_utils::RunFunctionAndReturnSingleResult(
          function.get(), R"([{"title": "Mobile Bookmarks"}])", profile())
          .value();

  EXPECT_THAT(result.GetList(), ::testing::IsEmpty());
}

TEST_F(BookmarksApiUnittest,
       GetTree_SucceedsForLocalPermanentFolderWhenNoAccountFolders) {
  auto get_tree_function = base::MakeRefCounted<BookmarksGetTreeFunction>();
  const base::Value result =
      extensions::api_test_utils::RunFunctionAndReturnSingleResult(
          get_tree_function.get(), R"([])", profile())
          .value();

  // The result should contain a single root node.
  ASSERT_EQ(result.GetList().size(), 1u);
  auto root_node = extensions::api::bookmarks::BookmarkTreeNode::FromValue(
      result.GetList()[0]);
  EXPECT_EQ(root_node->id, "0");

#if BUILDFLAG(IS_ANDROID)
  // Check that its child is the mobile bookmarks folder.
  ASSERT_EQ(root_node->children.value().size(), 1u);
  EXPECT_EQ(root_node->children.value()[0].id,
            base::NumberToString(model()->mobile_node()->id()));

  // Check that the mobile bookmarks folder contains the non-permanent
  // folder/url.
  auto& mobile_node = root_node->children.value()[0];

  ASSERT_EQ(mobile_node.children.value().size(), 2u);
  EXPECT_EQ(mobile_node.children.value()[0].id,
            base::NumberToString(url_node()->id()));
  EXPECT_EQ(mobile_node.children.value()[1].id,
            base::NumberToString(folder_node()->id()));
#else
  // Check that its children include the bookmarks bar and other bookmarks
  // folder,
  ASSERT_EQ(root_node->children.value().size(), 2u);
  EXPECT_EQ(root_node->children.value()[0].id,
            base::NumberToString(model()->bookmark_bar_node()->id()));
  EXPECT_EQ(root_node->children.value()[1].id,
            base::NumberToString(model()->other_node()->id()));

  // Check that the other bookmarks folder contains the non-permanent
  // folder/url.
  auto& other_node = root_node->children.value()[1];
  ASSERT_EQ(other_node.children.value().size(), 2u);
  EXPECT_EQ(other_node.children.value()[0].id,
            base::NumberToString(url_node()->id()));
  EXPECT_EQ(other_node.children.value()[1].id,
            base::NumberToString(folder_node()->id()));
#endif  // BUILDFLAG(IS_ANDROID)
}

TEST_F(BookmarksApiUnittest, GetTree_SucceedsWhenLocalAndAccountFolders) {
  base::test::ScopedFeatureList scoped_feature_list{
      switches::kSyncEnableBookmarksInTransportMode};
  model()->CreateAccountPermanentFolders();

  auto get_tree_function = base::MakeRefCounted<BookmarksGetTreeFunction>();
  const base::Value result =
      extensions::api_test_utils::RunFunctionAndReturnSingleResult(
          get_tree_function.get(), R"([])", profile())
          .value();

  // The result should contain a single root node. Check that its children
  // include the three permanent folders, plus the non-permanent folder/url.
  ASSERT_EQ(result.GetList().size(), 1u);
  auto root_node = extensions::api::bookmarks::BookmarkTreeNode::FromValue(
      result.GetList()[0]);
  EXPECT_EQ(root_node->id, "0");

#if BUILDFLAG(IS_ANDROID)
  // For Android, the local and account mobile bookmarks should be returned,
  // totalling two children.
  ASSERT_EQ(root_node->children.value().size(), 2u);

  // TODO(crbug.com/382263783): the account folders should be returned before
  // the local folders.
  EXPECT_EQ(root_node->children.value()[0].id,
            base::NumberToString(model()->mobile_node()->id()));
  auto& mobile_node = root_node->children.value()[0];
  EXPECT_EQ(mobile_node.index, 0);
  ASSERT_EQ(mobile_node.children.value().size(), 2u);
  EXPECT_EQ(mobile_node.children.value()[0].id,
            base::NumberToString(url_node()->id()));
  EXPECT_EQ(mobile_node.children.value()[1].id,
            base::NumberToString(folder_node()->id()));

  EXPECT_EQ(root_node->children.value()[1].id,
            base::NumberToString(model()->account_mobile_node()->id()));
  EXPECT_EQ(root_node->children.value()[1].index, 1);
#else
  // For desktop, the local and account bookmark bars and other bookmarks should
  // be returned, totalling four children.
  ASSERT_EQ(root_node->children.value().size(), 4u);

  // TODO(crbug.com/382263783): the account folders should be returned before
  // the local folders.
  EXPECT_EQ(root_node->children.value()[0].id,
            base::NumberToString(model()->bookmark_bar_node()->id()));
  EXPECT_EQ(root_node->children.value()[0].index, 0);

  EXPECT_EQ(root_node->children.value()[1].id,
            base::NumberToString(model()->other_node()->id()));
  auto& other_node = root_node->children.value()[1];
  EXPECT_EQ(other_node.index, 1);
  ASSERT_EQ(other_node.children.value().size(), 2u);
  EXPECT_EQ(other_node.children.value()[0].id,
            base::NumberToString(url_node()->id()));
  EXPECT_EQ(other_node.children.value()[1].id,
            base::NumberToString(folder_node()->id()));

  EXPECT_EQ(root_node->children.value()[2].id,
            base::NumberToString(model()->account_bookmark_bar_node()->id()));
  EXPECT_EQ(root_node->children.value()[2].index, 2);

  EXPECT_EQ(root_node->children.value()[3].id,
            base::NumberToString(model()->account_other_node()->id()));
  EXPECT_EQ(root_node->children.value()[3].index, 3);
#endif  // BUILDFLAG(IS_ANDROID)
}

// Tests that moving from local to account storage is allowed.
TEST_F(BookmarksApiUnittest, Move_LocalToAccount) {
  base::test::ScopedFeatureList scoped_feature_list{
      switches::kSyncEnableBookmarksInTransportMode};
  model()->CreateAccountPermanentFolders();

  ASSERT_TRUE(model()->IsLocalOnlyNode(*folder_node()));

  // Set the account node to mirror `default_shown_permanent_node()`.
  auto* account_node = model()->account_other_node();
#if BUILDFLAG(IS_ANDROID)
  account_node = model()->account_mobile_node();
#endif  // BUILDFLAG(IS_ANDROID)

  auto move_function = base::MakeRefCounted<BookmarksMoveFunction>();
  base::Value result =
      extensions::api_test_utils::RunFunctionAndReturnSingleResult(
          move_function.get(),
          absl::StrFormat(R"(["%lu", {"parentId": "%lu"}])",
                          folder_node()->id(), account_node->id()),
          profile())
          .value();
  api::bookmarks::BookmarkTreeNode result_node =
      extensions::api::bookmarks::BookmarkTreeNode::FromValue(result).value();

  EXPECT_EQ(result_node.parent_id, base::NumberToString(account_node->id()));
  EXPECT_EQ(result_node.index, 0);
  EXPECT_EQ(account_node->children()[0].get(), folder_node());
}

// Tests that attempting to move a bookmark to a non-folder parent does
// not add the bookmark to that parent.
// Regression test for https://crbug.com/1491227.
TEST_F(BookmarksApiUnittest, Move_NonFolderParent) {
  auto move_function = base::MakeRefCounted<BookmarksMoveFunction>();
  std::string error = api_test_utils::RunFunctionAndReturnError(
      move_function.get(),
      absl::StrFormat(R"(["%d", {"parentId": "%d"}])", folder_node()->id(),
                      url_node()->id()),
      profile());
  ASSERT_EQ("Parameter 'parentId' does not specify a folder.", error);

  const bookmarks::BookmarkNode* url_node =
      model()->GetMostRecentlyAddedUserNodeForURL(url());
  ASSERT_TRUE(url_node->children().empty());
}

// Tests that attempting to move a bookmark to a non existent parent returns an
// error.
TEST_F(BookmarksApiUnittest, Move_NonExistentParent) {
  auto move_function = base::MakeRefCounted<BookmarksMoveFunction>();
  std::string error = api_test_utils::RunFunctionAndReturnError(
      move_function.get(),
      absl::StrFormat(R"(["%s", {"parentId": "1234"}])",
                      folder_node_id().c_str()),
      profile());
  EXPECT_EQ(error, bookmarks_errors::kNoParentError);

  const bookmarks::BookmarkNode* url_node =
      model()->GetMostRecentlyAddedUserNodeForURL(url());
  ASSERT_TRUE(url_node->children().empty());
}

TEST_F(BookmarksApiUnittest, Move_NonVisibleParentNoVisibilityEnforcement) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(
      kEnforceBookmarkVisibilityOnExtensionsAPI);

  // The default hidden permanent node is not visible, because it is empty.
  ASSERT_FALSE(default_hidden_permanent_node()->IsVisible());

  auto move_function = base::MakeRefCounted<BookmarksMoveFunction>();
  base::Value result =
      extensions::api_test_utils::RunFunctionAndReturnSingleResult(
          move_function.get(),
          absl::StrFormat(R"(["%lu", {"parentId": "%lu"}])",
                          folder_node()->id(),
                          default_hidden_permanent_node()->id()),
          profile())
          .value();
  api::bookmarks::BookmarkTreeNode result_node =
      extensions::api::bookmarks::BookmarkTreeNode::FromValue(result).value();

  EXPECT_EQ(result_node.parent_id,
            base::NumberToString(default_hidden_permanent_node()->id()));
  EXPECT_EQ(result_node.index, 0);
  EXPECT_EQ(default_hidden_permanent_node()->children()[0].get(),
            folder_node());

  ASSERT_TRUE(default_hidden_permanent_node()->IsVisible());
}

TEST_F(BookmarksApiUnittest, Move_NonVisibleParent) {
  // The default hidden permanent node is not visible, because it is empty.
  ASSERT_FALSE(default_hidden_permanent_node()->IsVisible());

  auto move_function = base::MakeRefCounted<BookmarksMoveFunction>();
  std::string error = api_test_utils::RunFunctionAndReturnError(
      move_function.get(),
      absl::StrFormat(R"(["%lu", {"parentId": "%lu"}])", folder_node()->id(),
                      default_hidden_permanent_node()->id()),
      profile());

  EXPECT_EQ(error, bookmarks_errors::kNoParentError);
}

// Tests that attempting to move a folder to itself returns an error.
TEST_F(BookmarksApiUnittest, Move_FolderToItself) {
  auto move_function = base::MakeRefCounted<BookmarksMoveFunction>();
  std::string error = api_test_utils::RunFunctionAndReturnError(
      move_function.get(),
      absl::StrFormat(R"(["%s", {"parentId": "%s"}])", folder_node_id().c_str(),
                      folder_node_id().c_str()),
      profile());
  ASSERT_EQ("Can't move a folder to itself or its descendant.", error);
}

// Tests that attempting to move a folder to its descendant returns an error.
TEST_F(BookmarksApiUnittest, Move_FolderToDescendant) {
  auto move_function = base::MakeRefCounted<BookmarksMoveFunction>();
  std::string error = api_test_utils::RunFunctionAndReturnError(
      move_function.get(),
      absl::StrFormat(R"(["%d", {"parentId": "%d"}])", folder_node()->id(),
                      subfolder_node()->id()),
      profile());
  ASSERT_EQ("Can't move a folder to itself or its descendant.", error);
}

}  // namespace extensions
