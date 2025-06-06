// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstddef>
#include <memory>
#include <optional>
#include <utility>

#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/bookmarks/managed_bookmark_service_factory.h"
#include "chrome/browser/extensions/api/bookmarks/bookmarks_api.h"
#include "chrome/browser/extensions/api/bookmarks/test/bookmarks_api_matchers.h"
#include "chrome/browser/extensions/bookmarks/bookmarks_helpers.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/api/bookmarks.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_node.h"
#include "components/bookmarks/common/bookmark_pref_names.h"
#include "components/bookmarks/managed/managed_bookmark_service.h"
#include "components/bookmarks/test/bookmark_test_helpers.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/base/signin_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/mock_render_process_host.h"
#include "extensions/browser/api_test_utils.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/test_event_router_observer.h"
#include "extensions/buildflags/buildflags.h"
#include "extensions/common/extension_builder.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/abseil-cpp/absl/strings/str_format.h"

static_assert(BUILDFLAG(ENABLE_EXTENSIONS_CORE));

using bookmarks::BookmarkModel;

namespace extensions {

using ContextType = extensions::browser_test_util::ContextType;
using ::testing::Eq;
using ::testing::IsEmpty;
using ::testing::PrintToString;
using ::testing::StrEq;

// Matches an `Event` that is an `OnCreated` event for the given `node`.
//
// const extensions::Event* arg
// const bookmarks::BookmarkNode* node
MATCHER_P(IsCreatedEventForNode, node, "") {
  return ExplainMatchResult(Eq(api::bookmarks::OnCreated::kEventName),
                            arg->event_name, result_listener) &&
         ExplainMatchResult(Eq(2u), arg->event_args.size(), result_listener) &&
         ExplainMatchResult(Eq(base::NumberToString(node->id())),
                            arg->event_args[0].GetString(), result_listener) &&
         ExplainMatchResult(
             MatchesBookmarkNode(node),
             api::bookmarks::BookmarkTreeNode::FromValue(arg->event_args[1])
                 .value(),
             result_listener);
}

// Matches an `Event` that is an `OnMoved` event. The args are:
//
// long node_id
// long old_parent_id
// int old_index
// long new_parent_id
// int new_index
MATCHER_P5(IsMovedEvent,
           node_id,
           old_parent_id,
           old_index,
           new_parent_id,
           new_index,
           "") {
  if (arg->event_name != api::bookmarks::OnMoved::kEventName) {
    *result_listener << "Expected event_name "
                     << api::bookmarks::OnMoved::kEventName << ", got "
                     << arg->event_name;
    return false;
  }
  if (!ExplainMatchResult(Eq(2u), arg->event_args.size(), result_listener)) {
    return false;
  }
  if (!ExplainMatchResult(Eq(base::NumberToString(node_id)),
                          arg->event_args[0].GetString(), result_listener)) {
    return false;
  }

  api::bookmarks::OnMoved::MoveInfo expected_move_info;
  expected_move_info.old_parent_id = base::NumberToString(old_parent_id);
  expected_move_info.old_index = old_index;
  expected_move_info.parent_id = base::NumberToString(new_parent_id);
  expected_move_info.index = new_index;
  if (arg->event_args[1] != expected_move_info.ToValue()) {
    *result_listener << "Actual MoveInfo:\n"
                     << PrintToString(arg->event_args[1])
                     << "\nDoes not match expected value:\n"
                     << PrintToString(expected_move_info.ToValue());
    return false;
  }
  return true;
}

// Matches an `Event` that is an `OnRemoved` event for the given `remove_info`
// at the given `index`.
MATCHER_P(IsRemoveEventForNodeWithIndex, remove_info, "") {
  if (arg->event_name != api::bookmarks::OnRemoved::kEventName) {
    *result_listener << "Expected event_name "
                     << api::bookmarks::OnRemoved::kEventName << ", got "
                     << arg->event_name;
    return false;
  }
  if (!ExplainMatchResult(Eq(2u), arg->event_args.size(), result_listener)) {
    return false;
  }
  if (!ExplainMatchResult(Eq(remove_info->node.id),
                          arg->event_args[0].GetString(), result_listener)) {
    return false;
  }
  if (arg->event_args[1] != remove_info->ToValue()) {
    *result_listener << "Actual RemoveInfo:\n"
                     << PrintToString(arg->event_args[1])
                     << "\nDoes not match expected value:\n"
                     << PrintToString(remove_info->ToValue());
    return false;
  }
  return true;
}

// TODO(crbug.com/414844449): The API test extension:
// - heavily relies on desktop bookmarks behavior (bookmarks bar and other
//   bookmarks folders are visible when empty)
// - uses a global state that gets carried over between sub-tests
// The easiest way to re-enable the test for desktop Android is to modernize and
// rewrite it.
#if !BUILDFLAG(IS_ANDROID)
class BookmarksApiTest : public ExtensionApiTest,
                         public testing::WithParamInterface<ContextType> {
 public:
  BookmarksApiTest() : ExtensionApiTest(GetParam()) {}
  ~BookmarksApiTest() override = default;
  BookmarksApiTest(const BookmarksApiTest&) = delete;
  BookmarksApiTest& operator=(const BookmarksApiTest&) = delete;
};

INSTANTIATE_TEST_SUITE_P(EventPage,
                         BookmarksApiTest,
                         ::testing::Values(ContextType::kEventPage));

INSTANTIATE_TEST_SUITE_P(ServiceWorker,
                         BookmarksApiTest,
                         ::testing::Values(ContextType::kServiceWorker));

IN_PROC_BROWSER_TEST_P(BookmarksApiTest, Bookmarks) {
  // Add test managed bookmarks to verify that the bookmarks API can read them
  // and can't modify them.
  BookmarkModel* model = BookmarkModelFactory::GetForBrowserContext(profile());
  bookmarks::ManagedBookmarkService* managed =
      ManagedBookmarkServiceFactory::GetForProfile(profile());
  bookmarks::test::WaitForBookmarkModelToLoad(model);

  base::Value::List list;
  {
    base::Value::Dict node;
    node.Set("name", "Managed Bookmark");
    node.Set("url", "http://www.chromium.org");
    list.Append(std::move(node));
  }

  {
    base::Value::Dict node;
    node.Set("name", "Managed Folder");
    node.Set("children", base::Value::List());
    list.Append(std::move(node));
  }

  profile()->GetPrefs()->Set(bookmarks::prefs::kManagedBookmarks,
                             base::Value(std::move(list)));
  ASSERT_EQ(2u, managed->managed_node()->children().size());

  ASSERT_TRUE(RunExtensionTest("bookmarks")) << message_;
}
#endif  // !BUILDFLAG(IS_ANDROID)

// TODO(crbug.com/414844449): The tests below depend on which permanent folders
// are visible when empty. This behaviour is currently different on Android
// Desktop vs. other Desktop platforms.
// Once it has been decided what the intended behaviour is for Android Desktop,
// these tests (or at least a subset) should be re-enabled.
#if !BUILDFLAG(IS_ANDROID)
class BookmarksApiEventsTest : public ExtensionApiTest {
 public:
  BookmarksApiEventsTest() = default;
  ~BookmarksApiEventsTest() override = default;
  BookmarksApiEventsTest(const BookmarksApiEventsTest&) = delete;
  BookmarksApiEventsTest& operator=(const BookmarksApiEventsTest&) = delete;

