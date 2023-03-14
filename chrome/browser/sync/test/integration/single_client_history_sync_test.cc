// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/path_service.h"
#include "base/test/scoped_feature_list.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/sync/test/integration/fake_server_match_status_checker.h"
#include "chrome/browser/sync/test/integration/sync_service_impl_harness.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "chrome/browser/sync/test/integration/typed_urls_helper.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "components/history/core/browser/history_types.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "components/sync/base/features.h"
#include "components/sync/base/model_type.h"
#include "components/sync/driver/sync_service_impl.h"
#include "components/sync/protocol/history_specifics.pb.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/base/filename_util.h"
#include "net/dns/mock_host_resolver.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/page_transition_types.h"
#include "url/gurl.h"

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#endif

namespace sync_pb {

// Makes the GMock matchers print out a readable version of the protobuf.
void PrintTo(const HistorySpecifics& history, std::ostream* os) {
  *os << "[ Visit time: " << history.visit_time_windows_epoch_micros()
      << ", Originator: " << history.originator_cache_guid()
      << ", Redirects: ( ";
  for (int i = 0; i < history.redirect_entries_size(); i++) {
    *os << history.redirect_entries(i).url() << " ";
  }
  *os << "), Transition: " << history.page_transition().core_transition()
      << ", Referring visit: " << history.originator_referring_visit_id()
      << ", Duration: " << history.visit_duration_micros() << " ]";
}

}  // namespace sync_pb

namespace history {

// Makes the GMock matchers print out a readable version of a VisitRow.
void PrintTo(const VisitRow& row, std::ostream* os) {
  *os << "[ VisitID: " << row.visit_id << ", Duration: " << row.visit_duration
      << " ]";
}

}  // namespace history

namespace {

using testing::AllOf;
using testing::Not;
using testing::UnorderedElementsAre;

const char kRedirectFromPath[] = "/redirect.html";
const char kRedirectToPath[] = "/sync/simple.html";

// Matchers for sync_pb::HistorySpecifics.

MATCHER_P(UrlIs, url, "") {
  if (arg.redirect_entries_size() != 1) {
    return false;
  }
  return arg.redirect_entries(0).url() == url;
}

MATCHER_P2(UrlsAre, url1, url2, "") {
  if (arg.redirect_entries_size() != 2) {
    return false;
  }
  return arg.redirect_entries(0).url() == url1 &&
         arg.redirect_entries(1).url() == url2;
}

MATCHER_P(CoreTransitionIs, transition, "") {
  return arg.page_transition().core_transition() == transition;
}

MATCHER(IsChainStart, "") {
  return !arg.redirect_chain_start_incomplete();
}

MATCHER(IsChainEnd, "") {
  return !arg.redirect_chain_end_incomplete();
}

MATCHER(HasReferringVisit, "") {
  return arg.originator_referring_visit_id() != 0;
}

MATCHER(HasOpenerVisit, "") {
  return arg.originator_opener_visit_id() != 0;
}

MATCHER(HasReferrerURL, "") {
  return !arg.referrer_url().empty();
}

MATCHER_P(ReferrerURLIs, referrer_url, "") {
  return arg.referrer_url() == referrer_url;
}

MATCHER(HasVisitDuration, "") {
  return arg.visit_duration_micros() > 0;
}

MATCHER(HasHttpResponseCode, "") {
  return arg.http_response_code() > 0;
}

// Matchers for history::VisitRow.

MATCHER_P(VisitRowIdIs, visit_id, "") {
  return arg.visit_id == visit_id;
}

MATCHER_P(VisitRowDurationIs, duration, "") {
  return arg.visit_duration == duration;
}

MATCHER(StandardFieldsArePopulated, "") {
  // Checks all fields that should never be empty/unset/default. Some fields can
  // be legitimately empty, or are set after an entity is first created.
  // May be legitimately empty:
  //   redirect_entries.title (may simply be empty)
  //   redirect_entries.redirect_type (empty if it's not a redirect)
  //   originator_referring_visit_id, originator_opener_visit_id (may not exist)
  //   root_task_id, parent_task_id (not always set)
  //   http_response_code (unset for replaced navigations)
  // Populated later:
  //   visit_duration_micros, page_language, password_state
  return arg.visit_time_windows_epoch_micros() > 0 &&
         !arg.originator_cache_guid().empty() &&
         arg.redirect_entries_size() > 0 &&
         arg.redirect_entries(0).originator_visit_id() > 0 &&
         !arg.redirect_entries(0).url().empty() && arg.has_browser_type() &&
         arg.window_id() > 0 && arg.tab_id() > 0 && arg.task_id() > 0;
}

GURL GetFileUrl(const char* file) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  return net::FilePathToFileURL(
      base::PathService::CheckedGet(chrome::DIR_TEST_DATA).AppendASCII(file));
}

sync_pb::HistorySpecifics CreateSpecifics(
    base::Time visit_time,
    const std::string& originator_cache_guid,
    const std::vector<GURL>& urls,
    const std::vector<history::VisitID>& originator_visit_ids) {
  DCHECK_EQ(originator_visit_ids.size(), urls.size());
  sync_pb::HistorySpecifics specifics;
  specifics.set_visit_time_windows_epoch_micros(
      visit_time.ToDeltaSinceWindowsEpoch().InMicroseconds());
  specifics.set_originator_cache_guid(originator_cache_guid);
  specifics.mutable_page_transition()->set_core_transition(
      sync_pb::SyncEnums_PageTransition_LINK);
  for (size_t i = 0; i < urls.size(); ++i) {
    auto* redirect_entry = specifics.add_redirect_entries();
    redirect_entry->set_originator_visit_id(originator_visit_ids[i]);
    redirect_entry->set_url(urls[i].spec());
    if (i > 0) {
      redirect_entry->set_redirect_type(
          sync_pb::SyncEnums_PageTransitionRedirectType_SERVER_REDIRECT);
    }
  }
  return specifics;
}

