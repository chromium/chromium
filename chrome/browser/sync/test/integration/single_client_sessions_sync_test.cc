// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/base64.h"
#include "base/containers/to_vector.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/test_timeouts.h"
#include "base/threading/thread_restrictions.h"
#include "build/build_config.h"
#include "chrome/browser/browser_features.h"
#include "chrome/browser/favicon/favicon_service_factory.h"
#include "chrome/browser/favicon/large_icon_service_factory.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/sync/session_sync_service_factory.h"
#include "chrome/browser/sync/test/integration/history_helper.h"
#include "chrome/browser/sync/test/integration/session_hierarchy_match_checker.h"
#include "chrome/browser/sync/test/integration/sessions_helper.h"
#include "chrome/browser/sync/test/integration/sync_service_impl_harness.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "chrome/browser/sync/test/integration/updated_progress_marker_checker.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/common/webui_url_constants.h"
#include "components/favicon/core/favicon_service.h"
#include "components/favicon/core/large_icon_service_impl.h"
#include "components/favicon_base/favicon_types.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/browser/history_types.h"
#include "components/sessions/core/session_types.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/accounts_in_cookie_jar_info.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "components/sync/base/time.h"
#include "components/sync/engine/cycle/entity_change_metric_recording.h"
#include "components/sync/protocol/entity_specifics.pb.h"
#include "components/sync/protocol/proto_value_conversions.h"
#include "components/sync/protocol/session_specifics.pb.h"
#include "components/sync/protocol/sync.pb.h"
#include "components/sync/protocol/sync_entity.pb.h"
#include "components/sync/service/glue/sync_transport_data_prefs.h"
#include "components/sync/test/sessions_hierarchy.h"
#include "components/sync_sessions/session_store.h"
#include "components/sync_sessions/session_sync_service.h"
#include "components/sync_sessions/session_sync_test_helper.h"
#include "content/public/test/browser_test.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

using fake_server::SessionsHierarchy;
using history_helper::GetUrlFromClient;
using sessions_helper::CheckInitialState;
using sessions_helper::CloseTab;
using sessions_helper::GetLocalWindows;
using sessions_helper::GetSessionData;
using sessions_helper::MoveTab;
using sessions_helper::NavigateTab;
using sessions_helper::NavigateTabBack;
using sessions_helper::NavigateTabForward;
using sessions_helper::OpenTab;
using sessions_helper::OpenTabAtIndex;
using sessions_helper::ScopedWindowMap;
using sessions_helper::SessionEntitiesChecker;
using sessions_helper::SyncedSessionVector;
using sessions_helper::WaitForTabsToLoad;
using sessions_helper::WindowsMatch;
using sync_sessions::SessionSyncTestHelper;
using testing::_;
using testing::UnorderedElementsAre;

MATCHER_P(SessionHeader, session_tag, "") {
  return arg.has_header() && arg.session_tag() == session_tag;
}

static const char* kBaseFragmentURL =
    "data:text/html,<html><title>Fragment</title><body></body></html>";
static const char* kSpecifiedFragmentURL =
    "data:text/html,<html><title>Fragment</title><body></body></html>#fragment";

std::unique_ptr<net::test_server::HttpResponse> FaviconServerRequestHandler(
    const net::test_server::HttpRequest& request) {
  // An arbitrary 16x16 png (opaque black square), Base64 encoded.
  const std::string kTestPngBase64 =
      "iVBORw0KGgoAAAANSUhEUgAAABAAAAAQCAQAAAC1+jfqAAAAE0lEQVR42mNk+M+"
      "AFzCOKhhJCgBrLxABLz0PwwAAAABJRU5ErkJggg==";
  std::string content;
  base::Base64Decode(kTestPngBase64, &content);

  std::unique_ptr<net::test_server::BasicHttpResponse> http_response(
      new net::test_server::BasicHttpResponse);
  http_response->set_content(content);
  http_response->set_code(net::HTTP_OK);
  return std::move(http_response);
}

class IsIconURLSyncedChecker : public SingleClientStatusChangeChecker {
 public:
  IsIconURLSyncedChecker(const std::string& page_url,
                         const std::string& icon_url,
                         fake_server::FakeServer* fake_server,
                         syncer::SyncServiceImpl* service)
      : SingleClientStatusChangeChecker(service),
        page_url_(page_url),
        icon_url_(icon_url),
        fake_server_(fake_server) {}