  void SetUpOnMainThread() override {
    ExtensionApiTest::SetUpOnMainThread();
    event_observer_ = std::make_unique<TestEventRouterObserver>(event_router());
    bookmarks::test::WaitForBookmarkModelToLoad(model());
    render_process_host_ =
        std::make_unique<content::MockRenderProcessHost>(profile());

    // A listener must be added in order for BookmarksAPI to create a
    // BookmarkEventRouter. Although only one event is needed (which will)
    // trigger all notifications, the tests add each relevant type to be more
    // realistic.
    extension_ =
        ExtensionBuilder("bookmark test").AddAPIPermission("bookmarks").Build();
    AddEventListener(api::bookmarks::OnCreated::kEventName);
    AddEventListener(api::bookmarks::OnRemoved::kEventName);
  }

  void TearDownOnMainThread() override {
    render_process_host_.reset();
    event_observer_.reset();
    ExtensionApiTest::TearDownOnMainThread();
  }

  EventRouter* event_router() { return EventRouter::Get(profile()); }
  TestEventRouterObserver* event_observer() { return event_observer_.get(); }
  bookmarks::BookmarkModel* model() {
    return BookmarkModelFactory::GetForBrowserContext(profile());
  }

  void AddEventListener(const std::string& event_name) {
    event_router()->AddEventListener(event_name, render_process_host_.get(),
                                     extension_->id());
  }

