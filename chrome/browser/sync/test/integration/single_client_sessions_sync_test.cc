// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/macros.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/test/metrics/histogram_tester.h"
#include "build/build_config.h"
#include "chrome/browser/sessions/session_service.h"
#include "chrome/browser/sessions/session_tab_helper.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/sync/session_sync_service_factory.h"
#include "chrome/browser/sync/sessions/sync_sessions_router_tab_helper.h"
#include "chrome/browser/sync/test/integration/profile_sync_service_harness.h"
#include "chrome/browser/sync/test/integration/session_hierarchy_match_checker.h"
#include "chrome/browser/sync/test/integration/sessions_helper.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "chrome/browser/sync/test/integration/typed_urls_helper.h"
#include "chrome/browser/sync/test/integration/updated_progress_marker_checker.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/url_constants.h"
#include "components/history/core/browser/history_types.h"
#include "components/sessions/core/session_types.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "components/sync/base/time.h"
#include "components/sync/protocol/proto_value_conversions.h"
#include "components/sync/protocol/session_specifics.pb.h"
#include "components/sync/test/fake_server/sessions_hierarchy.h"
#include "components/sync_sessions/session_store.h"
#include "components/sync_sessions/session_sync_service.h"
#include "components/sync_sessions/session_sync_test_helper.h"
#include "components/sync_sessions/synced_session_tracker.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

using base::HistogramBase;
using base::HistogramSamples;
using base::HistogramTester;
using fake_server::SessionsHierarchy;
using sessions_helper::CheckInitialState;
using sessions_helper::CloseTab;
using sessions_helper::ExecJs;
using sessions_helper::GetLocalWindows;
using sessions_helper::GetSessionData;
using sessions_helper::MoveTab;
using sessions_helper::NavigateTab;
using sessions_helper::NavigateTabBack;
using sessions_helper::NavigateTabForward;
using sessions_helper::OpenTab;
using sessions_helper::OpenTabAtIndex;
using sessions_helper::OpenTabFromSourceIndex;
using sessions_helper::ScopedWindowMap;
using sessions_helper::SessionWindowMap;
using sessions_helper::SyncedSessionVector;
using sessions_helper::WaitForTabsToLoad;
using sessions_helper::WindowsMatch;
using sync_sessions::SessionSyncTestHelper;
using testing::IsEmpty;
using testing::UnorderedElementsAre;
using typed_urls_helper::GetUrlFromClient;

static const char* kURL1 = "data:text/html,<html><title>Test</title></html>";
static const char* kURL2 = "data:text/html,<html><title>Test2</title></html>";
static const char* kURL3 = "data:text/html,<html><title>Test3</title></html>";
static const char* kURL4 = "data:text/html,<html><title>Test4</title></html>";
static const char* kBaseFragmentURL =
    "data:text/html,<html><title>Fragment</title><body></body></html>";
static const char* kSpecifiedFragmentURL =
    "data:text/html,<html><title>Fragment</title><body></body></html>#fragment";

void ExpectUniqueSampleGE(const HistogramTester& histogram_tester,
                          const std::string& name,
                          HistogramBase::Sample sample,
                          HistogramBase::Count expected_inclusive_lower_bound) {
  std::unique_ptr<HistogramSamples> samples =
      histogram_tester.GetHistogramSamplesSinceCreation(name);
  int sample_count = samples->GetCount(sample);
  EXPECT_GE(sample_count, expected_inclusive_lower_bound)
      << " for histogram " << name << " sample " << sample;
  EXPECT_EQ(sample_count, samples->TotalCount())
      << " for histogram " << name << " sample " << sample;
}

class IsHistoryURLSyncedChecker : public SingleClientStatusChangeChecker {
 public:
  IsHistoryURLSyncedChecker(const std::string& url,
                            fake_server::FakeServer* fake_server,
                            syncer::ProfileSyncService* service)
      : SingleClientStatusChangeChecker(service),
        url_(url),
        fake_server_(fake_server) {}

