// Copyright (c) 2012 The Chromium Authors. All rights reserved.
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

  base::ListValue list;
  std::unique_ptr<base::DictionaryValue> node(new base::DictionaryValue());
  node->SetString("name", "Managed Bookmark");
  node->SetString("url", "http://www.chromium.org");
  list.Append(std::move(node));
  node = std::make_unique<base::DictionaryValue>();
  node->SetString("name", "Managed Folder");
  node->Set("children", std::make_unique<base::ListValue>());
  list.Append(std::move(node));
  profile->GetPrefs()->Set(bookmarks::prefs::kManagedBookmarks, list);
  ASSERT_EQ(2u, managed->managed_node()->children().size());

  ASSERT_TRUE(RunExtensionTest(
      {.name = "bookmarks"},
      {.load_as_service_worker = GetParam() == ContextType::kServiceWorker}))
      << message_;
}

}  // namespace extensions