  // StatusChangeChecker implementation.
  bool IsExitConditionSatisfied(std::ostream* os) override {
    *os << "Waiting for URLs to be commited to the server";
    std::vector<sync_pb::SyncEntity> sessions =
        fake_server_->GetSyncEntitiesByDataType(syncer::SESSIONS);
    for (const sync_pb::SyncEntity& entity : sessions) {
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
  const raw_ptr<fake_server::FakeServer> fake_server_;
};

// Checker to block until the history DB for |profile| does / does not have a
// favicon for |page_url| (depending on |should_be_available|).
class FaviconForPageUrlAvailableChecker : public StatusChangeChecker {
 public:
  FaviconForPageUrlAvailableChecker(Profile* profile,
                                    const GURL& page_url,
                                    bool should_be_available)
      : profile_(profile),
        page_url_(page_url),
        should_be_available_(should_be_available) {
    history::HistoryService* history_service =
        HistoryServiceFactory::GetForProfile(
            profile_, ServiceAccessType::EXPLICIT_ACCESS);
    callback_subscription_ =
        history_service->AddFaviconsChangedCallback(base::BindRepeating(
            &FaviconForPageUrlAvailableChecker::OnFaviconsChanged,
            base::Unretained(this)));

    // Load the state asynchronously to figure out if further waiting is needed.
    CheckExitConditionAsync();
  }
  ~FaviconForPageUrlAvailableChecker() override = default;

 protected:
  // StatusChangeChecker implementation.
  bool IsExitConditionSatisfied(std::ostream* os) override {
    return exit_condition_satisfied_;
  }

 private:
  void OnFaviconsChanged(const std::set<GURL>& page_urls,
                         const GURL& icon_url) {
    for (const GURL& page_url : page_urls) {
      if (page_url == page_url_) {
        CheckExitConditionAsync();
      }
    }
  }

  void CheckExitConditionAsync() {
    favicon::FaviconService* favicon_service =
        FaviconServiceFactory::GetForProfile(
            profile_, ServiceAccessType::EXPLICIT_ACCESS);
    favicon_service->GetFaviconImageForPageURL(
        page_url_,
        base::BindOnce(&FaviconForPageUrlAvailableChecker::OnFaviconLoaded,
                       base::Unretained(this)),
        &tracker_);
  }

  void OnFaviconLoaded(const favicon_base::FaviconImageResult& result) {
    bool is_available = !result.image.IsEmpty();
    exit_condition_satisfied_ = (is_available == should_be_available_);
    CheckExitCondition();
  }

  const raw_ptr<Profile> profile_;
  const GURL page_url_;
  const bool should_be_available_;
  base::CallbackListSubscription callback_subscription_;
  bool exit_condition_satisfied_ = false;
  base::CancelableTaskTracker tracker_;
};

class SingleClientSessionsSyncTest : public SyncTest {
 public:
  SingleClientSessionsSyncTest() : SyncTest(SINGLE_CLIENT) {}

  SingleClientSessionsSyncTest(const SingleClientSessionsSyncTest&) = delete;
  SingleClientSessionsSyncTest& operator=(const SingleClientSessionsSyncTest&) =
      delete;

  ~SingleClientSessionsSyncTest() override = default;

  void ExpectNavigationChain(const std::vector<GURL>& urls) {
    ScopedWindowMap windows;
    ASSERT_TRUE(GetLocalWindows(0, &windows));
    ASSERT_EQ(windows.begin()->second->wrapped_window.tabs.size(), 1u);
    sessions::SessionTab* tab =
        windows.begin()->second->wrapped_window.tabs[0].get();

    int index = 0;
    EXPECT_EQ(urls.size(), tab->navigations.size());
    for (const sessions::SerializedNavigationEntry& navigation :
         tab->navigations) {
      EXPECT_EQ(urls[index], navigation.virtual_url());
      index++;
    }
  }

  // Block until the expected hierarchy is recorded on the FakeServer for
  // profile 0. This will time out if the hierarchy is never recorded.
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
  // SyncServiceImpl and waits for change to propagate to sync engine.
  void UpdateCookieJarAccountsAndWait(std::vector<CoreAccountInfo> accounts,
                                      bool expected_cookie_jar_mismatch) {
    std::vector<gaia::ListedAccount> signed_in_accounts =
        base::ToVector(accounts, [](const CoreAccountInfo& account) {
          gaia::ListedAccount listed_account;
          listed_account.id = account.account_id;
          listed_account.gaia_id = account.gaia;
          listed_account.email = account.email;
          return listed_account;
        });
    signin::AccountsInCookieJarInfo cookies(/*accounts_are_fresh=*/true,
                                            signed_in_accounts);
    base::RunLoop run_loop;
    EXPECT_EQ(expected_cookie_jar_mismatch,
              GetClient(0)->service()->HasCookieJarMismatch(
                  cookies.GetPotentiallyInvalidSignedInAccounts()));
    GetClient(0)->service()->OnAccountsInCookieUpdatedWithCallback(
        cookies, run_loop.QuitClosure());
    run_loop.Run();
  }

 protected:
  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(embedded_test_server()->Start());
    SyncTest::SetUpOnMainThread();
  }
};

IN_PROC_BROWSER_TEST_F(SingleClientSessionsSyncTest,
                       RequireUserSelectableTypeTabsForUiDelegate) {
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
  GURL url = embedded_test_server()->GetURL("/sync/simple.html");
  ASSERT_TRUE(OpenTab(0, url));
  EXPECT_TRUE(GetLocalWindows(0, &old_windows));
  EXPECT_TRUE(UpdatedProgressMarkerChecker(GetSyncService(0)).Wait());

  // Get foreign session data from client 0.
  SyncedSessionVector sessions;
  EXPECT_FALSE(GetSessionData(0, &sessions));
  EXPECT_EQ(0U, sessions.size());

  // Verify client didn't change.
  ScopedWindowMap new_windows;
  ASSERT_TRUE(GetLocalWindows(0, &new_windows));
  EXPECT_TRUE(WindowsMatch(old_windows, new_windows));

  WaitForURLOnServer(url);
}

IN_PROC_BROWSER_TEST_F(SingleClientSessionsSyncTest, PRE_SessionStartTime) {
  const base::Time initial_time = base::Time::Now();
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  ASSERT_TRUE(CheckInitialState(0));

  // Add a tab and wait for it to sync.
  GURL url =
      embedded_test_server()->GetURL("www.host1.com", "/sync/simple.html");
  ASSERT_TRUE(OpenTab(0, url));
  EXPECT_TRUE(UpdatedProgressMarkerChecker(GetSyncService(0)).Wait());

  WaitForURLOnServer(url);

  // Ensure the session start time was properly populated in the header.
  std::vector<sync_pb::SyncEntity> entities =
      fake_server_->GetSyncEntitiesByDataType(syncer::SESSIONS);
  EXPECT_EQ(entities.size(), 2u);
  bool found_header = false;
  for (const sync_pb::SyncEntity& entity : entities) {
    const sync_pb::SessionSpecifics session = entity.specifics().session();
    if (session.has_header()) {
      found_header = true;
      EXPECT_TRUE(session.header().has_session_start_time_unix_epoch_millis());
      EXPECT_GE(base::Time::FromMillisecondsSinceUnixEpoch(
                    session.header().session_start_time_unix_epoch_millis()),
                initial_time);
    }
  }
  EXPECT_TRUE(found_header);
}

IN_PROC_BROWSER_TEST_F(SingleClientSessionsSyncTest, SessionStartTime) {
  const base::Time initial_time = base::Time::Now();
  ASSERT_TRUE(SetupClients()) << "SetupSync() failed.";

  // Open another tab and wait for it to sync, just to ensure everything's up
  // to date.
  GURL url =
      embedded_test_server()->GetURL("www.host2.com", "/sync/simple.html");
  ASSERT_TRUE(OpenTab(0, url));
  EXPECT_TRUE(UpdatedProgressMarkerChecker(GetSyncService(0)).Wait());

  WaitForURLOnServer(url);

  // Ensure the session start time in the header was *not* updated.
  std::vector<sync_pb::SyncEntity> entities =
      fake_server_->GetSyncEntitiesByDataType(syncer::SESSIONS);
  EXPECT_EQ(entities.size(), 2u);
  bool found_header = false;
  for (const sync_pb::SyncEntity& entity : entities) {
    const sync_pb::SessionSpecifics session = entity.specifics().session();
    if (session.has_header()) {
      found_header = true;
      EXPECT_TRUE(session.header().has_session_start_time_unix_epoch_millis());
      // `initial_time` is after the browser restart, so the session start time
      // should be before that.
      EXPECT_LT(base::Time::FromMillisecondsSinceUnixEpoch(
                    session.header().session_start_time_unix_epoch_millis()),
                initial_time);
    }
  }
  EXPECT_TRUE(found_header);
}

#if !BUILDFLAG(IS_CHROMEOS_ASH)
// Regression test for crbug.com/361256057.
IN_PROC_BROWSER_TEST_F(SingleClientSessionsSyncTest, UpdateSessionTag) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  ASSERT_TRUE(CheckInitialState(0));

