// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/string_number_conversions.h"
#include "chrome/browser/sync/test/integration/bookmarks_helper.h"
#include "chrome/browser/sync/test/integration/performance/sync_timing_helper.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "components/bookmarks/browser/bookmark_node.h"
#include "content/public/test/browser_test.h"
#include "testing/perf/perf_result_reporter.h"

using bookmarks_helper::AddURL;
using bookmarks_helper::GetBookmarkBarNode;
using bookmarks_helper::IndexedURL;
using bookmarks_helper::IndexedURLTitle;
using bookmarks_helper::Remove;
using bookmarks_helper::SetURL;
using sync_timing_helper::TimeMutualSyncCycle;

static const size_t kNumBookmarks = 150;

namespace {

constexpr char kMetricPrefixBookmarks[] = "Bookmarks.";
constexpr char kMetricAddBookmarksSyncTime[] = "add_bookmarks_sync_time";
constexpr char kMetricUpdateBookmarksSyncTime[] = "update_bookmarks_sync_time";
constexpr char kMetricDeleteBookmarksSyncTime[] = "delete_bookmarks_sync_time";

perf_test::PerfResultReporter SetUpReporter(const std::string& story) {
  perf_test::PerfResultReporter reporter(kMetricPrefixBookmarks, story);
  reporter.RegisterImportantMetric(kMetricAddBookmarksSyncTime, "ms");
  reporter.RegisterImportantMetric(kMetricUpdateBookmarksSyncTime, "ms");
  reporter.RegisterImportantMetric(kMetricDeleteBookmarksSyncTime, "ms");
  return reporter;
}

}  // namespace

class BookmarksSyncPerfTest : public SyncTest {
 public:
  BookmarksSyncPerfTest() : SyncTest(TWO_CLIENT) {}

  BookmarksSyncPerfTest(const BookmarksSyncPerfTest&) = delete;
  BookmarksSyncPerfTest& operator=(const BookmarksSyncPerfTest&) = delete;

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
};

void BookmarksSyncPerfTest::AddURLs(int profile, size_t num_urls) {
  for (size_t i = 0; i < num_urls; ++i) {
    ASSERT_TRUE(AddURL(profile, 0, NextIndexedURLTitle(),
                       GURL(NextIndexedURL())) != nullptr);
  }
}

void BookmarksSyncPerfTest::UpdateURLs(int profile) {
  for (const std::unique_ptr<bookmarks::BookmarkNode>& child :
       GetBookmarkBarNode(profile)->children()) {
    ASSERT_TRUE(SetURL(profile, child.get(), GURL(NextIndexedURL())));
  }
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

  perf_test::PerfResultReporter reporter =
      SetUpReporter(base::NumberToString(kNumBookmarks) + "_bookmarks");
  AddURLs(0, kNumBookmarks);
  base::TimeDelta dt = TimeMutualSyncCycle(GetClient(0), GetClient(1));
  ASSERT_EQ(kNumBookmarks, GetURLCount(1));
  reporter.AddResult(kMetricAddBookmarksSyncTime, dt);

  UpdateURLs(0);
  dt = TimeMutualSyncCycle(GetClient(0), GetClient(1));
  ASSERT_EQ(kNumBookmarks, GetURLCount(1));
  reporter.AddResult(kMetricUpdateBookmarksSyncTime, dt);

  RemoveURLs(0);
  dt = TimeMutualSyncCycle(GetClient(0), GetClient(1));
  ASSERT_EQ(0u, GetURLCount(1));
  reporter.AddResult(kMetricDeleteBookmarksSyncTime, dt);
}