  api::bookmarks::OnRemoved::RemoveInfo GetRemoveInfo(
      const bookmarks::BookmarkNode& node,
      size_t index) {
    api::bookmarks::OnRemoved::RemoveInfo remove_info;
    remove_info.index = index;
    remove_info.node.date_added =
        node.date_added().InMillisecondsSinceUnixEpoch();
    remove_info.node.id = base::NumberToString(node.id());
    remove_info.node.title = base::UTF16ToUTF8(node.GetTitledUrlNodeTitle());
    remove_info.node.syncing = !model()->IsLocalOnlyNode(node);
    remove_info.parent_id = base::NumberToString(node.parent()->id());

    if (!node.is_folder()) {
      remove_info.node.url = node.url().spec();
    } else {
      remove_info.node.children =
          std::vector<api::bookmarks::BookmarkTreeNode>();

      if (node.type() == bookmarks::BookmarkNode::Type::BOOKMARK_BAR) {
        remove_info.node.folder_type =
            api::bookmarks::FolderType::kBookmarksBar;
      } else if (node.type() == bookmarks::BookmarkNode::Type::OTHER_NODE) {
        remove_info.node.folder_type = api::bookmarks::FolderType::kOther;
      } else if (node.type() == bookmarks::BookmarkNode::Type::MOBILE) {
        remove_info.node.folder_type = api::bookmarks::FolderType::kMobile;
      }
    }

    return remove_info;
  }

