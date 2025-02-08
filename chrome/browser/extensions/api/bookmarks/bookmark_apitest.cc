// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <utility>

#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/bookmarks/managed_bookmark_service_factory.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/extensions/api/bookmarks.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_node.h"
#include "components/bookmarks/common/bookmark_pref_names.h"
#include "components/bookmarks/managed/managed_bookmark_service.h"
#include "components/bookmarks/test/bookmark_test_helpers.h"
#include "components/prefs/pref_service.h"
#include "components/sync/base/features.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/mock_render_process_host.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/test_event_router_observer.h"
#include "extensions/common/extension_builder.h"

using bookmarks::BookmarkModel;

namespace extensions {

using ContextType = extensions::browser_test_util::ContextType;

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

 private:
  base::test::ScopedFeatureList scoped_feature_list{
      syncer::kSyncEnableBookmarksInTransportMode};
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
  EXPECT_EQ(event_observer()->all_events()[0]->event_name,
            api::bookmarks::OnCreated::kEventName);
  base::Value::List& event_args = event_observer()->all_events()[0]->event_args;
  EXPECT_EQ(event_args.size(), 2u);
  EXPECT_EQ(event_args[0].GetString(), base::NumberToString(model_node->id()));
  api::bookmarks::BookmarkTreeNode observed_node =
      api::bookmarks::BookmarkTreeNode::FromValue(event_args[1]).value();
  EXPECT_EQ(observed_node.title, "Chromium");
  EXPECT_EQ(observed_node.url, "https://www.chromium.org/");
}

IN_PROC_BROWSER_TEST_F(BookmarksApiEventsTest,
                       OnCreated_CalledWhenAccountPermanentFoldersCreated) {
  EXPECT_EQ(event_observer()->all_events().size(), 0u);

  // Create the account permanent folders.
  model()->CreateAccountPermanentFolders();

  // The onCreated event should have been called for each of the permanent
  // folders.
  // TODO(crbug.com/392614318): The non-visible mobile folder should not trigger
  // an event.
  EXPECT_EQ(event_observer()->all_events().size(), 3u);

  EXPECT_EQ(event_observer()->all_events()[0]->event_name,
            api::bookmarks::OnCreated::kEventName);
  base::Value::List* event_args =
      &event_observer()->all_events()[0]->event_args;
  EXPECT_EQ(event_args->size(), 2u);
  EXPECT_EQ((*event_args)[0].GetString(),
            base::NumberToString(model()->account_bookmark_bar_node()->id()));

  EXPECT_EQ(event_observer()->all_events()[1]->event_name,
            api::bookmarks::OnCreated::kEventName);
  event_args = &event_observer()->all_events()[1]->event_args;
  EXPECT_EQ(event_args->size(), 2u);
  EXPECT_EQ((*event_args)[0].GetString(),
            base::NumberToString(model()->account_other_node()->id()));

  EXPECT_EQ(event_observer()->all_events()[2]->event_name,
            api::bookmarks::OnCreated::kEventName);
  event_args = &event_observer()->all_events()[2]->event_args;
  EXPECT_EQ(event_args->size(), 2u);
  EXPECT_EQ((*event_args)[0].GetString(),
            base::NumberToString(model()->account_mobile_node()->id()));
}

IN_PROC_BROWSER_TEST_F(BookmarksApiEventsTest,
                       OnRemoved_CalledWhenBookmarkRemoved) {
  const bookmarks::BookmarkNode* model_node = model()->AddURL(
      model()->other_node(), 0, u"Chromium", GURL("https://www.chromium.org/"));
  event_observer()->ClearEvents();

  // Remove the bookmark.
  model()->Remove(model_node, bookmarks::metrics::BookmarkEditSource::kOther,
                  FROM_HERE);

  // The onRemoved event should have been called.
  EXPECT_EQ(event_observer()->all_events().size(), 1u);
  EXPECT_EQ(event_observer()->all_events()[0]->event_name,
            api::bookmarks::OnRemoved::kEventName);
  base::Value::List& event_args = event_observer()->all_events()[0]->event_args;
  EXPECT_EQ(event_args.size(), 2u);
  EXPECT_EQ(event_args[0].GetString(), base::NumberToString(model_node->id()));
  // TODO(crbug.com/394562329): Test the contents of the
  // api::bookmarks::OnRemoved::RemoveInfo is correct. Probably by updating the
  // JSON schema to define this as a struct so that a `FromValue()` method is
  // auto-generated.
}

}  // namespace extensions