sync_pb::HistorySpecifics CreateSpecifics(
    base::Time visit_time,
    const std::string& originator_cache_guid,
    const GURL& url,
    history::VisitID originator_visit_id = 0) {
  return CreateSpecifics(visit_time, originator_cache_guid, std::vector{url},
                         std::vector{originator_visit_id});
}

std::unique_ptr<syncer::LoopbackServerEntity> CreateFakeServerEntity(
    const sync_pb::HistorySpecifics& specifics) {
  sync_pb::EntitySpecifics entity;
  *entity.mutable_history() = specifics;
  return syncer::PersistentUniqueClientEntity::CreateFromSpecificsForTesting(
      /*non_unique_name=*/"",
      /*client_tag=*/
      base::NumberToString(specifics.visit_time_windows_epoch_micros()), entity,
      /*creation_time=*/0,
      /*last_modified_time=*/0);
}

std::vector<sync_pb::HistorySpecifics> SyncEntitiesToHistorySpecifics(
    std::vector<sync_pb::SyncEntity> entities) {
  std::vector<sync_pb::HistorySpecifics> history;
  for (sync_pb::SyncEntity& entity : entities) {
    DCHECK(entity.specifics().has_history());
    history.push_back(std::move(entity.specifics().history()));
  }
  return history;
}

// A helper class that waits for entries in the local history DB that match the
// given matchers.
// Note that this only checks URLs that were passed in - any additional URLs in
// the DB (and their corresponding visits) are ignored.
class LocalHistoryMatchChecker : public SingleClientStatusChangeChecker {
 public:
  using Matcher = testing::Matcher<std::vector<history::VisitRow>>;

  explicit LocalHistoryMatchChecker(syncer::SyncServiceImpl* service,
                                    const std::map<GURL, Matcher>& matchers);
  ~LocalHistoryMatchChecker() override;

  // StatusChangeChecker implementation.
  bool IsExitConditionSatisfied(std::ostream* os) override;

  // syncer::SyncServiceObserver implementation.
  void OnSyncCycleCompleted(syncer::SyncService* sync) override;

 private:
  const std::map<GURL, Matcher> matchers_;
};

LocalHistoryMatchChecker::LocalHistoryMatchChecker(
    syncer::SyncServiceImpl* service,
    const std::map<GURL, Matcher>& matchers)
    : SingleClientStatusChangeChecker(service), matchers_(matchers) {}

LocalHistoryMatchChecker::~LocalHistoryMatchChecker() = default;

bool LocalHistoryMatchChecker::IsExitConditionSatisfied(std::ostream* os) {
  for (const auto& [url, matcher] : matchers_) {
    history::VisitVector visits =
        typed_urls_helper::GetVisitsForURLFromClient(/*index=*/0, url);
    testing::StringMatchResultListener result_listener;
    const bool matches =
        testing::ExplainMatchResult(matcher, visits, &result_listener);
    *os << result_listener.str();
    if (!matches) {
      return false;
    }
  }
  return true;
}

void LocalHistoryMatchChecker::OnSyncCycleCompleted(syncer::SyncService* sync) {
  CheckExitCondition();
}

// A helper class that waits for the HISTORY entities on the FakeServer to match
// a given GMock matcher.
class ServerHistoryMatchChecker
    : public fake_server::FakeServerMatchStatusChecker {
 public:
  using Matcher = testing::Matcher<std::vector<sync_pb::HistorySpecifics>>;

  explicit ServerHistoryMatchChecker(const Matcher& matcher);
  ~ServerHistoryMatchChecker() override;
  ServerHistoryMatchChecker(const ServerHistoryMatchChecker&) = delete;
  ServerHistoryMatchChecker& operator=(const ServerHistoryMatchChecker&) =
      delete;

  // FakeServer::Observer overrides.
  void OnCommit(const std::string& committer_invalidator_client_id,
                syncer::ModelTypeSet committed_model_types) override;

  // StatusChangeChecker overrides.
  bool IsExitConditionSatisfied(std::ostream* os) override;

 private:
  const Matcher matcher_;
};

ServerHistoryMatchChecker::ServerHistoryMatchChecker(const Matcher& matcher)
    : matcher_(matcher) {}

ServerHistoryMatchChecker::~ServerHistoryMatchChecker() = default;

void ServerHistoryMatchChecker::OnCommit(
    const std::string& committer_invalidator_client_id,
    syncer::ModelTypeSet committed_model_types) {
  if (committed_model_types.Has(syncer::HISTORY)) {
    CheckExitCondition();
  }
}

bool ServerHistoryMatchChecker::IsExitConditionSatisfied(std::ostream* os) {
  std::vector<sync_pb::HistorySpecifics> entities =
      SyncEntitiesToHistorySpecifics(
          fake_server()->GetSyncEntitiesByModelType(syncer::HISTORY));

  testing::StringMatchResultListener result_listener;
  const bool matches =
      testing::ExplainMatchResult(matcher_, entities, &result_listener);
  *os << result_listener.str();
  return matches;
}

class SingleClientHistorySyncTest : public SyncTest {
 public:
  SingleClientHistorySyncTest() : SyncTest(SINGLE_CLIENT) {
    features_.InitWithFeatures(
        {syncer::kSyncEnableHistoryDataType},
        // TODO(crbug.com/1394910): Use HTTPS URLs in tests to avoid having to
        // disable this feature.
        /*disabled_features=*/{features::kHttpsUpgrades});
  }
  ~SingleClientHistorySyncTest() override = default;

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");

