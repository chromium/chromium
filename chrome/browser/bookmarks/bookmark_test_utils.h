// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/raw_ptr.h"
#include "components/bookmarks/test/test_bookmark_client.h"

#ifndef CHROME_BROWSER_BOOKMARKS_BOOKMARK_TEST_UTILS_H_
#define CHROME_BROWSER_BOOKMARKS_BOOKMARK_TEST_UTILS_H_

namespace bookmarks {
class ManagedBookmarkService;
class BookmarkNode;
}  // namespace bookmarks

namespace sync_preferences {
class TestingPrefServiceSyncable;
}  // namespace sync_preferences

// Creates an instance of `bookmarks::ManagedBookmarkService` for testing. It
// will generate `managed_bookmarks_size` number of bookmarks within the managed
// node.
std::unique_ptr<bookmarks::ManagedBookmarkService> CreateManagedBookmarkService(
    sync_preferences::TestingPrefServiceSyncable* prefs,
    size_t managed_bookmarks_size);

// Bookmark client to be used with `bookmarks::ManagedBookmarkService` for
// testing managed nodes.
class TestBookmarkClientWithManagedService
    : public bookmarks::TestBookmarkClient {
 public:
  explicit TestBookmarkClientWithManagedService(
      bookmarks::ManagedBookmarkService* managed_bookmark_service);

  // BookmarkClient:
  void Init(bookmarks::BookmarkModel* model) override;
  bookmarks::LoadManagedNodeCallback GetLoadManagedNodeCallback() override;
  bool CanSetPermanentNodeTitle(
      const bookmarks::BookmarkNode* permanent_node) override;
  bool IsNodeManaged(const bookmarks::BookmarkNode* node) override;

 private:
  const raw_ptr<bookmarks::ManagedBookmarkService> managed_bookmark_service_;
};

#endif  // CHROME_BROWSER_BOOKMARKS_BOOKMARK_TEST_UTILS_H_