  std::string first_cache_guid;
  {
    syncer::SyncTransportDataPrefs prefs(
        GetProfile(0)->GetPrefs(),
        GetClient(0)->GetGaiaIdHashForPrimaryAccount());
    first_cache_guid = prefs.GetCacheGuid();
  }

  // Note: The only tab that exists is an NTP, which is not interesting and thus
  // not uploaded. So the only SESSIONS entity on the server is now a header.
  ASSERT_TRUE(SessionEntitiesChecker(
                  UnorderedElementsAre(SessionHeader(first_cache_guid)))
                  .Wait());

  // Disable Sync, then turn it on again with a different account.
  GetClient(0)->SignOutPrimaryAccount();
  GetClient(0)->SetUsernameForFutureSignins("account2@gmail.com");
  ASSERT_TRUE(GetClient(0)->SetupSync());

  std::string second_cache_guid;
  {
    syncer::SyncTransportDataPrefs prefs(
        GetProfile(0)->GetPrefs(),
        GetClient(0)->GetGaiaIdHashForPrimaryAccount());
    second_cache_guid = prefs.GetCacheGuid();
  }
  ASSERT_NE(first_cache_guid, second_cache_guid);

  // Ensure that a session header with the new cache GUID was uploaded.
  EXPECT_TRUE(SessionEntitiesChecker(
                  UnorderedElementsAre(SessionHeader(first_cache_guid),
                                       SessionHeader(second_cache_guid)))
                  .Wait());
}
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)

