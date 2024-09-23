// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "base/uuid.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/history/web_history_service_factory.h"
#include "chrome/browser/sync/test/integration/bookmarks_helper.h"
#include "chrome/browser/sync/test/integration/history_helper.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "components/history/content/browser/history_context_helper.h"
#include "components/history/core/browser/browsing_history_driver.h"
#include "components/history/core/browser/browsing_history_service.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/browser/history_types.h"
#include "components/sync/base/features.h"
#include "components/sync/protocol/history_specifics.pb.h"
#include "components/sync/service/sync_service_impl.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/dns/mock_host_resolver.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/page_transition_types.h"
#include "url/gurl.h"

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#endif

using history_helper::HasVisitDuration;
using history_helper::UrlIs;
using history_helper::VisitRowHasDuration;
using testing::AllOf;
using testing::IsEmpty;
using testing::SizeIs;
using testing::UnorderedElementsAre;

namespace {

class TestBrowsingHistoryDriver : public history::BrowsingHistoryDriver {
 public:
  explicit TestBrowsingHistoryDriver(history::WebHistoryService* web_history)
      : web_history_(web_history) {}
  ~TestBrowsingHistoryDriver() override = default;

  bool AllowHistoryDeletions() override { return true; }
  history::WebHistoryService* GetWebHistoryService() override {
    return web_history_;
  }

  void OnRemoveVisits(
      const std::vector<history::ExpireHistoryArgs>& expire_list) override {}
  bool ShouldHideWebHistoryUrl(const GURL& url) override { return false; }
  void ShouldShowNoticeAboutOtherFormsOfBrowsingHistory(
      const syncer::SyncService* sync_service,
      history::WebHistoryService* history_service,
      base::OnceCallback<void(bool)> callback) override {}

 private:
  const raw_ptr<history::WebHistoryService> web_history_;
};

class TwoClientHistorySyncTest : public SyncTest {
 public:
  TwoClientHistorySyncTest() : SyncTest(TWO_CLIENT) {
    // TODO(crbug.com/40248833): Use HTTPS URLs in tests to avoid having to
    // disable this feature.
    features_.InitAndDisableFeature(features::kHttpsUpgrades);
  }
  ~TwoClientHistorySyncTest() override = default;

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");

    ASSERT_TRUE(embedded_test_server()->Start());

    SyncTest::SetUpOnMainThread();
  }

  bool SetupClients() override {
    if (!SyncTest::SetupClients()) {
      return false;
    }

    // SyncTest doesn't create any tabs in the profiles/browsers it creates.
    // Create an "empty" tab here, so that NavigateToURL() will have a non-null
    // WebContents to navigate in.
    for (int i = 0; i < num_clients(); ++i) {
      if (!AddTabAtIndexToBrowser(GetBrowser(i), 0, GURL("about:blank"),
                                  ui::PAGE_TRANSITION_AUTO_TOPLEVEL)) {
        return false;
      }
    }

    return true;
  }

  void NavigateToURL(
      int profile_index,
      const GURL& url,
      ui::PageTransition transition = ui::PAGE_TRANSITION_TYPED) {
    content::NavigationController::LoadURLParams params(url);
    params.transition_type = transition;
    content::NavigateToURLBlockUntilNavigationsComplete(
        GetActiveWebContents(profile_index), params, 1);

    // Ensure the navigation succeeded (i.e. whatever test URL was passed in was
    // actually valid).
    ASSERT_EQ(200, GetActiveWebContents(profile_index)
                       ->GetController()
                       .GetLastCommittedEntry()
                       ->GetHttpStatusCode());
  }

  [[nodiscard]] bool WaitForServerHistory(
      testing::Matcher<std::vector<sync_pb::HistorySpecifics>> matcher) {
    return history_helper::ServerHistoryMatchChecker(matcher).Wait();
  }

  [[nodiscard]] bool WaitForLocalHistory(
      int profile_index,
      const std::map<GURL, testing::Matcher<std::vector<history::VisitRow>>>&
          matchers) {
    return history_helper::LocalHistoryMatchChecker(
               profile_index, GetSyncService(profile_index), matchers)
        .Wait();
  }