    // Set up a server redirect from `kRedirectFromPath` to `kRedirectToPath`.
    embedded_test_server()->RegisterDefaultHandler(base::BindRepeating(
        [](const net::test_server::HttpRequest& request)
            -> std::unique_ptr<net::test_server::HttpResponse> {
          if (request.relative_url != kRedirectFromPath) {
            return nullptr;
          }
          auto response =
              std::make_unique<net::test_server::BasicHttpResponse>();
          response->set_code(net::HTTP_TEMPORARY_REDIRECT);
          response->AddCustomHeader("Location", kRedirectToPath);
          return response;
        }));

    ASSERT_TRUE(embedded_test_server()->Start());

    SyncTest::SetUpOnMainThread();
  }

  bool SetupClients() override {
    if (!SyncTest::SetupClients()) {
      return false;
    }

#if !BUILDFLAG(IS_ANDROID)
    // On non-Android platforms, SyncTest doesn't create any tabs in the
    // profiles/browsers it creates. Create an "empty" tab here, so that
    // NavigateToURL() will have a non-null WebContents to navigate in.
    for (int i = 0; i < num_clients(); ++i) {
      if (!AddTabAtIndexToBrowser(GetBrowser(0), 0, GURL("about:blank"),
                                  ui::PAGE_TRANSITION_AUTO_TOPLEVEL)) {
        return false;
      }
    }
#endif

    return true;
  }

  void NavigateToURL(const GURL& url,
                     ui::PageTransition transition = ui::PAGE_TRANSITION_TYPED,
                     const GURL& referrer = GURL()) {
    content::NavigationController::LoadURLParams params(url);
    params.transition_type = transition;
    if (referrer.is_valid()) {
      params.referrer =
          content::Referrer(referrer, network::mojom::ReferrerPolicy::kAlways);
    }
    content::NavigateToURLBlockUntilNavigationsComplete(GetActiveWebContents(),
                                                        params, 1);

    // Ensure the navigation succeeded (i.e. whatever test URL was passed in was
    // actually valid).
    ASSERT_EQ(200, GetActiveWebContents()
                       ->GetController()
                       .GetLastCommittedEntry()
                       ->GetHttpStatusCode());
  }

  bool WaitForServerHistory(
      testing::Matcher<std::vector<sync_pb::HistorySpecifics>> matcher) {
    return ServerHistoryMatchChecker(matcher).Wait();
  }

  bool WaitForLocalHistory(
      const std::map<GURL, testing::Matcher<std::vector<history::VisitRow>>>&
          matchers) {
    return LocalHistoryMatchChecker(GetSyncService(0), matchers).Wait();
  }

  content::WebContents* GetActiveWebContents() {
#if BUILDFLAG(IS_ANDROID)
    return chrome_test_utils::GetActiveWebContents(this);
#else
    // Note: chrome_test_utils::GetActiveWebContents() doesn't work on
    // non-Android platforms, since it uses the profile created by
    // InProcessBrowserTest, not the profile(s) from SyncTest.
    return GetBrowser(0)->tab_strip_model()->GetActiveWebContents();
#endif
  }

 private:
  base::test::ScopedFeatureList features_;
};

IN_PROC_BROWSER_TEST_F(SingleClientHistorySyncTest,
                       DoesNotUploadRetroactively) {
  ASSERT_TRUE(SetupClients()) << "SetupClients() failed.";

  // Navigate somewhere before Sync is turned on.
  GURL not_synced_url =
      embedded_test_server()->GetURL("not-synced.com", "/sync/simple.html");
  NavigateToURL(not_synced_url);

  // Navigate on. The previous URL should *not* get synced, but this one
  // (currently open at the time Sync is turned on) will get synced when it
  // gets updated, which in practice happens on the next navigation, or when the
  // tab is closed.
  GURL synced_url1 =
      embedded_test_server()->GetURL("synced1.com", "/sync/simple.html");
  NavigateToURL(synced_url1);

  // Note: On Android, SetupSync(WAIT_FOR_COMMITS_TO_COMPLETE) (the default)
  // waits for an "about:blank" tab to show up in the Sessions data on the fake
  // server. Since this test already navigated away, that'll never happen. So
  // use the slightly-weaker WAIT_FOR_SYNC_SETUP_TO_COMPLETE here.
  ASSERT_TRUE(SetupSync(SyncTest::WAIT_FOR_SYNC_SETUP_TO_COMPLETE))
      << "SetupSync() failed.";

  // After Sync was enabled, navigate further.
  GURL synced_url2 =
      embedded_test_server()->GetURL("synced2.com", "/sync/simple.html");
  NavigateToURL(synced_url2);

  // The last two URLs (currently open while Sync was turned on, and
  // navigated-to after Sync was turned on, respectively) should have been
  // synced. The first URL (closed before Sync was turned on) should not have
  // been synced.
  EXPECT_TRUE(WaitForServerHistory(UnorderedElementsAre(
      UrlIs(synced_url1.spec()), UrlIs(synced_url2.spec()))));
}

IN_PROC_BROWSER_TEST_F(SingleClientHistorySyncTest,
                       DoesNotUploadUnsyncableURLs) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  // Navigate to some unsyncable URLs. Note that some of these are excluded by
  // the history system itself (see CanAddURLToHistory()) and thus don't even
  // make it to the history DB, while others are filtered by HistorySyncBridge.
  NavigateToURL(GURL(chrome::kChromeUIVersionURL));
  NavigateToURL(GetFileUrl("sync/simple.html"));
  NavigateToURL(GURL("data:text/plain;base64,SGVsbG8sIFdvcmxkIQ=="));

  // Finally, navigate to a regular, syncable URL, so that there's something to
  // wait for.
  GURL synced_url =
      embedded_test_server()->GetURL("synced.com", "/sync/simple.html");
  NavigateToURL(synced_url);

  // Only the regular, syncable URL should have arrived at the server.
  WaitForServerHistory(UnorderedElementsAre(UrlIs(synced_url)));
}