  // Returns a List of BookmarkTreeNode.
  base::Value::List GetVisiblePermanentFolders() {
    auto get_function = base::MakeRefCounted<BookmarksGetChildrenFunction>();
    return extensions::api_test_utils::RunFunctionAndReturnSingleResult(
               get_function.get(),
               absl::StrFormat(R"(["%lu"])", model()->root_node()->id()),
               profile())
        .value()
        .GetList()
        .Clone();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list{
      switches::kSyncEnableBookmarksInTransportMode};
  std::unique_ptr<TestEventRouterObserver> event_observer_;
  std::unique_ptr<content::MockRenderProcessHost> render_process_host_;
  scoped_refptr<const Extension> extension_;
};

IN_PROC_BROWSER_TEST_F(BookmarksApiEventsTest,
                       OnCreated_CalledWhenBookmarkCreated) {
  EXPECT_EQ(event_observer()->all_events().size(), 0u);

  // Create a new folder.
  const bookmarks::BookmarkNode* model_node = model()->AddURL(
      model()->other_node(), 0, u"Chromium", GURL("https://www.chromium.org/"));

  // The onCreated event should have been called.
  EXPECT_EQ(event_observer()->all_events().size(), 1u);
  EXPECT_THAT(event_observer()->all_events()[0].get(),
              IsCreatedEventForNode(model_node));
}

IN_PROC_BROWSER_TEST_F(BookmarksApiEventsTest,
                       OnCreated_CalledWhenAccountPermanentFoldersCreated) {
  // Create bookmarks in each of the local permanent folders. This ensures
  // they are visible both before and after the account permanent folders are
  // created.
  model()->AddURL(model()->bookmark_bar_node(), 0, u"Chromium",
                  GURL("https://www.chromium.org/"));
  model()->AddURL(model()->other_node(), 0, u"Chromium",
                  GURL("https://www.chromium.org/"));
  model()->AddURL(model()->mobile_node(), 0, u"Chromium",
                  GURL("https://www.chromium.org/"));
  event_observer()->ClearEvents();

  // Create the account permanent folders.
  model()->CreateAccountPermanentFolders();

  // The onCreated event should have been called for each of the visible
  // permanent folders.
  EXPECT_EQ(event_observer()->all_events().size(), 2u);
  EXPECT_THAT(event_observer()->all_events()[0].get(),
              IsCreatedEventForNode(model()->account_bookmark_bar_node()));
  EXPECT_THAT(event_observer()->all_events()[1].get(),
              IsCreatedEventForNode(model()->account_other_node()));
}

IN_PROC_BROWSER_TEST_F(BookmarksApiEventsTest,
                       OnMoved_CalledWhenBookmarkMoved) {
  // Create a new bookmark in the other folder.
  const bookmarks::BookmarkNode* bookmark_node = model()->AddURL(
      model()->other_node(), 0, u"Chromium", GURL("https://www.chromium.org/"));
  event_observer()->ClearEvents();

  // Move the bookmark to the bookmark bar.
  model()->Move(bookmark_node, model()->bookmark_bar_node(), 0);

  // The onMoved event should have been called.
  EXPECT_EQ(event_observer()->all_events().size(), 1u);
  EXPECT_THAT(event_observer()->all_events()[0].get(),
              IsMovedEvent(bookmark_node->id(), model()->other_node()->id(), 0,
                           model()->bookmark_bar_node()->id(), 0));
}

IN_PROC_BROWSER_TEST_F(BookmarksApiEventsTest, MoveMakesSourceFolderInvisible) {
  // Create a new bookmark in the mobile folder.
  const bookmarks::BookmarkNode* bookmark_node =
      model()->AddURL(model()->mobile_node(), 0, u"Chromium",
                      GURL("https://www.chromium.org/"));
  event_observer()->ClearEvents();

  // Move the bookmark to the bookmark bar. This causes the mobile folder to
  // become invisible (because it is now empty).
  model()->Move(bookmark_node, model()->bookmark_bar_node(), 0);

  // Check that we get an onMoved event for the moved node, followed by an
  // onRemoved event for the mobile folder.
  EXPECT_EQ(event_observer()->all_events().size(), 2u);

  EXPECT_THAT(event_observer()->all_events()[0].get(),
              IsMovedEvent(bookmark_node->id(), model()->mobile_node()->id(), 0,
                           model()->bookmark_bar_node()->id(), 0));

  api::bookmarks::OnRemoved::RemoveInfo expected_remove_info =
      GetRemoveInfo(*model()->mobile_node(), 2);
  expected_remove_info.node.date_group_modified =
      model()
          ->mobile_node()
          ->date_folder_modified()
          .InMillisecondsSinceUnixEpoch();
  EXPECT_THAT(event_observer()->all_events()[1].get(),
              IsRemoveEventForNodeWithIndex(&expected_remove_info));
}

IN_PROC_BROWSER_TEST_F(BookmarksApiEventsTest,
                       MoveMakesDestinationFolderVisible) {
  // Create a new bookmark in the other folder.
  const bookmarks::BookmarkNode* bookmark_node = model()->AddURL(
      model()->other_node(), 0, u"Chromium", GURL("https://www.chromium.org/"));
  event_observer()->ClearEvents();

  // Move the bookmark to the mobile folder. This causes the mobile folder to
  // become invisible (because it is now empty).
  model()->Move(bookmark_node, model()->mobile_node(), 0);

  // Check that we get an onMoved event for the moved node, followed by an
  // onRemoved event for the mobile folder.
  EXPECT_EQ(event_observer()->all_events().size(), 2u);
  EXPECT_THAT(event_observer()->all_events()[0].get(),
              IsCreatedEventForNode(model()->mobile_node()));
  EXPECT_THAT(event_observer()->all_events()[1].get(),
              IsMovedEvent(bookmark_node->id(), model()->other_node()->id(), 0,
                           model()->mobile_node()->id(), 0));
}

IN_PROC_BROWSER_TEST_F(BookmarksApiEventsTest,
                       OnRemoved_CalledWhenBookmarkRemoved) {
  const bookmarks::BookmarkNode* model_node = model()->AddURL(
      model()->other_node(), 0, u"Chromium", GURL("https://www.chromium.org/"));
  event_observer()->ClearEvents();

  // Remove the bookmark.
  api::bookmarks::OnRemoved::RemoveInfo expected_remove_info =
      GetRemoveInfo(*model_node, 0);
  model()->Remove(model_node, bookmarks::metrics::BookmarkEditSource::kOther,
                  FROM_HERE);

  // The onRemoved event should have been called once.
  EXPECT_EQ(event_observer()->all_events().size(), 1u);
  EXPECT_THAT(event_observer()->all_events()[0].get(),
              IsRemoveEventForNodeWithIndex(&expected_remove_info));
}

IN_PROC_BROWSER_TEST_F(BookmarksApiEventsTest,
                       OnRemoved_CalledWhenPermanentFoldersRemoved) {
  // Initially the following permanent folders are visible to the API:
  //
  // - Bookmark bar (index 0)
  // - Other (index 1)
  std::vector<const bookmarks::BookmarkNode*> expected_nodes = {
      model()->bookmark_bar_node(), model()->other_node()};
  EXPECT_THAT(GetVisiblePermanentFolders(), ResultMatchesNodes(expected_nodes));
  EXPECT_THAT(event_observer()->all_events(), IsEmpty());

  // Create the account permanent folders.
  model()->CreateAccountPermanentFolders();

  // The tree now contains two visible account permanent folders (the empty
  // mobile account folder, and the three empty local permanent folders are
  // hidden):
  //
  // - Account Bookmark bar (index 0)
  // - Account Other (index 1)
  expected_nodes = {model()->account_bookmark_bar_node(),
                    model()->account_other_node()};
  EXPECT_THAT(GetVisiblePermanentFolders(), ResultMatchesNodes(expected_nodes));

  // Check that observers were notified of the changes in the following order:
  //
  // - Removed: Bookmark bar (index 0)
  // - Removed: Other (index 0)
  // - Created: Account Bookmark bar (index 0)
  // - Created: Account Other (index 1)
  EXPECT_EQ(event_observer()->all_events().size(), 4u);
  api::bookmarks::OnRemoved::RemoveInfo bookmark_bar_info =
      GetRemoveInfo(*model()->bookmark_bar_node(), 0);
  api::bookmarks::OnRemoved::RemoveInfo other_info =
      GetRemoveInfo(*model()->other_node(), 0);
  EXPECT_THAT(event_observer()->all_events()[0].get(),
              IsRemoveEventForNodeWithIndex(&bookmark_bar_info));
  EXPECT_THAT(event_observer()->all_events()[1].get(),
              IsRemoveEventForNodeWithIndex(&other_info));
  EXPECT_THAT(event_observer()->all_events()[2].get(),
              IsCreatedEventForNode(model()->account_bookmark_bar_node()));
  EXPECT_THAT(event_observer()->all_events()[3].get(),
              IsCreatedEventForNode(model()->account_other_node()));
  event_observer()->ClearEvents();

  // Store info about the visible permanent folders before they are removed.
  // The folders are removed from last to first (i.e. other, then bookmark
  // bar).
  //
  // We therefore expect the onRemoved event to be called with the following
  // RemoveInfo:
  // - account_other_info (index 1)
  // - account_bookmark_bar_info (index 0)
  api::bookmarks::OnRemoved::RemoveInfo account_bookmark_bar_info =
      GetRemoveInfo(*model()->account_bookmark_bar_node(), 2);
  api::bookmarks::OnRemoved::RemoveInfo account_other_info =
      GetRemoveInfo(*model()->account_other_node(), 3);

  // Remove the account permanent folders.
  model()->RemoveAccountPermanentFolders();

  // The tree now contains just the local folders, with two of them visible.
  expected_nodes = {model()->bookmark_bar_node(), model()->other_node()};
  EXPECT_THAT(GetVisiblePermanentFolders(), ResultMatchesNodes(expected_nodes));

  // The onRemoved event should have been called for each of the visible
  // permanent folders.
  EXPECT_EQ(event_observer()->all_events().size(), 4u);
  EXPECT_THAT(event_observer()->all_events()[0].get(),
              IsCreatedEventForNode(model()->bookmark_bar_node()));
  EXPECT_THAT(event_observer()->all_events()[1].get(),
              IsCreatedEventForNode(model()->other_node()));
  EXPECT_THAT(event_observer()->all_events()[2].get(),
              IsRemoveEventForNodeWithIndex(&account_other_info));
  EXPECT_THAT(event_observer()->all_events()[3].get(),
              IsRemoveEventForNodeWithIndex(&account_bookmark_bar_info));
}
#endif  // !BUILDFLAG(IS_ANDROID)

}  // namespace extensions