  content::WebContents* GetActiveWebContents(int profile_index) {
    // Note: chrome_test_utils::GetActiveWebContents() doesn't work, since it
    // uses the profile created by InProcessBrowserTest, not the profiles from
    // SyncTest.
    return GetBrowser(profile_index)->tab_strip_model()->GetActiveWebContents();
  }

 private:
  base::test::ScopedFeatureList features_;
};

// Very simple test; its main reason for existence is that it's the only E2E
// test covering history.
IN_PROC_BROWSER_TEST_F(TwoClientHistorySyncTest, E2E_ENABLED(SyncsUrl)) {
  ResetSyncForPrimaryAccount();
  // Use a randomized URL to prevent test collisions.
  const std::u16string kHistoryUrl = base::ASCIIToUTF16(base::StringPrintf(
      "http://www.add-history.google.com/%s",
      base::Uuid::GenerateRandomV4().AsLowercaseString().c_str()));
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  // Populate one client with a URL, wait for it to sync to the other.
  GURL new_url(kHistoryUrl);
  history_helper::AddUrlToHistory(0, new_url);
  EXPECT_TRUE(WaitForLocalHistory(1, {{new_url, SizeIs(1)}}));
}

IN_PROC_BROWSER_TEST_F(TwoClientHistorySyncTest, SyncsVisitForBookmarkedUrl) {
  GURL bookmark_url("http://www.bookmark.google.com/");
  GURL bookmark_icon_url("http://www.bookmark.google.com/favicon.ico");
  ASSERT_TRUE(SetupClients());
  // Create a bookmark.
  const bookmarks::BookmarkNode* node = bookmarks_helper::AddURL(
      0, bookmarks_helper::IndexedURLTitle(0), bookmark_url);
  bookmarks_helper::SetFavicon(0, node, bookmark_icon_url,
                               bookmarks_helper::CreateFavicon(SK_ColorWHITE),
                               bookmarks_helper::FROM_UI);
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  // A row in the DB for client 1 should have been created as a result of
  // syncing the bookmark.
  history::URLRow row;
  ASSERT_TRUE(history_helper::GetUrlFromClient(1, bookmark_url, &row));

  // Now, add a visit for client 0 to the bookmark URL and sync it over - this
  // should not cause a crash.
  history_helper::AddUrlToHistory(0, bookmark_url);

  ASSERT_TRUE(WaitForLocalHistory(0, {{bookmark_url, SizeIs(1)}}));
  EXPECT_TRUE(WaitForLocalHistory(1, {{bookmark_url, SizeIs(1)}}));
}

IN_PROC_BROWSER_TEST_F(TwoClientHistorySyncTest, SyncsUrlDeletion) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  // Navigate to two URLs on the first client.
  GURL url1 =
      embedded_test_server()->GetURL("synced1.com", "/sync/simple.html");
  NavigateToURL(0, url1);
  GURL url2 =
      embedded_test_server()->GetURL("synced2.com", "/sync/simple.html");
  NavigateToURL(0, url2);

  // Wait for the visits to arrive on the second client.
  ASSERT_TRUE(WaitForLocalHistory(1, {{url1, SizeIs(1)}, {url2, SizeIs(1)}}));

  // Delete the first URL on the first client.
  history::HistoryService* history_service =
      HistoryServiceFactory::GetForProfile(GetProfile(0),
                                           ServiceAccessType::EXPLICIT_ACCESS);
  history::WebHistoryService* web_history_service =
      WebHistoryServiceFactory::GetForProfile(GetProfile(0));
  history_service->DeleteLocalAndRemoteUrl(web_history_service, url1);

  // Wait for the deletion to apply to the second client: The first URL should
  // be gone, but the second one should remain.
  EXPECT_TRUE(WaitForLocalHistory(1, {{url1, IsEmpty()}, {url2, SizeIs(1)}}));
}