// TODO(crbug.com/1373448): EnterSyncPausedStateForPrimaryAccount is currently
// not supported on Android. Enable this test once it is.
#if !BUILDFLAG(IS_ANDROID)
IN_PROC_BROWSER_TEST_F(SingleClientHistorySyncTest, DoesNotUploadWhilePaused) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  // Navigate somewhere and make sure the URL arrives on the server.
  GURL synced_url1 =
      embedded_test_server()->GetURL("synced1.com", "/sync/simple.html");
  NavigateToURL(synced_url1);
  ASSERT_TRUE(
      WaitForServerHistory(UnorderedElementsAre(UrlIs(synced_url1.spec()))));

  // Enter the Sync-paused state.
  GetClient(0)->EnterSyncPausedStateForPrimaryAccount();
  ASSERT_EQ(GetSyncService(0)->GetTransportState(),
            syncer::SyncService::TransportState::PAUSED);

  // Navigate somewhere while Sync is paused.
  GURL paused_url =
      embedded_test_server()->GetURL("not-synced.com", "/sync/simple.html");
  NavigateToURL(paused_url);

  // Navigate somewhere else. The previous URL should *not* get synced, but this
  // one (currently open at the time Sync is un-paused) will get synced when it
  // gets updated, which in practice happens on the next navigation, or when the
  // tab is closed.
  GURL synced_url2 =
      embedded_test_server()->GetURL("synced2.com", "/sync/simple.html");
  NavigateToURL(synced_url2);

  GetClient(0)->ExitSyncPausedStateForPrimaryAccount();
  ASSERT_EQ(GetSyncService(0)->GetTransportState(),
            syncer::SyncService::TransportState::ACTIVE);

  // After Sync was un-paused, navigate further. This triggers an update to
  // `synced_url2` and also uploads `synced_url3`.
  GURL synced_url3 =
      embedded_test_server()->GetURL("synced3.com", "/sync/simple.html");
  NavigateToURL(synced_url3);

  EXPECT_TRUE(WaitForServerHistory(
      UnorderedElementsAre(UrlIs(synced_url1.spec()), UrlIs(synced_url2.spec()),
                           UrlIs(synced_url3.spec()))));
}
#endif  // !BUILDFLAG(IS_ANDROID)

IN_PROC_BROWSER_TEST_F(SingleClientHistorySyncTest, UploadsAllFields) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  // Navigate to some URL, and make sure it shows up on the server.
  GURL url1 =
      embedded_test_server()->GetURL("www.host1.com", "/sync/simple.html");
  NavigateToURL(url1, ui::PAGE_TRANSITION_AUTO_BOOKMARK);

  EXPECT_TRUE(WaitForServerHistory(UnorderedElementsAre(
      AllOf(StandardFieldsArePopulated(), UrlIs(url1.spec())))));

  // Navigate to a second URL. This "completes" the first visit, which should
  // cause it to get updated with some details that are known only now, e.g.
  // the visit duration.
  // Note that currently, HistoryBackend depends on the presence of a referrer
  // to correctly populate the visit_duration (see crbug.com/1357013).
  GURL url2 =
      embedded_test_server()->GetURL("www.host2.com", "/sync/simple.html");
  NavigateToURL(url2, ui::PAGE_TRANSITION_LINK, /*referrer=*/url1);

  EXPECT_TRUE(WaitForServerHistory(UnorderedElementsAre(
      AllOf(StandardFieldsArePopulated(), UrlIs(url1.spec()),
            CoreTransitionIs(sync_pb::SyncEnums_PageTransition_AUTO_BOOKMARK),
            HasHttpResponseCode(), Not(HasReferringVisit()),
            Not(HasReferrerURL()), HasVisitDuration()),
      AllOf(StandardFieldsArePopulated(), UrlIs(url2.spec()),
            CoreTransitionIs(sync_pb::SyncEnums_PageTransition_LINK),
            HasHttpResponseCode(), HasReferringVisit(),
            ReferrerURLIs(url1.spec())))));
}

IN_PROC_BROWSER_TEST_F(SingleClientHistorySyncTest,
                       UploadsMarkVisitAsKnownToSync) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  // Navigate to some URL, and make sure it shows up on the server.
  GURL url1 =
      embedded_test_server()->GetURL("www.host1.com", "/sync/simple.html");
  NavigateToURL(url1, ui::PAGE_TRANSITION_AUTO_BOOKMARK);
  ASSERT_TRUE(WaitForServerHistory(UnorderedElementsAre(
      AllOf(StandardFieldsArePopulated(), UrlIs(url1.spec())))));

  // Now also verify that the local visit is marked as known to sync.
  history::VisitVector visits =
      typed_urls_helper::GetVisitsForURLFromClient(/*index=*/0, url1);
  ASSERT_EQ(visits.size(), 1U);
  EXPECT_TRUE(visits[0].is_known_to_sync);
}

IN_PROC_BROWSER_TEST_F(SingleClientHistorySyncTest, UploadsServerRedirect) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  // Navigate to a URL which will redirect to another URL via a server redirect
  // i.e. an HTTP 3xx response (see SetUpOnMainThread()).
  const GURL url_from =
      embedded_test_server()->GetURL("www.host.com", kRedirectFromPath);
  NavigateToURL(url_from, ui::PAGE_TRANSITION_AUTO_BOOKMARK);

  const GURL url_to =
      embedded_test_server()->GetURL("www.host.com", kRedirectToPath);

  // The redirect chain should have been uploaded as a single entity (since
  // server redirects within a chain all have the same visit_time).
  EXPECT_TRUE(WaitForServerHistory(UnorderedElementsAre(AllOf(
      StandardFieldsArePopulated(), UrlsAre(url_from.spec(), url_to.spec()),
      IsChainStart(), IsChainEnd(), Not(HasReferringVisit())))));
}