  // StatusChangeChecker implementation.
  bool IsExitConditionSatisfied(std::ostream* os) override {
    *os << "Waiting for URLs to be commited to the server";
    return fake_server_->GetCommittedHistoryURLs().count(url_) != 0;
  }

 private:
  const std::string url_;
  fake_server::FakeServer* fake_server_;
};

class IsIconURLSyncedChecker : public SingleClientStatusChangeChecker {
 public:
  IsIconURLSyncedChecker(const std::string& page_url,
                         const std::string& icon_url,
                         fake_server::FakeServer* fake_server,
                         syncer::ProfileSyncService* service)
      : SingleClientStatusChangeChecker(service),
        page_url_(page_url),
        icon_url_(icon_url),
        fake_server_(fake_server) {}

  // StatusChangeChecker implementation.
  bool IsExitConditionSatisfied(std::ostream* os) override {
    *os << "Waiting for URLs to be commited to the server";
    std::vector<sync_pb::SyncEntity> sessions =
        fake_server_->GetSyncEntitiesByModelType(syncer::SESSIONS);
    for (const auto& entity : sessions) {
      const sync_pb::SessionSpecifics& session_specifics =
          entity.specifics().session();
      if (!session_specifics.has_tab()) {
        continue;
      }
      for (int i = 0; i < session_specifics.tab().navigation_size(); i++) {
        const sync_pb::TabNavigation& nav =
            session_specifics.tab().navigation(i);
        if (nav.has_virtual_url() && nav.has_favicon_url() &&
            nav.virtual_url() == page_url_ && nav.favicon_url() == icon_url_) {
          return true;
        }
      }
    }
    return false;
  }

 private:
  const std::string page_url_;
  const std::string icon_url_;
  fake_server::FakeServer* fake_server_;
};

class SingleClientSessionsSyncTest : public SyncTest {
 public:
  SingleClientSessionsSyncTest() : SyncTest(SINGLE_CLIENT) {}
  ~SingleClientSessionsSyncTest() override {}

  void ExpectNavigationChain(const std::vector<GURL>& urls) {
    ScopedWindowMap windows;
    ASSERT_TRUE(GetLocalWindows(0, &windows));
    ASSERT_EQ(windows.begin()->second->wrapped_window.tabs.size(), 1u);
    sessions::SessionTab* tab =
        windows.begin()->second->wrapped_window.tabs[0].get();

    int index = 0;
    EXPECT_EQ(urls.size(), tab->navigations.size());
    for (auto it = tab->navigations.begin(); it != tab->navigations.end();
         ++it, ++index) {
      EXPECT_EQ(urls[index], it->virtual_url());
    }
  }

  // Block until the expected hierarchy is recorded on the FakeServer for
  // profile 0. This will time out if the hierarchy is never
  // recorded.
  void WaitForHierarchyOnServer(
      const fake_server::SessionsHierarchy& hierarchy) {
    SessionHierarchyMatchChecker checker(hierarchy, GetSyncService(0),
                                         GetFakeServer());
    EXPECT_TRUE(checker.Wait());
  }

  // Shortcut to call WaitForHierarchyOnServer for only |url| in a single
  // window.
  void WaitForURLOnServer(const GURL& url) {
    WaitForHierarchyOnServer({{url.spec()}});
  }

  // Simulates receiving list of accounts in the cookie jar from ListAccounts
  // endpoint. Adds |account_ids| into signed in accounts, notifies
  // ProfileSyncService and waits for change to propagate to sync engine.
  void UpdateCookieJarAccountsAndWait(std::vector<CoreAccountId> account_ids,
                                      bool expected_cookie_jar_mismatch) {
    std::vector<gaia::ListedAccount> accounts;
    for (const auto& account_id : account_ids) {
      gaia::ListedAccount signed_in_account;
      signed_in_account.id = account_id;
      accounts.push_back(signed_in_account);
    }
    base::RunLoop run_loop;
    EXPECT_EQ(expected_cookie_jar_mismatch,
              GetClient(0)->service()->HasCookieJarMismatch(accounts));
    GetClient(0)->service()->OnAccountsInCookieUpdatedWithCallback(
        accounts, run_loop.QuitClosure());
    run_loop.Run();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(SingleClientSessionsSyncTest);
};

IN_PROC_BROWSER_TEST_F(SingleClientSessionsSyncTest,
                       RequireProxyTabsForUiDelegate) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(CheckInitialState(0));

