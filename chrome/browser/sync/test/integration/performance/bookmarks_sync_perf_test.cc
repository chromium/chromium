// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/macros.h"
#include "chrome/browser/sync/test/integration/bookmarks_helper.h"
#include "chrome/browser/sync/test/integration/performance/sync_timing_helper.h"
#include "chrome/browser/sync/test/integration/profile_sync_service_harness.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "components/bookmarks/browser/bookmark_node.h"

using bookmarks_helper::AddURL;
using bookmarks_helper::AllModelsMatch;
using bookmarks_helper::GetBookmarkBarNode;
using bookmarks_helper::IndexedURL;
using bookmarks_helper::IndexedURLTitle;
using bookmarks_helper::Remove;
using bookmarks_helper::SetURL;
using sync_timing_helper::PrintResult;
using sync_timing_helper::TimeMutualSyncCycle;

static const size_t kNumBookmarks = 150;

class BookmarksSyncPerfTest : public SyncTest {
 public:
  BookmarksSyncPerfTest() : SyncTest(TWO_CLIENT) {}

  // Adds |num_urls| new unique bookmarks to the bookmark bar for |profile|.
  void AddURLs(int profile, size_t num_urls);

  // Updates the URL for all bookmarks in the bookmark bar for |profile|.
  void UpdateURLs(int profile);

  // Removes all bookmarks in the bookmark bar for |profile|.
  void RemoveURLs(int profile);

  // Returns the number of bookmarks stored in the bookmark bar for |profile|.
  size_t GetURLCount(int profile);

 private:
  // Returns a new unique bookmark URL.
  std::string NextIndexedURL();

  // Returns a new unique bookmark title.
  std::string NextIndexedURLTitle();

  size_t url_number_ = 0;
  size_t url_title_number_ = 0;
  DISALLOW_COPY_AND_ASSIGN(BookmarksSyncPerfTest);
};

void BookmarksSyncPerfTest::AddURLs(int profile, size_t num_urls) {
  for (size_t i = 0; i < num_urls; ++i) {
    ASSERT_TRUE(AddURL(profile, 0, NextIndexedURLTitle(),
                       GURL(NextIndexedURL())) != nullptr);
  }
}

void BookmarksSyncPerfTest::UpdateURLs(int profile) {
  for (const auto& child : GetBookmarkBarNode(profile)->children())
    ASSERT_TRUE(SetURL(profile, child.get(), GURL(NextIndexedURL())));
}

void BookmarksSyncPerfTest::RemoveURLs(int profile) {
  while (!GetBookmarkBarNode(profile)->children().empty()) {
    Remove(profile, GetBookmarkBarNode(profile), 0);
  }
}

size_t BookmarksSyncPerfTest::GetURLCount(int profile) {
  return GetBookmarkBarNode(profile)->children().size();
}

std::string BookmarksSyncPerfTest::NextIndexedURL() {
  return IndexedURL(url_number_++);
}

std::string BookmarksSyncPerfTest::NextIndexedURLTitle() {
  return IndexedURLTitle(url_title_number_++);
}

IN_PROC_BROWSER_TEST_F(BookmarksSyncPerfTest, P0) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  AddURLs(0, kNumBookmarks);
  base::TimeDelta dt = TimeMutualSyncCycle(GetClient(0), GetClient(1));
  ASSERT_EQ(kNumBookmarks, GetURLCount(1));
  PrintResult("bookmarks", "add_bookmarks", dt);

  UpdateURLs(0);
  dt = TimeMutualSyncCycle(GetClient(0), GetClient(1));
  ASSERT_EQ(kNumBookmarks, GetURLCount(1));
  PrintResult("bookmarks", "update_bookmarks", dt);

  RemoveURLs(0);
  dt = TimeMutualSyncCycle(GetClient(0), GetClient(1));
  ASSERT_EQ(0u, GetURLCount(1));
  PrintResult("bookmarks", "delete_bookmarks", dt);
}