IN_PROC_BROWSER_TEST_F(SingleClientSessionsSyncTest, NavigateInTab) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(CheckInitialState(0));

  GURL url1 =
      embedded_test_server()->GetURL("www.host1.com", "/sync/simple.html");
  GURL url2 =
      embedded_test_server()->GetURL("www.host2.com", "/sync/simple.html");

  ASSERT_TRUE(OpenTab(0, url1));
  WaitForHierarchyOnServer(SessionsHierarchy({{url1.spec()}}));

  NavigateTab(0, url2);
  WaitForHierarchyOnServer(SessionsHierarchy({{url2.spec()}}));
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

  GURL url1 =
      embedded_test_server()->GetURL("www.host1.com", "/sync/simple.html");
  GURL url2 =
      embedded_test_server()->GetURL("www.host2.com", "/sync/simple.html");

  ASSERT_TRUE(OpenTab(0, url1));
  WaitForHierarchyOnServer(SessionsHierarchy({{url1.spec()}}));

  NavigateTab(0, url2);
  WaitForHierarchyOnServer(SessionsHierarchy({{url2.spec()}}));
}

IN_PROC_BROWSER_TEST_F(SingleClientSessionsSyncTest, NoSessions) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  WaitForHierarchyOnServer(SessionsHierarchy());
}

IN_PROC_BROWSER_TEST_F(SingleClientSessionsSyncTest, TimestampMatchesHistory) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  ASSERT_TRUE(CheckInitialState(0));

  GURL url = embedded_test_server()->GetURL("/sync/simple.html");

  ScopedWindowMap windows;
  ASSERT_TRUE(OpenTab(0, url));
  ASSERT_TRUE(GetLocalWindows(0, &windows));

  int found_navigations = 0;
  for (const auto& [window_id, window] : windows) {
    for (const std::unique_ptr<sessions::SessionTab>& tab :
         window->wrapped_window.tabs) {
      for (const sessions::SerializedNavigationEntry& navigation :
           tab->navigations) {
        history::URLRow virtual_row;
        ASSERT_TRUE(
            GetUrlFromClient(0, navigation.virtual_url(), &virtual_row));
        const base::Time history_timestamp = virtual_row.last_visit();
        // Propagated timestamps have millisecond-level resolution, so we avoid
        // exact comparison here (i.e. usecs might differ).
        ASSERT_EQ(
            0, (navigation.timestamp() - history_timestamp).InMilliseconds());
        ++found_navigations;
      }
    }
  }
  ASSERT_EQ(1, found_navigations);
}