  sync_sessions::SessionSyncService* service =
      SessionSyncServiceFactory::GetForProfile(GetProfile(0));

  EXPECT_NE(nullptr, service->GetOpenTabsUIDelegate());
  ASSERT_TRUE(
      GetClient(0)->DisableSyncForType(syncer::UserSelectableType::kTabs));
  EXPECT_EQ(nullptr, service->GetOpenTabsUIDelegate());
}

IN_PROC_BROWSER_TEST_F(SingleClientSessionsSyncTest, Sanity) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  ASSERT_TRUE(CheckInitialState(0));

  // Add a new session to client 0 and wait for it to sync.
  ScopedWindowMap old_windows;
  GURL url = GURL(kURL1);
  ASSERT_TRUE(OpenTab(0, url));
  ASSERT_TRUE(GetLocalWindows(0, &old_windows));
  ASSERT_TRUE(UpdatedProgressMarkerChecker(GetSyncService(0)).Wait());

  // Get foreign session data from client 0.
  SyncedSessionVector sessions;
  ASSERT_FALSE(GetSessionData(0, &sessions));
  ASSERT_EQ(0U, sessions.size());

  // Verify client didn't change.
  ScopedWindowMap new_windows;
  ASSERT_TRUE(GetLocalWindows(0, &new_windows));
  ASSERT_TRUE(WindowsMatch(old_windows, new_windows));

  WaitForURLOnServer(url);

  EXPECT_THAT(GetFakeServer()->GetCommittedHistoryURLs(),
              UnorderedElementsAre(kURL1));
}

IN_PROC_BROWSER_TEST_F(SingleClientSessionsSyncTest, NavigateInTab) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(CheckInitialState(0));

  ASSERT_TRUE(OpenTab(0, GURL(kURL1)));
  WaitForHierarchyOnServer(SessionsHierarchy({{kURL1}}));

  NavigateTab(0, GURL(kURL2));
  WaitForHierarchyOnServer(SessionsHierarchy({{kURL2}}));

  EXPECT_THAT(GetFakeServer()->GetCommittedHistoryURLs(),
              UnorderedElementsAre(kURL1, kURL2));
}

IN_PROC_BROWSER_TEST_F(SingleClientSessionsSyncTest,
                       JavascriptHistoryReplaceState) {
  // Executing Javascript requires HTTP pages with an origin.
  ASSERT_TRUE(embedded_test_server()->Start());
  const std::string url1 =
      embedded_test_server()->GetURL("/sync/simple.html").spec();
  const std::string url2 =
      embedded_test_server()->GetURL("/replaced_history.html").spec();

  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(CheckInitialState(0));

  ASSERT_TRUE(OpenTab(0, GURL(url1)));
  WaitForHierarchyOnServer(SessionsHierarchy({{url1}}));

  ASSERT_TRUE(
      ExecJs(/*browser_index=*/0, /*tab_index=*/0,
             base::StringPrintf("history.replaceState({}, 'page 2', '%s')",
                                url2.c_str())));

  WaitForHierarchyOnServer(SessionsHierarchy({{url2}}));

  // Fetch the tab from the server for further verification.
  const std::vector<sync_pb::SyncEntity> entities =
      GetFakeServer()->GetSyncEntitiesByModelType(syncer::SESSIONS);
  const sync_pb::TabNavigation* tab_navigation = nullptr;
  for (const sync_pb::SyncEntity& entity : entities) {
    if (entity.specifics().session().tab().navigation_size() == 1 &&
        entity.specifics().session().tab().navigation(0).virtual_url() ==
            url2) {
      tab_navigation = &entity.specifics().session().tab().navigation(0);
    }
  }

  ASSERT_NE(nullptr, tab_navigation);
  EXPECT_TRUE(tab_navigation->has_replaced_navigation());
  EXPECT_EQ(url1, tab_navigation->replaced_navigation().first_committed_url());
}