IN_PROC_BROWSER_TEST_F(SingleClientHistorySyncTest, UploadsClientMetaRedirect) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  // Navigate to a URL which will redirect to another URL via an html <meta>
  // tag.
  const GURL url_from = embedded_test_server()->GetURL(
      "www.host.com", "/sync/meta_redirect.html");
  NavigateToURL(url_from, ui::PAGE_TRANSITION_AUTO_BOOKMARK);

  const GURL url_to =
      embedded_test_server()->GetURL("www.host.com", kRedirectToPath);

  // The redirect chain should have been uploaded as two separate entities,
  // since client redirects result in different visit_times. However, the
  // chain_start and chain_end markers should indicate that these two entities
  // belong to the same chain.
  EXPECT_TRUE(WaitForServerHistory(UnorderedElementsAre(
      AllOf(StandardFieldsArePopulated(), UrlIs(url_from.spec()),
            IsChainStart(), Not(IsChainEnd()), Not(HasReferringVisit())),
      AllOf(StandardFieldsArePopulated(), UrlIs(url_to.spec()),
            Not(IsChainStart()), IsChainEnd(), HasReferringVisit()))));
}

IN_PROC_BROWSER_TEST_F(SingleClientHistorySyncTest, UploadsClientJSRedirect) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  // Navigate to a page.
  const GURL url1 =
      embedded_test_server()->GetURL("www.host1.com", "/sync/simple.html");
  NavigateToURL(url1, ui::PAGE_TRANSITION_AUTO_BOOKMARK);

  // The page sets window.location in JavaScript to redirect to a different URL.
  const GURL url2 =
      embedded_test_server()->GetURL("www.host2.com", "/sync/simple.html");
  ASSERT_TRUE(content::ExecJs(
      GetActiveWebContents(),
      base::StringPrintf("window.location = '%s';", url2.spec().c_str())));

  // This kind of "redirect" is not actually considered a redirect by the
  // history backend, so two separate sync entities should have been uploaded,
  // each its own complete redirect chain.
  EXPECT_TRUE(WaitForServerHistory(UnorderedElementsAre(
      AllOf(StandardFieldsArePopulated(), UrlIs(url1.spec()), IsChainStart(),
            IsChainEnd()),
      AllOf(StandardFieldsArePopulated(), UrlIs(url2.spec()), IsChainStart(),
            IsChainEnd()))));
}

IN_PROC_BROWSER_TEST_F(SingleClientHistorySyncTest,
                       UploadsReplaceStateNavigation) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  // Navigate to some page.
  const GURL url1 =
      embedded_test_server()->GetURL("www.host1.com", "/sync/simple.html");
  NavigateToURL(url1, ui::PAGE_TRANSITION_AUTO_BOOKMARK);

  // The page uses the JS history.replaceState API to update the URL.
  const GURL url2 =
      embedded_test_server()->GetURL("www.host1.com", "/replaced_history.html");
  ASSERT_TRUE(content::ExecJs(
      GetActiveWebContents(),
      base::StringPrintf("history.replaceState({}, 'page 2', '%s')",
                         url2.spec().c_str())));

  // This results in two visits with different visit_times, which thus gets
  // mapped to two separate sync entities. There's no redirection link between
  // the two, but since it was a same-document navigation, the first visit
  // should be the opener of the second.
  EXPECT_TRUE(WaitForServerHistory(UnorderedElementsAre(
      AllOf(StandardFieldsArePopulated(), UrlIs(url1.spec()), IsChainStart(),
            IsChainEnd()),
      AllOf(StandardFieldsArePopulated(), UrlIs(url2.spec()), IsChainStart(),
            IsChainEnd(), HasOpenerVisit()))));
}

IN_PROC_BROWSER_TEST_F(SingleClientHistorySyncTest, DownloadsAndMerges) {
  ASSERT_TRUE(SetupClients()) << "SetupClients() failed.";

  // Before Sync gets enabled, one URL exists locally, one remotely, and one in
  // both places.
  const GURL url_local("https://www.url-local.com");
  const GURL url_remote("https://www.url-remote.com");
  const GURL url_both("https://www.url-both.com");

  typed_urls_helper::AddUrlToHistory(/*index=*/0, url_local);
  typed_urls_helper::AddUrlToHistory(/*index=*/0, url_both);

  GetFakeServer()->InjectEntity(CreateFakeServerEntity(CreateSpecifics(
      base::Time::Now() - base::Minutes(5), "other_cache_guid", url_remote)));
  GetFakeServer()->InjectEntity(CreateFakeServerEntity(CreateSpecifics(
      base::Time::Now() - base::Minutes(4), "other_cache_guid", url_both)));

  // Turn on Sync - this should cause the two remote URLs to get downloaded and
  // merged with the existing local ones.
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  // Now the "local" and "remote" URLs should have one visit each, while the
  // "both" one should have two.
  history::URLRow row_local;
  EXPECT_TRUE(
      typed_urls_helper::GetUrlFromClient(/*index=*/0, url_local, &row_local));
  EXPECT_EQ(row_local.visit_count(), 1);

  history::URLRow row_remote;
  EXPECT_TRUE(typed_urls_helper::GetUrlFromClient(/*index=*/0, url_remote,
                                                  &row_remote));
  EXPECT_EQ(row_remote.visit_count(), 1);

  history::URLRow row_both;
  EXPECT_TRUE(
      typed_urls_helper::GetUrlFromClient(/*index=*/0, url_both, &row_both));
  EXPECT_EQ(row_both.visit_count(), 2);
}

