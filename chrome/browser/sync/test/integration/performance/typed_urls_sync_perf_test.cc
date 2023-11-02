// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/sync/test/integration/performance/sync_timing_helper.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "chrome/browser/sync/test/integration/typed_urls_helper.h"
#include "components/sync/engine/cycle/sync_cycle_context.h"
#include "content/public/test/browser_test.h"
#include "testing/perf/perf_result_reporter.h"

using sync_timing_helper::TimeMutualSyncCycle;
using typed_urls_helper::AddUrlToHistory;
using typed_urls_helper::DeleteUrlsFromHistory;
using typed_urls_helper::GetTypedUrlsFromClient;
// This number should be as far away from a multiple of
// kDefaultMaxCommitBatchSize as possible, so that sync cycle counts
// for batch operations stay the same even if some batches end up not
// being completely full.
static const int kNumUrls = 163;
// This compile assert basically asserts that kNumUrls is right in the
// middle between two multiples of kDefaultMaxCommitBatchSize.
static_assert(
    ((kNumUrls % syncer::kDefaultMaxCommitBatchSize) >=
     (syncer::kDefaultMaxCommitBatchSize / 2)) &&
        ((kNumUrls % syncer::kDefaultMaxCommitBatchSize) <=
         ((syncer::kDefaultMaxCommitBatchSize + 1) / 2)),
    "kNumUrls should be between two multiples of kDefaultMaxCommitBatchSize");

namespace {

constexpr char kMetricPrefixTypedUrls[] = "TypedUrls.";
constexpr char kMetricAddTypedUrlsSyncTime[] = "add_typed_urls_sync_time";
constexpr char kMetricUpdateTypedUrlsSynctime[] = "update_typed_urls_sync_time";
constexpr char kMetricDeleteTypedUrlsSyncTime[] = "delete_typed_urls_sync_time";

perf_test::PerfResultReporter SetUpReporter(const std::string& story) {
  perf_test::PerfResultReporter reporter(kMetricPrefixTypedUrls, story);
  reporter.RegisterImportantMetric(kMetricAddTypedUrlsSyncTime, "ms");
  reporter.RegisterImportantMetric(kMetricUpdateTypedUrlsSynctime, "ms");
  reporter.RegisterImportantMetric(kMetricDeleteTypedUrlsSyncTime, "ms");
  return reporter;
}

}  // namespace

class TypedUrlsSyncPerfTest : public SyncTest {
 public:
  TypedUrlsSyncPerfTest() : SyncTest(TWO_CLIENT) {}

  TypedUrlsSyncPerfTest(const TypedUrlsSyncPerfTest&) = delete;
  TypedUrlsSyncPerfTest& operator=(const TypedUrlsSyncPerfTest&) = delete;

  // Adds |num_urls| new unique typed urls to |profile|.
  void AddURLs(int profile, int num_urls);

  // Update all typed urls in |profile| by visiting them once again.
  void UpdateURLs(int profile);

  // Removes all typed urls for |profile|.
  void RemoveURLs(int profile);

  // Returns the number of typed urls stored in |profile|.
  int GetURLCount(int profile);

 private:
  // Returns a new unique typed URL.
  GURL NextURL();

  // Returns a unique URL according to the integer |n|.
  GURL IntToURL(int n);

  int url_number_ = 0;
};

void TypedUrlsSyncPerfTest::AddURLs(int profile, int num_urls) {
  for (int i = 0; i < num_urls; ++i) {
    AddUrlToHistory(profile, NextURL());
  }
}

void TypedUrlsSyncPerfTest::UpdateURLs(int profile) {
  history::URLRows urls = GetTypedUrlsFromClient(profile);
  for (history::URLRows::const_iterator it = urls.begin(); it != urls.end();
       ++it) {
    AddUrlToHistory(profile, it->url());
  }
}

void TypedUrlsSyncPerfTest::RemoveURLs(int profile) {
  const history::URLRows& urls = GetTypedUrlsFromClient(profile);
  std::vector<GURL> gurls;
  for (const history::URLRow& url_row : urls) {
    gurls.push_back(url_row.url());
  }
  DeleteUrlsFromHistory(profile, gurls);
}

int TypedUrlsSyncPerfTest::GetURLCount(int profile) {
  return GetTypedUrlsFromClient(profile).size();
}

GURL TypedUrlsSyncPerfTest::NextURL() {
  return IntToURL(url_number_++);
}

GURL TypedUrlsSyncPerfTest::IntToURL(int n) {
  return GURL(base::StringPrintf("http://history%d.google.com/", n));
}

IN_PROC_BROWSER_TEST_F(TypedUrlsSyncPerfTest, P0) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  perf_test::PerfResultReporter reporter =
      SetUpReporter(base::NumberToString(kNumUrls) + "_urls");
  AddURLs(0, kNumUrls);
  base::TimeDelta dt = TimeMutualSyncCycle(GetClient(0), GetClient(1));
  ASSERT_EQ(kNumUrls, GetURLCount(1));
  reporter.AddResult(kMetricAddTypedUrlsSyncTime, dt);

  UpdateURLs(0);
  dt = TimeMutualSyncCycle(GetClient(0), GetClient(1));
  ASSERT_EQ(kNumUrls, GetURLCount(1));
  reporter.AddResult(kMetricUpdateTypedUrlsSynctime, dt);

  RemoveURLs(0);
  dt = TimeMutualSyncCycle(GetClient(0), GetClient(1));
  ASSERT_EQ(0, GetURLCount(1));
  reporter.AddResult(kMetricDeleteTypedUrlsSyncTime, dt);
}