IN_PROC_BROWSER_TEST_F(SingleClientSessionsSyncTest,
                       SessionsWithoutHistorySync) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  // If the user disables history sync on settings, but still enables tab sync,
  // then sessions should be synced but the server should be able to tell the
  // difference based on active datatypes.
  ASSERT_TRUE(
      GetClient(0)->DisableSyncForType(syncer::UserSelectableType::kHistory));
  ASSERT_TRUE(CheckInitialState(0));

  ASSERT_TRUE(OpenTab(0, GURL(kURL1)));
  WaitForHierarchyOnServer(SessionsHierarchy({{kURL1}}));

  NavigateTab(0, GURL(kURL2));
  WaitForHierarchyOnServer(SessionsHierarchy({{kURL2}}));

  EXPECT_THAT(GetFakeServer()->GetCommittedHistoryURLs(), IsEmpty());
}

IN_PROC_BROWSER_TEST_F(SingleClientSessionsSyncTest, NoSessions) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  WaitForHierarchyOnServer(SessionsHierarchy());
}

IN_PROC_BROWSER_TEST_F(SingleClientSessionsSyncTest, ChromeHistoryPage) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  ASSERT_TRUE(CheckInitialState(0));

  ASSERT_TRUE(OpenTab(0, GURL(chrome::kChromeUIHistoryURL)));
  WaitForURLOnServer(GURL(chrome::kChromeUIHistoryURL));
}

IN_PROC_BROWSER_TEST_F(SingleClientSessionsSyncTest, NavigateThenCloseTab) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(CheckInitialState(0));

  // Two tabs are opened initially.
  ASSERT_TRUE(OpenTab(0, GURL(kURL1)));
  ASSERT_TRUE(OpenTab(0, GURL(kURL2)));
  WaitForHierarchyOnServer(SessionsHierarchy({{kURL1, kURL2}}));

  // Close one of the two tabs immediately after issuing an navigation. We also
  // issue another navigation to make sure association logic kicks in.
  NavigateTab(0, GURL(kURL3));
  ASSERT_TRUE(WaitForTabsToLoad(0, {GURL(kURL1), GURL(kURL3)}));
  CloseTab(/*index=*/0, /*tab_index=*/1);
  NavigateTab(0, GURL(kURL4));

  ASSERT_TRUE(
      IsHistoryURLSyncedChecker(kURL4, GetFakeServer(), GetSyncService(0))
          .Wait());

  // All URLs should be synced, for synced history to be complete. In
  // particular, |kURL3| should be synced despite the tab being closed.
  EXPECT_TRUE(
      IsHistoryURLSyncedChecker(kURL3, GetFakeServer(), GetSyncService(0))
          .Wait());
}

class SingleClientSessionsWithDeferRecyclingSyncTest
    : public SingleClientSessionsSyncTest {
 public:
  SingleClientSessionsWithDeferRecyclingSyncTest() {
    features_.InitAndEnableFeature(
        sync_sessions::kDeferRecyclingOfSyncTabNodesIfUnsynced);
  }

 private:
  base::test::ScopedFeatureList features_;
};