IN_PROC_BROWSER_TEST_F(TwoClientHistorySyncTest, SyncsTimeRangeDeletion) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  // Navigate to three URLs on the first client.
  GURL url1 =
      embedded_test_server()->GetURL("synced1.com", "/sync/simple.html");
  NavigateToURL(0, url1);
  GURL url2 =
      embedded_test_server()->GetURL("synced2.com", "/sync/simple.html");
  NavigateToURL(0, url2);
  GURL url3 =
      embedded_test_server()->GetURL("synced3.com", "/sync/simple.html");
  NavigateToURL(0, url3);

  // Wait for the visits to arrive on the second client.
  ASSERT_TRUE(WaitForLocalHistory(
      1, {{url1, SizeIs(1)}, {url2, SizeIs(1)}, {url3, SizeIs(1)}}));

  // Get the visit timestamps.
  history::VisitVector visits1 =
      history_helper::GetVisitsForURLFromClient(/*index=*/0, url1);
  ASSERT_EQ(visits1.size(), 1u);
  base::Time time1 = visits1[0].visit_time;
  history::VisitVector visits2 =
      history_helper::GetVisitsForURLFromClient(/*index=*/0, url2);
  ASSERT_EQ(visits2.size(), 1u);
  base::Time time2 = visits2[0].visit_time;
  history::VisitVector visits3 =
      history_helper::GetVisitsForURLFromClient(/*index=*/0, url3);
  ASSERT_EQ(visits3.size(), 1u);
  base::Time time3 = visits3[0].visit_time;

  // Delete a time range that covers exactly the second visit.
  base::Time begin = time1 + (time2 - time1) / 2;
  ASSERT_LT(time1, begin);
  ASSERT_LT(begin, time2);
  base::Time end = time2 + (time3 - time2) / 2;
  ASSERT_LT(time2, end);
  ASSERT_LT(end, time3);

  history::HistoryService* history_service =
      HistoryServiceFactory::GetForProfile(GetProfile(0),
                                           ServiceAccessType::EXPLICIT_ACCESS);
  history::WebHistoryService* web_history_service =
      WebHistoryServiceFactory::GetForProfile(GetProfile(0));
  base::CancelableTaskTracker task_tracker;
  history_service->DeleteLocalAndRemoteHistoryBetween(
      web_history_service, begin, end, history::kNoAppIdFilter,
      base::DoNothing(), &task_tracker);

  // Wait for the deletion to apply to the second client: The second URL should
  // be gone, but the first and third should remain.
  EXPECT_TRUE(WaitForLocalHistory(
      1, {{url1, SizeIs(1)}, {url2, IsEmpty()}, {url3, SizeIs(1)}}));
}

IN_PROC_BROWSER_TEST_F(TwoClientHistorySyncTest, SyncsVisitsDeletion) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  // Navigate to two URLs on the first client.
  GURL url1 =
      embedded_test_server()->GetURL("synced1.com", "/sync/simple.html");
  NavigateToURL(0, url1);
  GURL url2 =
      embedded_test_server()->GetURL("synced2.com", "/sync/simple.html");
  NavigateToURL(0, url2);

  // Navigate to the URLs again, so there are two visits per URL.
  NavigateToURL(0, url1);
  NavigateToURL(0, url2);

  // Wait for the visits to arrive on the second client.
  ASSERT_TRUE(WaitForLocalHistory(1, {{url1, SizeIs(2)}, {url2, SizeIs(2)}}));

  // Get the visit timestamps corresponding to the first URL.
  history::VisitVector visits1 =
      history_helper::GetVisitsForURLFromClient(/*index=*/0, url1);
  ASSERT_EQ(visits1.size(), 2u);
  base::Time time1a = visits1[0].visit_time;
  base::Time time1b = visits1[1].visit_time;

  // Specify deletions for both of the first URL's visits.
  // Logic similar to `BrowsingHistoryHandler::HandleRemoveVisits()`.
  history::BrowsingHistoryService::HistoryEntry entry1;
  entry1.url = url1;
  entry1.all_timestamps.insert(time1a.ToInternalValue());
  entry1.all_timestamps.insert(time1b.ToInternalValue());
  std::vector<history::BrowsingHistoryService::HistoryEntry> items_to_remove;
  items_to_remove.push_back(entry1);

  // Apply the deletions. This requires creating a BrowsingHistoryService
  // instance.
  history::HistoryService* history_service =
      HistoryServiceFactory::GetForProfile(GetProfile(0),
                                           ServiceAccessType::EXPLICIT_ACCESS);
  history::WebHistoryService* web_history_service =
      WebHistoryServiceFactory::GetForProfile(GetProfile(0));
  TestBrowsingHistoryDriver driver(web_history_service);
  history::BrowsingHistoryService browsing_history_service(
      &driver, history_service, GetSyncService(0));

  browsing_history_service.RemoveVisits(items_to_remove);
  // Note that this API applies deletions to all matching visits on the same
  // day, so it's hard to test the deletion of only *some* of a URL's visits.

  // Wait for the deletions to apply to the second client: Both visits to the
  // first URL should be gone, but the second URL's visits should remain.
  EXPECT_TRUE(WaitForLocalHistory(1, {{url1, IsEmpty()}, {url2, SizeIs(2)}}));
}

