// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/sync/test/integration/history_helper.h"
#include "chrome/browser/sync/test/integration/sync_service_impl_harness.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "chrome/browser/sync/test/integration/typed_urls_helper.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "components/history/content/browser/history_context_helper.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/browser/history_types.h"
#include "components/sync/base/features.h"
#include "components/sync/base/model_type.h"
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
using testing::UnorderedElementsAre;

class TwoClientHistorySyncTest : public SyncTest {
 public:
  TwoClientHistorySyncTest() : SyncTest(TWO_CLIENT) {
    features_.InitWithFeatures(
        {syncer::kSyncEnableHistoryDataType},
        // TODO(crbug.com/1394910): Use HTTPS URLs in tests to avoid having to
        // disable this feature.
        /*disabled_features=*/{features::kHttpsUpgrades});
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
      typed_urls_helper::GetAnnotatedVisitsForURLFromClient(0, url1);
  ASSERT_EQ(local_visits.size(), 1u);
  history::AnnotatedVisit local_visit = local_visits[0];
  EXPECT_TRUE(local_visit.content_annotations.annotation_flags &
              history::VisitContentAnnotationFlag::kBrowsingTopicsEligible);

  std::vector<history::AnnotatedVisit> synced_visits =
      typed_urls_helper::GetAnnotatedVisitsForURLFromClient(1, url1);
  ASSERT_EQ(synced_visits.size(), 1u);
  history::AnnotatedVisit synced_visit = synced_visits[0];
  EXPECT_FALSE(synced_visit.content_annotations.annotation_flags &
               history::VisitContentAnnotationFlag::kBrowsingTopicsEligible);
  // Just as a sanity check: Other visit fields *did* arrive.
  EXPECT_FALSE(synced_visit.visit_row.visit_duration.is_zero());
}