IN_PROC_BROWSER_TEST_F(SingleClientSessionsWithDeferRecyclingSyncTest,
                       NavigateThenCloseTabThenOpenTab) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(CheckInitialState(0));

  // Two tabs are opened initially.
  ASSERT_TRUE(OpenTab(0, GURL(kURL1)));
  ASSERT_TRUE(OpenTab(0, GURL(kURL2)));
  WaitForHierarchyOnServer(SessionsHierarchy({{kURL1, kURL2}}));

  // Close one of the two tabs immediately after issuing an navigation. In
  // addition, a new tab is opened.
  NavigateTab(0, GURL(kURL3));
  ASSERT_TRUE(WaitForTabsToLoad(0, {GURL(kURL1), GURL(kURL3)}));
  CloseTab(/*index=*/0, /*tab_index=*/1);
  ASSERT_TRUE(OpenTab(0, GURL(kURL4)));

  ASSERT_TRUE(
      IsHistoryURLSyncedChecker(kURL4, GetFakeServer(), GetSyncService(0))
          .Wait());

  // All URLs should be synced, for synced history to be complete. In
  // particular, |kURL3| should be synced despite the tab being closed.
  EXPECT_TRUE(
      IsHistoryURLSyncedChecker(kURL3, GetFakeServer(), GetSyncService(0))
          .Wait());
}

IN_PROC_BROWSER_TEST_F(SingleClientSessionsSyncTest, TimestampMatchesHistory) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  ASSERT_TRUE(CheckInitialState(0));

  const GURL url(kURL1);

  ScopedWindowMap windows;
  ASSERT_TRUE(OpenTab(0, url));
  ASSERT_TRUE(GetLocalWindows(0, &windows));

  int found_navigations = 0;
  for (auto it = windows.begin(); it != windows.end(); ++it) {
    for (auto it2 = it->second->wrapped_window.tabs.begin();
         it2 != it->second->wrapped_window.tabs.end(); ++it2) {
      for (auto it3 = (*it2)->navigations.begin();
           it3 != (*it2)->navigations.end(); ++it3) {
        const base::Time timestamp = it3->timestamp();

        history::URLRow virtual_row;
        ASSERT_TRUE(GetUrlFromClient(0, it3->virtual_url(), &virtual_row));
        const base::Time history_timestamp = virtual_row.last_visit();
        // Propagated timestamps have millisecond-level resolution, so we avoid
        // exact comparison here (i.e. usecs might differ).
        ASSERT_EQ(0, (timestamp - history_timestamp).InMilliseconds());
        ++found_navigations;
      }
    }
  }
  ASSERT_EQ(1, found_navigations);
}

IN_PROC_BROWSER_TEST_F(SingleClientSessionsSyncTest, ResponseCodeIsPreserved) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  ASSERT_TRUE(CheckInitialState(0));

  const GURL url(kURL1);
  ScopedWindowMap windows;
  ASSERT_TRUE(OpenTab(0, url));
  ASSERT_TRUE(GetLocalWindows(0, &windows));

  int found_navigations = 0;
  for (auto it = windows.begin(); it != windows.end(); ++it) {
    for (auto it2 = it->second->wrapped_window.tabs.begin();
         it2 != it->second->wrapped_window.tabs.end(); ++it2) {
      for (auto it3 = (*it2)->navigations.begin();
           it3 != (*it2)->navigations.end(); ++it3) {
        EXPECT_EQ(200, it3->http_status_code());
        ++found_navigations;
      }
    }
  }
  ASSERT_EQ(1, found_navigations);
}

IN_PROC_BROWSER_TEST_F(SingleClientSessionsSyncTest, FragmentURLNavigation) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(CheckInitialState(0));

  const GURL url(kBaseFragmentURL);
  ASSERT_TRUE(OpenTab(0, url));
  WaitForURLOnServer(url);

  const GURL fragment_url(kSpecifiedFragmentURL);
  NavigateTab(0, fragment_url);
  WaitForURLOnServer(fragment_url);
}

IN_PROC_BROWSER_TEST_F(SingleClientSessionsSyncTest,
                       NavigationChainForwardBack) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(CheckInitialState(0));

  GURL first_url = GURL(kURL1);
  ASSERT_TRUE(OpenTab(0, first_url));
  WaitForURLOnServer(first_url);

  GURL second_url = GURL(kURL2);
  NavigateTab(0, second_url);
  WaitForURLOnServer(second_url);

  NavigateTabBack(0);
  WaitForURLOnServer(first_url);

  ExpectNavigationChain({first_url, second_url});

  NavigateTabForward(0);
  WaitForURLOnServer(second_url);

  ExpectNavigationChain({first_url, second_url});
}