IN_PROC_BROWSER_TEST_F(SingleClientHistorySyncTest,
                       DownloadsAndMarksRemoteVisitAsKnownToSync) {
  ASSERT_TRUE(SetupClients()) << "SetupClients() failed.";

  // This simple test only has a single remote visit.
  const GURL url_remote("https://www.url-remote.com");
  GetFakeServer()->InjectEntity(CreateFakeServerEntity(CreateSpecifics(
      base::Time::Now() - base::Minutes(5), "other_cache_guid", url_remote)));

  // Turn on Sync - this should download the single remote visit.
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  // The "remote" URLs should have one visit marked as known to Sync.
  history::URLRow row_remote;
  EXPECT_TRUE(typed_urls_helper::GetUrlFromClient(/*index=*/0, url_remote,
                                                  &row_remote));
  EXPECT_EQ(row_remote.visit_count(), 1);

  history::VisitVector visits =
      typed_urls_helper::GetVisitsFromClient(/*index=*/0, row_remote.id());
  ASSERT_EQ(visits.size(), 1U);
  EXPECT_TRUE(visits[0].is_known_to_sync);
}

IN_PROC_BROWSER_TEST_F(SingleClientHistorySyncTest,
                       DownloadsServerRedirectChain) {
  const GURL url1("https://www.url1.com");
  const GURL url2("https://www.url2.com");
  const GURL url3("https://www.url3.com");

  GetFakeServer()->InjectEntity(CreateFakeServerEntity(
      CreateSpecifics(base::Time::Now() - base::Minutes(5), "other_cache_guid",
                      {url1, url2, url3}, {101, 102, 103})));

  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  // Make sure the chain arrived intact.
  history::URLRow url_row;
  EXPECT_TRUE(typed_urls_helper::GetUrlFromClient(/*index=*/0, url3, &url_row));
  history::VisitVector visits =
      typed_urls_helper::GetVisitsFromClient(/*index=*/0, url_row.id());
  ASSERT_EQ(visits.size(), 1u);
  history::VisitVector redirect_chain =
      typed_urls_helper::GetRedirectChainFromClient(/*index=*/0, visits[0]);
  ASSERT_EQ(redirect_chain.size(), 3u);

  history::URLRow url_row1;
  EXPECT_TRUE(typed_urls_helper::GetUrlFromClient(
      /*index=*/0, redirect_chain[0].url_id, &url_row1));
  EXPECT_EQ(url_row1.url(), url1);
  history::URLRow url_row2;
  EXPECT_TRUE(typed_urls_helper::GetUrlFromClient(
      /*index=*/0, redirect_chain[1].url_id, &url_row2));
  EXPECT_EQ(url_row2.url(), url2);
  history::URLRow url_row3;
  EXPECT_TRUE(typed_urls_helper::GetUrlFromClient(
      /*index=*/0, redirect_chain[2].url_id, &url_row3));
  EXPECT_EQ(url_row3.url(), url3);
}

IN_PROC_BROWSER_TEST_F(SingleClientHistorySyncTest,
                       DownloadsClientRedirectChain) {
  const GURL url1("https://www.url1.com");
  const GURL url2("https://www.url2.com");

  // As opposed to server redirects, client redirect entries have different
  // visit_times, so the chain gets split up into multiple entities.
  sync_pb::HistorySpecifics specifics1 = CreateSpecifics(
      base::Time::Now() - base::Minutes(5), "other_cache_guid", url1, 101);
  sync_pb::HistorySpecifics specifics2 = CreateSpecifics(
      base::Time::Now() - base::Minutes(4), "other_cache_guid", url2, 102);
  // Link them together via the referring visit ID, and mark chain end/start
  // incomplete, so they'll be considered one chain.
  specifics1.set_redirect_chain_end_incomplete(true);
  specifics2.set_redirect_chain_start_incomplete(true);
  specifics2.set_originator_referring_visit_id(101);
  specifics2.mutable_redirect_entries(0)->set_redirect_type(
      sync_pb::SyncEnums_PageTransitionRedirectType_CLIENT_REDIRECT);

  GetFakeServer()->InjectEntity(CreateFakeServerEntity(specifics1));
  GetFakeServer()->InjectEntity(CreateFakeServerEntity(specifics2));

  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  // Make sure the chain arrived intact (i.e. was stitched back together).
  history::URLRow url_row;
  EXPECT_TRUE(typed_urls_helper::GetUrlFromClient(/*index=*/0, url2, &url_row));
  history::VisitVector visits =
      typed_urls_helper::GetVisitsFromClient(/*index=*/0, url_row.id());
  ASSERT_EQ(visits.size(), 1u);
  history::VisitVector redirect_chain =
      typed_urls_helper::GetRedirectChainFromClient(/*index=*/0, visits[0]);
  ASSERT_EQ(redirect_chain.size(), 2u);

  history::URLRow url_row1;
  EXPECT_TRUE(typed_urls_helper::GetUrlFromClient(
      /*index=*/0, redirect_chain[0].url_id, &url_row1));
  EXPECT_EQ(url_row1.url(), url1);
  history::URLRow url_row2;
  EXPECT_TRUE(typed_urls_helper::GetUrlFromClient(
      /*index=*/0, redirect_chain[1].url_id, &url_row2));
  EXPECT_EQ(url_row2.url(), url2);
}