IN_PROC_BROWSER_TEST_F(TwoClientHistorySyncTest,
                       DoesNotSyncBrowsingTopicsEligibility) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  // Navigate to some URL.
  GURL url1 =
      embedded_test_server()->GetURL("synced1.com", "/sync/simple.html");
  NavigateToURL(0, url1);

  // (Hackily) mark the just-added history entry as eligible for browsing
  // topics. This field should *not* be synced.
  history::HistoryService* history_service =
      HistoryServiceFactory::GetForProfile(GetProfile(0),
                                           ServiceAccessType::EXPLICIT_ACCESS);

  history::ContextID context_id =
      history::ContextIDForWebContents(GetActiveWebContents(0));
  int nav_entry_id = GetActiveWebContents(0)
                         ->GetController()
                         .GetLastCommittedEntry()
                         ->GetUniqueID();

  history_service->SetBrowsingTopicsAllowed(context_id, nav_entry_id, url1);

  // Navigate somewhere else, to "complete" the first visit and populate its
  // duration.
  GURL url2 =
      embedded_test_server()->GetURL("synced2.com", "/sync/simple.html");
  NavigateToURL(0, url2);

  // Ensure the visit arrived on the server, including the duration. The
  // browsing-topics-allowed bit should *not* be here, but there's no real way
  // to check for its absence on the server. Instead, we'll check that on the
  // second client, below.
  EXPECT_TRUE(WaitForServerHistory(UnorderedElementsAre(
      AllOf(UrlIs(url1.spec()), HasVisitDuration()), UrlIs(url2))));

  // Wait for the visit to arrive on the second client.
  EXPECT_TRUE(WaitForLocalHistory(
      1, {{url1, UnorderedElementsAre(VisitRowHasDuration())}}));

  // Finally, check that the local visit (on the first client) has the
  // browsing-topics-allowed bit set, but the synced visit (on the second
  // client) does not.
  std::vector<history::AnnotatedVisit> local_visits =
      history_helper::GetAnnotatedVisitsForURLFromClient(0, url1);
  ASSERT_EQ(local_visits.size(), 1u);
  history::AnnotatedVisit local_visit = local_visits[0];
  EXPECT_TRUE(local_visit.content_annotations.annotation_flags &
              history::VisitContentAnnotationFlag::kBrowsingTopicsEligible);

  std::vector<history::AnnotatedVisit> synced_visits =
      history_helper::GetAnnotatedVisitsForURLFromClient(1, url1);
  ASSERT_EQ(synced_visits.size(), 1u);
  history::AnnotatedVisit synced_visit = synced_visits[0];
  EXPECT_FALSE(synced_visit.content_annotations.annotation_flags &
               history::VisitContentAnnotationFlag::kBrowsingTopicsEligible);
  // Just as a sanity check: Other visit fields *did* arrive.
  EXPECT_FALSE(synced_visit.visit_row.visit_duration.is_zero());
}

}  // namespace