IN_PROC_BROWSER_TEST_F(SingleClientSessionsSyncTest,
                       NavigationChainAlteredDestructively) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(CheckInitialState(0));

  GURL base_url = GURL(kURL1);
  ASSERT_TRUE(OpenTab(0, base_url));
  WaitForURLOnServer(base_url);

  GURL first_url = GURL(kURL2);
  NavigateTab(0, first_url);
  WaitForURLOnServer(first_url);

  // Check that the navigation chain matches the above sequence of {base_url,
  // first_url}.
  ExpectNavigationChain({base_url, first_url});

  NavigateTabBack(0);
  WaitForURLOnServer(base_url);

  GURL second_url = GURL(kURL3);
  NavigateTab(0, second_url);
  WaitForURLOnServer(second_url);

  NavigateTabBack(0);
  WaitForURLOnServer(base_url);

  // Check that the navigation chain contains second_url where first_url was
  // before.
  ExpectNavigationChain({base_url, second_url});
}

IN_PROC_BROWSER_TEST_F(SingleClientSessionsSyncTest, OpenNewTab) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(CheckInitialState(0));

  GURL base_url = GURL(kURL1);
  ASSERT_TRUE(OpenTabAtIndex(0, 0, base_url));

  WaitForURLOnServer(base_url);

  GURL new_tab_url = GURL(kURL2);
  ASSERT_TRUE(OpenTabAtIndex(0, 1, new_tab_url));

  WaitForHierarchyOnServer(
      SessionsHierarchy({{base_url.spec(), new_tab_url.spec()}}));
}

IN_PROC_BROWSER_TEST_F(SingleClientSessionsSyncTest, OpenNewWindow) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(CheckInitialState(0));

  GURL base_url = GURL(kURL1);
  ASSERT_TRUE(OpenTab(0, base_url));

  WaitForURLOnServer(base_url);

  GURL new_window_url = GURL(kURL2);
  AddBrowser(0);
  ASSERT_TRUE(OpenTab(1, new_window_url));

  WaitForHierarchyOnServer(
      SessionsHierarchy({{base_url.spec()}, {new_window_url.spec()}}));
}

IN_PROC_BROWSER_TEST_F(SingleClientSessionsSyncTest,
                       GarbageCollectionOfForeignSessions) {
  const std::string kForeignSessionTag = "ForeignSessionTag";
  const SessionID kWindowId = SessionID::FromSerializedValue(5);
  const SessionID kTabId1 = SessionID::FromSerializedValue(1);
  const SessionID kTabId2 = SessionID::FromSerializedValue(2);
  const base::Time kLastModifiedTime =
      base::Time::Now() - base::TimeDelta::FromDays(100);

  SessionSyncTestHelper helper;

  sync_pb::EntitySpecifics tab1;
  *tab1.mutable_session() =
      helper.BuildTabSpecifics(kForeignSessionTag, kWindowId, kTabId1);

  sync_pb::EntitySpecifics tab2;
  *tab2.mutable_session() =
      helper.BuildTabSpecifics(kForeignSessionTag, kWindowId, kTabId2);

  // |tab2| is orphan, i.e. not referenced by the header. We do this to verify
  // that such tabs are also subject to garbage collection.
  sync_pb::EntitySpecifics header;
  SessionSyncTestHelper::BuildSessionSpecifics(kForeignSessionTag,
                                               header.mutable_session());
  SessionSyncTestHelper::AddWindowSpecifics(kWindowId, {kTabId1},
                                            header.mutable_session());

  for (const sync_pb::EntitySpecifics& specifics : {tab1, tab2, header}) {
    GetFakeServer()->InjectEntity(
        syncer::PersistentUniqueClientEntity::CreateFromSpecificsForTesting(
            /*non_unique_name=*/"",
            sync_sessions::SessionStore::GetClientTag(specifics.session()),
            specifics,
            /*creation_time=*/syncer::TimeToProtoTime(kLastModifiedTime),
            /*last_modified_time=*/syncer::TimeToProtoTime(kLastModifiedTime)));
  }

  base::HistogramTester histogram_tester;
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  // Verify that all entities have been deleted.
  WaitForHierarchyOnServer(SessionsHierarchy());

  std::vector<sync_pb::SyncEntity> entities =
      fake_server_->GetSyncEntitiesByModelType(syncer::SESSIONS);
  for (const sync_pb::SyncEntity& entity : entities) {
    EXPECT_NE(kForeignSessionTag, entity.specifics().session().session_tag());
  }

  EXPECT_EQ(
      3, histogram_tester.GetBucketCount("Sync.ModelTypeEntityChange3.SESSION",
                                         /*LOCAL_DELETION=*/0));
}