IN_PROC_BROWSER_TEST_F(SingleClientHistorySyncTest,
                       DownloadsAndRemapsReferrer) {
  const GURL url1("https://www.url1.com");
  const GURL url2("https://www.url2.com");

  sync_pb::HistorySpecifics specifics1 = CreateSpecifics(
      base::Time::Now() - base::Minutes(5), "other_cache_guid", url1, 101);
  sync_pb::HistorySpecifics specifics2 = CreateSpecifics(
      base::Time::Now() - base::Minutes(4), "other_cache_guid", url2, 102);
  // The second visit has the first one as a referrer.
  specifics2.set_originator_referring_visit_id(101);

  GetFakeServer()->InjectEntity(CreateFakeServerEntity(specifics1));
  GetFakeServer()->InjectEntity(CreateFakeServerEntity(specifics2));

  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  // Make sure the visits arrived, and the referrer link got properly remapped.
  // Also grab their local visit IDs.
  history::VisitID visit_id1 = history::kInvalidVisitID;
  history::VisitID visit_id2 = history::kInvalidVisitID;
  {
    history::VisitVector visits1 =
        typed_urls_helper::GetVisitsForURLFromClient(/*index=*/0, url1);
    ASSERT_EQ(visits1.size(), 1u);
    visit_id1 = visits1[0].visit_id;

    history::VisitVector visits2 =
        typed_urls_helper::GetVisitsForURLFromClient(/*index=*/0, url2);
    ASSERT_EQ(visits2.size(), 1u);
    visit_id2 = visits2[0].visit_id;

    EXPECT_EQ(visits2[0].referring_visit, visits1[0].visit_id);
  }

  // Update the visits on the server.
  specifics1.set_visit_duration_micros(1234);
  specifics2.set_visit_duration_micros(5678);
  GetFakeServer()->InjectEntity(CreateFakeServerEntity(specifics1));
  GetFakeServer()->InjectEntity(CreateFakeServerEntity(specifics2));

#if BUILDFLAG(IS_ANDROID)
  // On Android, invalidations for HISTORY are disabled, so trigger an explicit
  // refresh to fetch the updated data.
  GetSyncService(0)->TriggerRefresh({syncer::HISTORY});
#endif

  // Wait for the updates to arrive.
  WaitForLocalHistory(
      {{url1, UnorderedElementsAre(
                  AllOf(VisitRowIdIs(visit_id1),
                        VisitRowDurationIs(base::Microseconds(1234))))},
       {url2, UnorderedElementsAre(
                  AllOf(VisitRowIdIs(visit_id2),
                        VisitRowDurationIs(base::Microseconds(5678))))}});

  // Make sure the updates arrived, and the referrer link was preserved.
  {
    history::VisitVector visits1 =
        typed_urls_helper::GetVisitsForURLFromClient(/*index=*/0, url1);
    ASSERT_EQ(visits1.size(), 1u);

    history::VisitVector visits2 =
        typed_urls_helper::GetVisitsForURLFromClient(/*index=*/0, url2);
    ASSERT_EQ(visits2.size(), 1u);

    // The local visit IDs shouldn't have changed.
    EXPECT_EQ(visits1[0].visit_id, visit_id1);
    EXPECT_EQ(visits2[0].visit_id, visit_id2);

    // The updated visit durations should've been applied.
    EXPECT_EQ(visits1[0].visit_duration, base::Microseconds(1234));
    EXPECT_EQ(visits2[0].visit_duration, base::Microseconds(5678));

    // And finally, the referrer link should still exist.
    EXPECT_EQ(visits2[0].referring_visit, visits1[0].visit_id);
  }
}

IN_PROC_BROWSER_TEST_F(SingleClientHistorySyncTest,
                       DoesNotDownloadUnwantedURLs) {
  // Several visits to "unwanted" URLs exist on the server (e.g. a bad other
  // client might have added them). These shouldn't be added to the history DB,
  // per CanAddURLToHistory().
  const GURL url1("chrome://settings");
  const GURL url2("about:blank");
  const GURL url3("javascript:alert(1);");

  sync_pb::HistorySpecifics specifics1 = CreateSpecifics(
      base::Time::Now() - base::Minutes(5), "other_cache_guid", url1, 101);
  sync_pb::HistorySpecifics specifics2 = CreateSpecifics(
      base::Time::Now() - base::Minutes(4), "other_cache_guid", url2, 102);
  sync_pb::HistorySpecifics specifics3 = CreateSpecifics(
      base::Time::Now() - base::Minutes(3), "other_cache_guid", url3, 103);

  GetFakeServer()->InjectEntity(CreateFakeServerEntity(specifics1));
  GetFakeServer()->InjectEntity(CreateFakeServerEntity(specifics2));
  GetFakeServer()->InjectEntity(CreateFakeServerEntity(specifics3));

  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  // None of these should have made it into the history DB.
  EXPECT_TRUE(
      typed_urls_helper::GetVisitsForURLFromClient(/*index=*/0, url1).empty());
  EXPECT_TRUE(
      typed_urls_helper::GetVisitsForURLFromClient(/*index=*/0, url2).empty());
  EXPECT_TRUE(
      typed_urls_helper::GetVisitsForURLFromClient(/*index=*/0, url3).empty());
}

// Signing out or turning off Sync isn't possible in ChromeOS-Ash.
#if !BUILDFLAG(IS_CHROMEOS_ASH)

