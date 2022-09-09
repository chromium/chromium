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
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_node.h"
#include "components/bookmarks/common/bookmark_pref_names.h"
#include "components/bookmarks/managed/managed_bookmark_service.h"
#include "components/bookmarks/test/bookmark_test_helpers.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_test.h"

using bookmarks::BookmarkModel;

namespace extensions {

using ContextType = ExtensionApiTest::ContextType;

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
  Profile* profile = browser()->profile();
  BookmarkModel* model = BookmarkModelFactory::GetForBrowserContext(profile);
  bookmarks::ManagedBookmarkService* managed =
      ManagedBookmarkServiceFactory::GetForProfile(profile);
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

  profile->GetPrefs()->Set(bookmarks::prefs::kManagedBookmarks,
                           base::Value(std::move(list)));
  ASSERT_EQ(2u, managed->managed_node()->children().size());

  ASSERT_TRUE(RunExtensionTest("bookmarks")) << message_;
}

}  // namespace extensions