// Regression test for crbug.com/915133 that verifies the browser doesn't crash
// if the server sends corrupt data during initial merge.
IN_PROC_BROWSER_TEST_F(SingleClientSessionsSyncTest, CorruptInitialForeignTab) {
  // Tabs with a negative node ID should be ignored.
  sync_pb::EntitySpecifics specifics;
  specifics.mutable_session()->mutable_tab();
  specifics.mutable_session()->set_tab_node_id(-1);

  GetFakeServer()->InjectEntity(
      syncer::PersistentUniqueClientEntity::CreateFromSpecificsForTesting(
          "somename", "someclienttag", specifics,
          /*creation_time=*/0,
          /*last_modified_time=*/0));

  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  // Foreign data should be empty.
  SyncedSessionVector sessions;
  ASSERT_FALSE(GetSessionData(0, &sessions));
  EXPECT_EQ(0U, sessions.size());
}

// Regression test for crbug.com/915133 that verifies the browser doesn't crash
// if the server sends corrupt data as incremental update.
IN_PROC_BROWSER_TEST_F(SingleClientSessionsSyncTest, CorruptForeignTabUpdate) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  // Tabs with a negative node ID should be ignored.
  sync_pb::EntitySpecifics specifics;
  specifics.mutable_session()->mutable_tab();
  specifics.mutable_session()->set_tab_node_id(-1);

  GetFakeServer()->InjectEntity(
      syncer::PersistentUniqueClientEntity::CreateFromSpecificsForTesting(
          "somename", "someclienttag", specifics,
          /*creation_time=*/0,
          /*last_modified_time=*/0));

  // Mimic a browser restart to force a reconfiguration and fetch updates.
  GetClient(0)->StopSyncServiceWithoutClearingData();
  ASSERT_TRUE(GetClient(0)->StartSyncService());

  // Foreign data should be empty.
  SyncedSessionVector sessions;
  ASSERT_FALSE(GetSessionData(0, &sessions));
  EXPECT_EQ(0U, sessions.size());
}

IN_PROC_BROWSER_TEST_F(SingleClientSessionsSyncTest, TabMovedToOtherWindow) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(CheckInitialState(0));

  GURL base_url = GURL(kURL1);
  GURL moved_tab_url = GURL(kURL2);

  ASSERT_TRUE(OpenTab(0, base_url));
  ASSERT_TRUE(OpenTabAtIndex(0, 1, moved_tab_url));

  GURL new_window_url = GURL(kURL3);
  AddBrowser(0);
  ASSERT_TRUE(OpenTab(1, new_window_url));

  WaitForHierarchyOnServer(SessionsHierarchy(
      {{base_url.spec(), moved_tab_url.spec()}, {new_window_url.spec()}}));

  // Move tab 1 in browser 0 to browser 1.
  MoveTab(0, 1, 1);

  WaitForHierarchyOnServer(SessionsHierarchy(
      {{base_url.spec()}, {new_window_url.spec(), moved_tab_url.spec()}}));
}