IN_PROC_BROWSER_TEST_F(SingleClientHistorySyncTest,
                       ClearsForeignHistoryOnTurningSyncOff) {
  ASSERT_TRUE(SetupClients()) << "SetupClients() failed.";

  // Before Sync gets enabled, one URL exists locally, one remotely.
  const GURL url_local("https://www.url-local.com");
  const GURL url_remote("https://www.url-remote.com");

  typed_urls_helper::AddUrlToHistory(/*index=*/0, url_local);

  GetFakeServer()->InjectEntity(CreateFakeServerEntity(CreateSpecifics(
      base::Time::Now() - base::Minutes(5), "other_cache_guid", url_remote)));

  // Turn on Sync - this will cause the remote URL to get downloaded.
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  // Make sure the "local" and "remote" URLs both exist in the DB.
  history::URLRow row;
  ASSERT_TRUE(
      typed_urls_helper::GetUrlFromClient(/*index=*/0, url_local, &row));
  ASSERT_TRUE(
      typed_urls_helper::GetUrlFromClient(/*index=*/0, url_remote, &row));

  // Turn Sync off by removing the primary account.
  GetClient(0)->SignOutPrimaryAccount();
  ASSERT_EQ(GetSyncService(0)->GetTransportState(),
            syncer::SyncService::TransportState::DISABLED);

  // This should have triggered the deletion of foreign history (but left
  // local history alone).
  EXPECT_TRUE(
      typed_urls_helper::GetUrlFromClient(/*index=*/0, url_local, &row));
  EXPECT_FALSE(
      typed_urls_helper::GetUrlFromClient(/*index=*/0, url_remote, &row));
}

IN_PROC_BROWSER_TEST_F(SingleClientHistorySyncTest,
                       ClearsForeignHistoryOnTurningSyncOffInTwoSteps) {
  ASSERT_TRUE(SetupClients()) << "SetupClients() failed.";

  // Before Sync gets enabled, one URL exists locally, one remotely.
  const GURL url_local("https://www.url-local.com");
  const GURL url_remote("https://www.url-remote.com");

  typed_urls_helper::AddUrlToHistory(/*index=*/0, url_local);

  GetFakeServer()->InjectEntity(CreateFakeServerEntity(CreateSpecifics(
      base::Time::Now() - base::Minutes(5), "other_cache_guid", url_remote)));

  // Turn on Sync - this will cause the remote URL to get downloaded.
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  // Make sure the "local" and "remote" URLs both exist in the DB.
  history::URLRow row;
  ASSERT_TRUE(
      typed_urls_helper::GetUrlFromClient(/*index=*/0, url_local, &row));
  ASSERT_TRUE(
      typed_urls_helper::GetUrlFromClient(/*index=*/0, url_remote, &row));

  // Turn Sync off *in two steps* (similar to what actually happens in practice,
  // see crbug.com/1383912#c5):
  // 1) Remove the Sync-consent bit (but leave the primary account around).
  // 2) Actually remove the primary account.
  // After step 1, Sync will *not* be fully disabled, but rather try to start up
  // again in transport-only mode.
  signin::RevokeSyncConsent(
      IdentityManagerFactory::GetForProfile(GetProfile(0)));
  ASSERT_NE(GetSyncService(0)->GetTransportState(),
            syncer::SyncService::TransportState::DISABLED);

  GetClient(0)->SignOutPrimaryAccount();
  ASSERT_EQ(GetSyncService(0)->GetTransportState(),
            syncer::SyncService::TransportState::DISABLED);

  // This should have triggered the deletion of foreign history (but left
  // local history alone).
  EXPECT_TRUE(
      typed_urls_helper::GetUrlFromClient(/*index=*/0, url_local, &row));
  EXPECT_FALSE(
      typed_urls_helper::GetUrlFromClient(/*index=*/0, url_remote, &row));
}

#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)

// On Android, switches::kSyncUserForTest isn't supported (the passed-in
// username gets ignored in SyncSigninDelegateAndroid::SigninFake()), so it's
// not currently possible to simulate a non-@gmail.com account.
#if !BUILDFLAG(IS_ANDROID)

class SingleClientHistoryNonGmailSyncTest : public SingleClientHistorySyncTest {
 public:
  void SetUp() override {
    // Set up a non-@gmail.com account, so that it'll be treated as a potential
    // Dasher (aka managed aka enterprise) account.
    // Note: This can't be done in SetUpCommandLine() because that happens
    // slightly too late (SyncTest::SetUp() already consumes this param).
    base::CommandLine* cl = base::CommandLine::ForCurrentProcess();
    cl->AppendSwitchASCII(switches::kSyncUserForTest,
                          "user@managed-domain.com");
    SingleClientHistorySyncTest::SetUp();
  }

  void SignInAndSetAccountInfo(bool is_managed) {
    ASSERT_TRUE(GetClient(0)->SignInPrimaryAccount());

    signin::IdentityManager* identity_manager =
        IdentityManagerFactory::GetForProfile(GetProfile(0));
    CoreAccountInfo account =
        identity_manager->GetPrimaryAccountInfo(signin::ConsentLevel::kSync);

    // A non-empty hosted domain means the account is managed.
    std::string hosted_domain = is_managed ? "managed-domain.com" : "";
    signin::SimulateSuccessfulFetchOfAccountInfo(
        identity_manager, account.account_id, account.email, account.gaia,
        hosted_domain, "Full Name", "Given Name", "en-US", "");
  }
};

IN_PROC_BROWSER_TEST_F(SingleClientHistoryNonGmailSyncTest,
                       HistorySyncDisabledForManagedAccount) {
  ASSERT_TRUE(SetupClients()) << "SetupClients() failed.";
  SignInAndSetAccountInfo(/*is_managed=*/true);
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  ASSERT_FALSE(GetSyncService(0)->GetActiveDataTypes().Empty());
  EXPECT_FALSE(GetSyncService(0)->GetActiveDataTypes().Has(syncer::HISTORY));
}

IN_PROC_BROWSER_TEST_F(SingleClientHistoryNonGmailSyncTest,
                       HistorySyncEnabledForNonManagedAccount) {
  ASSERT_TRUE(SetupClients()) << "SetupClients() failed.";
  SignInAndSetAccountInfo(/*is_managed=*/false);
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  EXPECT_TRUE(GetSyncService(0)->GetActiveDataTypes().Has(syncer::HISTORY));
}

#endif  // !BUILDFLAG(IS_ANDROID)

}  // namespace