IN_PROC_BROWSER_TEST_F(SingleClientSessionsSyncTest, ResponseCodeIsPreserved) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  ASSERT_TRUE(CheckInitialState(0));

  GURL url = embedded_test_server()->GetURL("/sync/simple.html");

  ScopedWindowMap windows;
  ASSERT_TRUE(OpenTab(0, url));
  ASSERT_TRUE(GetLocalWindows(0, &windows));

  int found_navigations = 0;
  for (const auto& [window_id, window] : windows) {
    for (const std::unique_ptr<sessions::SessionTab>& tab :
         window->wrapped_window.tabs) {
      for (const sessions::SerializedNavigationEntry& navigation :
           tab->navigations) {
        EXPECT_EQ(200, navigation.http_status_code());
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

  GURL first_url =
      embedded_test_server()->GetURL("www.host1.com", "/sync/simple.html");
  ASSERT_TRUE(OpenTab(0, first_url));
  WaitForURLOnServer(first_url);

  GURL second_url =
      embedded_test_server()->GetURL("www.host2.com", "/sync/simple.html");
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

  GURL base_url =
      embedded_test_server()->GetURL("www.base.com", "/sync/simple.html");
  ASSERT_TRUE(OpenTab(0, base_url));
  WaitForURLOnServer(base_url);

  GURL first_url =
      embedded_test_server()->GetURL("www.host1.com", "/sync/simple.html");
  NavigateTab(0, first_url);
  WaitForURLOnServer(first_url);

  // Check that the navigation chain matches the above sequence of {base_url,
  // first_url}.
  ExpectNavigationChain({base_url, first_url});

  NavigateTabBack(0);
  WaitForURLOnServer(base_url);

  GURL second_url =
      embedded_test_server()->GetURL("www.host2.com", "/sync/simple.html");
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

  GURL base_url =
      embedded_test_server()->GetURL("www.host1.com", "/sync/simple.html");
  ASSERT_TRUE(OpenTabAtIndex(0, 0, base_url));

  WaitForURLOnServer(base_url);

  GURL new_tab_url =
      embedded_test_server()->GetURL("www.host2.com", "/sync/simple.html");
  ASSERT_TRUE(OpenTabAtIndex(0, 1, new_tab_url));

  WaitForHierarchyOnServer(
      SessionsHierarchy({{base_url.spec(), new_tab_url.spec()}}));
}

IN_PROC_BROWSER_TEST_F(SingleClientSessionsSyncTest, OpenNewWindow) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(CheckInitialState(0));

  GURL base_url =
      embedded_test_server()->GetURL("www.host1.com", "/sync/simple.html");
  ASSERT_TRUE(OpenTab(0, base_url));

  WaitForURLOnServer(base_url);

  GURL new_window_url =
      embedded_test_server()->GetURL("www.host2.com", "/sync/simple.html");
  AddBrowser(0);
  ASSERT_TRUE(OpenTab(1, new_window_url));

  WaitForHierarchyOnServer(
      SessionsHierarchy({{base_url.spec()}, {new_window_url.spec()}}));
}

IN_PROC_BROWSER_TEST_F(SingleClientSessionsSyncTest,
                       GarbageCollectionOfForeignSessions) {
  const std::string kForeignSessionTag = "ForeignSessionTag";
  const std::string kForeignClientName = "ForeignClientName";
  const SessionID kWindowId = SessionID::FromSerializedValue(5);
  const SessionID kTabId1 = SessionID::FromSerializedValue(1);
  const SessionID kTabId2 = SessionID::FromSerializedValue(2);
  const base::Time kLastModifiedTime = base::Time::Now() - base::Days(100);

  SessionSyncTestHelper helper;
  sync_pb::SessionSpecifics tab1 =
      helper.BuildTabSpecifics(kForeignSessionTag, kWindowId, kTabId1);
  sync_pb::SessionSpecifics tab2 =
      helper.BuildTabSpecifics(kForeignSessionTag, kWindowId, kTabId2);

  // |tab2| is orphan, i.e. not referenced by the header. We do this to verify
  // that such tabs are also subject to garbage collection.
  sync_pb::SessionSpecifics header =
      SessionSyncTestHelper::BuildHeaderSpecificsWithoutWindows(
          kForeignSessionTag, kForeignClientName);
  SessionSyncTestHelper::AddWindowSpecifics(kWindowId, {kTabId1}, &header);

  for (const sync_pb::SessionSpecifics& specifics : {tab1, tab2, header}) {
    sync_pb::EntitySpecifics entity;
    *entity.mutable_session() = specifics;
    GetFakeServer()->InjectEntity(
        syncer::PersistentUniqueClientEntity::CreateFromSpecificsForTesting(
            /*non_unique_name=*/"",
            sync_sessions::SessionStore::GetClientTag(entity.session()), entity,
            /*creation_time=*/syncer::TimeToProtoTime(kLastModifiedTime),
            /*last_modified_time=*/syncer::TimeToProtoTime(kLastModifiedTime)));
  }

  base::HistogramTester histogram_tester;
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  // Verify that all entities have been deleted.
  WaitForHierarchyOnServer(SessionsHierarchy());

  std::vector<sync_pb::SyncEntity> entities =
      fake_server_->GetSyncEntitiesByDataType(syncer::SESSIONS);
  for (const sync_pb::SyncEntity& entity : entities) {
    EXPECT_NE(kForeignSessionTag, entity.specifics().session().session_tag());
  }

  EXPECT_EQ(3, histogram_tester.GetBucketCount(
                   "Sync.DataTypeEntityChange.SESSION",
                   syncer::DataTypeEntityChange::kLocalDeletion));
}

IN_PROC_BROWSER_TEST_F(SingleClientSessionsSyncTest,
                       GarbageCollectionOfForeignOrphanTabWithoutHeader) {
  const std::string kForeignSessionTag = "ForeignSessionTag";
  const SessionID kWindowId = SessionID::FromSerializedValue(5);
  const SessionID kTabId1 = SessionID::FromSerializedValue(1);
  const SessionID kTabId2 = SessionID::FromSerializedValue(2);
  const base::Time kLastModifiedTime = base::Time::Now() - base::Days(100);

  SessionSyncTestHelper helper;

  // There are two orphan tab entities without a header entity.
  sync_pb::EntitySpecifics tab1;
  *tab1.mutable_session() =
      helper.BuildTabSpecifics(kForeignSessionTag, kWindowId, kTabId1);

  sync_pb::EntitySpecifics tab2;
  *tab2.mutable_session() =
      helper.BuildTabSpecifics(kForeignSessionTag, kWindowId, kTabId2);

  for (const sync_pb::EntitySpecifics& specifics : {tab1, tab2}) {
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
      fake_server_->GetSyncEntitiesByDataType(syncer::SESSIONS);
  for (const sync_pb::SyncEntity& entity : entities) {
    EXPECT_NE(kForeignSessionTag, entity.specifics().session().session_tag());
  }

  EXPECT_EQ(2, histogram_tester.GetBucketCount(
                   "Sync.DataTypeEntityChange.SESSION",
                   syncer::DataTypeEntityChange::kLocalDeletion));
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

  // Mimic a browser restart by forcing a refresh to get updates.
  GetSyncService(0)->TriggerRefresh({syncer::SESSIONS});
  EXPECT_TRUE(UpdatedProgressMarkerChecker(GetSyncService(0)).Wait());

  // Foreign data should be empty.
  SyncedSessionVector sessions;
  ASSERT_FALSE(GetSessionData(0, &sessions));
  EXPECT_EQ(0U, sessions.size());
}

IN_PROC_BROWSER_TEST_F(SingleClientSessionsSyncTest, TabMovedToOtherWindow) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(CheckInitialState(0));

  GURL base_url =
      embedded_test_server()->GetURL("www.base.com", "/sync/simple.html");
  GURL moved_tab_url =
      embedded_test_server()->GetURL("www.host1.com", "/sync/simple.html");

  ASSERT_TRUE(OpenTab(0, base_url));
  ASSERT_TRUE(OpenTabAtIndex(0, 1, moved_tab_url));

  GURL new_window_url =
      embedded_test_server()->GetURL("www.host2.com", "/sync/simple.html");
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

  // Simulate empty list of accounts in the cookie jar.
  UpdateCookieJarAccountsAndWait({},
                                 /*expected_cookie_jar_mismatch=*/true);

  // Add a new session to client 0 and wait for it to sync.
  GURL url1 =
      embedded_test_server()->GetURL("www.host1.com", "/sync/simple.html");
  ASSERT_TRUE(OpenTab(0, url1));
  WaitForURLOnServer(url1);

  // Verify the cookie jar mismatch bool is set to true.
  sync_pb::ClientToServerMessage first_commit;
  ASSERT_TRUE(GetFakeServer()->GetLastCommitMessage(&first_commit));
  EXPECT_TRUE(first_commit.commit().config_params().cookie_jar_mismatch())
      << syncer::ClientToServerMessageToValue(first_commit, /*options=*/{});

  // Avoid interferences from actual IdentityManager trying to fetch gaia
  // account information, which would exercise
  // SyncServiceImpl::OnAccountsInCookieUpdated().
  signin::CancelAllOngoingGaiaCookieOperations(
      IdentityManagerFactory::GetForProfile(GetProfile(0)));

  // Trigger a cookie jar change (user signing in to content area).
  // Updating the cookie jar has to travel to the sync engine. It is possible
  // something is already running or scheduled to run on the sync thread. We
  // want to block here until we know the cookie jar stats have been updated.
  UpdateCookieJarAccountsAndWait({GetClient(0)->service()->GetAccountInfo()},
                                 /*expected_cookie_jar_mismatch=*/false);

  // Trigger a sync and wait for it.
  GURL url2 =
      embedded_test_server()->GetURL("www.host2.com", "/sync/simple.html");
  NavigateTab(0, url2);
  WaitForURLOnServer(url2);

  // Verify the cookie jar mismatch bool is set to false.
  sync_pb::ClientToServerMessage second_commit;
  ASSERT_TRUE(GetFakeServer()->GetLastCommitMessage(&second_commit));
  EXPECT_FALSE(second_commit.commit().config_params().cookie_jar_mismatch())
      << syncer::ClientToServerMessageToValue(second_commit, /*options=*/{});
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

class SingleClientSessionsSyncTestWithFaviconTestServer
    : public SingleClientSessionsSyncTest {
 public:
  SingleClientSessionsSyncTestWithFaviconTestServer() = default;

  SingleClientSessionsSyncTestWithFaviconTestServer(
      const SingleClientSessionsSyncTestWithFaviconTestServer&) = delete;
  SingleClientSessionsSyncTestWithFaviconTestServer& operator=(
      const SingleClientSessionsSyncTestWithFaviconTestServer&) = delete;

  ~SingleClientSessionsSyncTestWithFaviconTestServer() override = default;

 protected:
  void SetUpOnMainThread() override {
    // Mock favicon server response.
    embedded_test_server()->RegisterRequestHandler(
        base::BindRepeating(&FaviconServerRequestHandler));
    SingleClientSessionsSyncTest::SetUpOnMainThread();
  }
};

IN_PROC_BROWSER_TEST_F(SingleClientSessionsSyncTestWithFaviconTestServer,
                       ShouldDeleteOnDemandIconsOnSessionsDisabled) {
  const std::string kForeignSessionTag = "ForeignSessionTag";
  const std::string kForeignClientName = "ForeignClientName";
  const SessionID kWindowId = SessionID::FromSerializedValue(5);
  const SessionID kTabId = SessionID::FromSerializedValue(1);
  const base::Time kLastModifiedTime = base::Time::Now();

  // Inject fake data on the server.
  SessionSyncTestHelper helper;
  sync_pb::SessionSpecifics tab =
      helper.BuildTabSpecifics(kForeignSessionTag, kWindowId, kTabId);
  sync_pb::SessionSpecifics header =
      SessionSyncTestHelper::BuildHeaderSpecificsWithoutWindows(
          kForeignSessionTag, kForeignClientName);
  SessionSyncTestHelper::AddWindowSpecifics(kWindowId, {kTabId}, &header);
  for (const sync_pb::SessionSpecifics& specifics : {tab, header}) {
    sync_pb::EntitySpecifics entity;
    *entity.mutable_session() = specifics;
    GetFakeServer()->InjectEntity(
        syncer::PersistentUniqueClientEntity::CreateFromSpecificsForTesting(
            "somename",
            sync_sessions::SessionStore::GetClientTag(entity.session()), entity,
            /*creation_time=*/syncer::TimeToProtoTime(kLastModifiedTime),
            /*last_modified_time=*/syncer::TimeToProtoTime(kLastModifiedTime)));
  }

  ASSERT_TRUE(SetupClients()) << "SetupClients() failed.";

  // Override large icon service to talk to the mock server.
  favicon::LargeIconServiceImpl* large_icon_service =
      static_cast<favicon::LargeIconServiceImpl*>(
          LargeIconServiceFactory::GetForBrowserContext(GetProfile(0)));
  large_icon_service->SetServerUrlForTesting(
      embedded_test_server()->GetURL("/"));

  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  // Expect injected foreign sessions to be synced down.
  SyncedSessionVector sessions;
  ASSERT_TRUE(GetSessionData(0, &sessions));
  ASSERT_EQ(1U, sessions.size());

  // Force creation of RecentTabsSubMenuModel which as a result fetches the
  // favicon for the injected fake recent tab.
  chrome::ShowAppMenu(GetBrowser(0));

  EXPECT_TRUE(FaviconForPageUrlAvailableChecker(GetProfile(0),
                                                GURL("http://foo/1"),
                                                /*should_be_available=*/true)
                  .Wait());

  // Disable tabs and history toggles.
  ASSERT_TRUE(
      GetClient(0)->DisableSyncForType(syncer::UserSelectableType::kTabs));
  ASSERT_TRUE(
      GetClient(0)->DisableSyncForType(syncer::UserSelectableType::kHistory));

  EXPECT_TRUE(FaviconForPageUrlAvailableChecker(GetProfile(0),
                                                GURL("http://foo/1"),
                                                /*should_be_available=*/false)
                  .Wait());
}

class SingleClientSessionsWithoutDestroyProfileSyncTest
    : public SingleClientSessionsSyncTest {
 public:
  SingleClientSessionsWithoutDestroyProfileSyncTest() {
    features_.InitAndDisableFeature(features::kDestroyProfileOnBrowserClose);
  }

 private:
  base::test::ScopedFeatureList features_;
};

IN_PROC_BROWSER_TEST_F(SingleClientSessionsWithoutDestroyProfileSyncTest,
                       ShouldDeleteLastClosedTab) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(CheckInitialState(0));

  GURL url1 =
      embedded_test_server()->GetURL("www.host1.com", "/sync/simple.html");
  GURL url2 =
      embedded_test_server()->GetURL("www.host2.com", "/sync/simple.html");

  ASSERT_TRUE(OpenTab(0, url1));
  ASSERT_TRUE(OpenTab(0, url2));
  WaitForHierarchyOnServer(SessionsHierarchy({{url1.spec(), url2.spec()}}));

  CloseTab(/*browser_index=*/0, /*tab_index=*/0);
  WaitForHierarchyOnServer(SessionsHierarchy({{url2.spec()}}));
  CloseTab(/*browser_index=*/0, /*tab_index=*/0);
  WaitForHierarchyOnServer(SessionsHierarchy());
}

#if !BUILDFLAG(IS_CHROMEOS)
class SingleClientSessionsWithDestroyProfileSyncTest
    : public SingleClientSessionsSyncTest {
 public:
  SingleClientSessionsWithDestroyProfileSyncTest() {
    features_.InitAndEnableFeature(features::kDestroyProfileOnBrowserClose);
  }

 private:
  base::test::ScopedFeatureList features_;
};

IN_PROC_BROWSER_TEST_F(SingleClientSessionsWithDestroyProfileSyncTest,
                       ShouldNotDeleteLastClosedTab) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(CheckInitialState(0));

  GURL url1 =
      embedded_test_server()->GetURL("www.host1.com", "/sync/simple.html");
  GURL url2 =
      embedded_test_server()->GetURL("www.host2.com", "/sync/simple.html");

  ASSERT_TRUE(OpenTab(0, url1));
  ASSERT_TRUE(OpenTab(0, url2));
  WaitForHierarchyOnServer(SessionsHierarchy({{url1.spec(), url2.spec()}}));

  CloseTab(/*browser_index=*/0, /*tab_index=*/0);
  WaitForHierarchyOnServer(SessionsHierarchy({{url2.spec()}}));

  {
    // Closing the last tab results in profile destruction and hence may require
    // running blocking tasks which are normally disallowed during tests.
    // TODO(crbug.com/40846214): remove once it's clear why it results in
    // blocking tasks.
    base::ScopedAllowUnresponsiveTasksForTesting scoped_allow_sync_primitives;
    CloseTab(/*browser_index=*/0, /*tab_index=*/0);
    // TODO(crbug.com/40113507): When DestroyProfileOnBrowserClose is enabled,
    // the last CloseTab() triggers Profile deletion (and SyncService deletion).
    // This means the last tab close never gets synced. We should fix this
    // regression eventually. Once that's done, merge this test with the
    // WithoutDestroyProfile version.
    base::RunLoop run_loop;
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE, run_loop.QuitClosure(), TestTimeouts::action_timeout());
    run_loop.Run();
  }

  // Even after several seconds, state didn't change on the server.
  fake_server::FakeServerVerifier verifier(GetFakeServer());
  EXPECT_TRUE(verifier.VerifySessions(SessionsHierarchy({{url2.spec()}})));
}
#endif  // !BUILDFLAG(IS_CHROMEOS)

}  // namespace