IN_PROC_BROWSER_TEST_F(SingleClientSessionsSyncTest, CookieJarMismatch) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  ASSERT_TRUE(CheckInitialState(0));

  // Simulate empty list of accounts in the cookie jar. This will record cookie
  // jar mismatch.
  UpdateCookieJarAccountsAndWait({},
                                 /*expected_cookie_jar_mismatch=*/true);
  // The HistogramTester objects are scoped to allow more precise verification.
  {
    HistogramTester histogram_tester;

    // Add a new session to client 0 and wait for it to sync.
    GURL url = GURL(kURL1);
    ASSERT_TRUE(OpenTab(0, url));
    WaitForURLOnServer(url);

    sync_pb::ClientToServerMessage message;
    ASSERT_TRUE(GetFakeServer()->GetLastCommitMessage(&message));
    ASSERT_TRUE(message.commit().config_params().cookie_jar_mismatch());

    // It is possible that multiple sync cycles occurred during the call to
    // OpenTab, which would cause multiple identical samples.
    ExpectUniqueSampleGE(histogram_tester, "Sync.CookieJarMatchOnNavigation",
                         /*sample=*/false,
                         /*expected_inclusive_lower_bound=*/1);
    ExpectUniqueSampleGE(histogram_tester, "Sync.CookieJarEmptyOnMismatch",
                         /*sample=*/true,
                         /*expected_inclusive_lower_bound=*/1);
  }

  // Avoid interferences from actual IdentityManager trying to fetch gaia
  // account information, which would exercise
  // ProfileSyncService::OnAccountsInCookieUpdated().
  signin::CancelAllOngoingGaiaCookieOperations(
      IdentityManagerFactory::GetForProfile(GetProfile(0)));

  // Trigger a cookie jar change (user signing in to content area).
  // Updating the cookie jar has to travel to the sync engine. It is possible
  // something is already running or scheduled to run on the sync thread. We
  // want to block here and not create the HistogramTester below until we know
  // the cookie jar stats have been updated.
  UpdateCookieJarAccountsAndWait(
      {GetClient(0)->service()->GetAuthenticatedAccountInfo().account_id},
      /*expected_cookie_jar_mismatch=*/false);

  {
    HistogramTester histogram_tester;

    // Trigger a sync and wait for it.
    GURL url = GURL(kURL2);
    NavigateTab(0, url);
    WaitForURLOnServer(url);

    ASSERT_NE(
        0, histogram_tester.GetBucketCount("Sync.PostedClientToServerMessage",
                                           /*COMMIT=*/1));

    // Verify the cookie jar mismatch bool is set to false.
    sync_pb::ClientToServerMessage message;
    ASSERT_TRUE(GetFakeServer()->GetLastCommitMessage(&message));
    EXPECT_FALSE(message.commit().config_params().cookie_jar_mismatch())
        << *syncer::ClientToServerMessageToValue(message, true);

    // Verify the histograms were recorded properly.
    ExpectUniqueSampleGE(histogram_tester, "Sync.CookieJarMatchOnNavigation",
                         /*sample=*/true, /*expected_inclusive_lower_bound=*/1);
    histogram_tester.ExpectTotalCount("Sync.CookieJarEmptyOnMismatch", 0);
  }
}

IN_PROC_BROWSER_TEST_F(SingleClientSessionsSyncTest,
                       ShouldNotifyLoadedIconUrl) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(CheckInitialState(0));

  // Url with endoded 1 pixel icon.
  std::string icon_url =
      "data:image/png;base64,"
      "R0lGODlhAQABAIAAAAAAAP///yH5BAEAAAAALAAAAAABAAEAAAIBRAA7";
  std::string page_url =
      "data:text/html,<html><title>TestWithFavicon</title><link rel=icon "
      "href=" +
      icon_url + " /></html>";

  ASSERT_TRUE(OpenTab(0, GURL(page_url)));

  IsIconURLSyncedChecker checker(page_url, icon_url, GetFakeServer(),
                                 GetSyncService(0));
  EXPECT_TRUE(checker.Wait());
}

}  // namespace
