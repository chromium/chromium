// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/path_service.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/sync/test/integration/history_helper.h"
#include "chrome/browser/sync/test/integration/sync_service_impl_harness.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/browser/history_service_observer.h"
#include "components/history/core/browser/history_types.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "components/sync/base/data_type.h"
#include "components/sync/base/features.h"
#include "components/sync/protocol/history_specifics.pb.h"
#include "components/sync/service/sync_service_impl.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/base/filename_util.h"
#include "net/dns/mock_host_resolver.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/page_transition_types.h"
#include "url/gurl.h"

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/metrics/desktop_session_duration/desktop_session_duration_tracker.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#endif

using history_helper::CoreTransitionIs;
using history_helper::HasHttpResponseCode;
using history_helper::HasOpenerVisit;
using history_helper::HasReferrerURL;
using history_helper::HasReferringVisit;
using history_helper::HasVisitDuration;
using history_helper::IsChainEnd;
using history_helper::IsChainStart;
using history_helper::ReferrerURLIs;
using history_helper::StandardFieldsArePopulated;
using history_helper::UrlIs;
using history_helper::UrlsAre;
using history_helper::VisitRowDurationIs;
using history_helper::VisitRowIdIs;
using ::testing::_;
using testing::AllOf;
using testing::Not;
using testing::UnorderedElementsAre;

namespace {

constexpr char kRedirectFromPath[] = "/redirect.html";
constexpr char kRedirectToPath[] = "/sync/simple.html";

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_CHROMEOS)
constexpr char kMetricNameWithHistorySync[] =
    "Session.TotalDurationMax1Day.WithHistorySync";
constexpr char kMetricNameWithHistorySyncWithoutAuthError[] =
    "Session.TotalDurationMax1Day.WithHistorySyncWithoutAuthError";
#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_CHROMEOS)

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

// Used to test if the History Service Observer gets called for both
// `OnURLVisited()` and `OnURLVisitedWithNavigationId()`.
class MockHistoryServiceObserver : public history::HistoryServiceObserver {
 public:
  MockHistoryServiceObserver() = default;

  MOCK_METHOD(void,
              OnURLVisited,
              (history::HistoryService*, const history::VisitedURLInfo&),
              (override));

  MOCK_METHOD(void,
              OnURLVisitedWithNavigationId,
              (history::HistoryService*, const history::VisitedURLInfo&),
              (override));
};

class SingleClientHistorySyncTest
    : public SyncTest,
      public testing::WithParamInterface<SyncTest::SetupSyncMode> {
 public:
  SingleClientHistorySyncTest() : SyncTest(SINGLE_CLIENT) {
    std::vector<base::test::FeatureRef> enabled_features;
    if (GetSetupSyncMode() == SetupSyncMode::kSyncTransportOnly) {
      enabled_features.push_back(syncer::kReplaceSyncPromosWithSignInPromos);
    }
    // TODO(crbug.com/40248833): Use HTTPS URLs in tests to avoid having to
    // disable this feature.
    features_.InitWithFeatures(
        enabled_features, /*disabled_features=*/{features::kHttpsUpgrades});
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

  SyncTest::SetupSyncMode GetSetupSyncMode() const override {
    return GetParam();
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
    return history_helper::ServerHistoryMatchChecker(matcher).Wait();
  }

  bool WaitForLocalHistory(
      const std::map<GURL, testing::Matcher<std::vector<history::VisitRow>>>&
          matchers) {
    return history_helper::LocalHistoryMatchChecker(/*profile_index=*/0,
                                                    GetSyncService(0), matchers)
        .Wait();
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

INSTANTIATE_TEST_SUITE_P(,
                         SingleClientHistorySyncTest,
                         GetSyncTestModes(),
                         testing::PrintToStringParamName());

IN_PROC_BROWSER_TEST_P(SingleClientHistorySyncTest,
                       DoesNotUploadRetroactively) {
  ASSERT_TRUE(SetupClients());

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
  ASSERT_TRUE(SetupSync(WAIT_FOR_SYNC_SETUP_TO_COMPLETE));

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

IN_PROC_BROWSER_TEST_P(SingleClientHistorySyncTest,
                       DoesNotUploadUnsyncableURLs) {
  ASSERT_TRUE(SetupSync());

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

// TODO(crbug.com/40871747): EnterSyncPausedStateForPrimaryAccount is currently
// not supported on Android. Enable these tests once it is.
#if !BUILDFLAG(IS_ANDROID)
IN_PROC_BROWSER_TEST_P(SingleClientHistorySyncTest, DoesNotUploadWhilePaused) {
  ASSERT_TRUE(SetupSync());

  // Navigate somewhere and make sure the URL arrives on the server.
  GURL synced_url1 =
      embedded_test_server()->GetURL("synced1.com", "/sync/simple.html");
  NavigateToURL(synced_url1);
  ASSERT_TRUE(
      WaitForServerHistory(UnorderedElementsAre(UrlIs(synced_url1.spec()))));

  // Enter the Sync-paused state.
  if (GetSetupSyncMode() == SetupSyncMode::kSyncTheFeature) {
    GetClient(0)->EnterSyncPausedStateForPrimaryAccount();
  } else {
    GetClient(0)->EnterSignInPendingStateForPrimaryAccount();
  }
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

  if (GetSetupSyncMode() == SetupSyncMode::kSyncTheFeature) {
    GetClient(0)->ExitSyncPausedStateForPrimaryAccount();
  } else {
    GetClient(0)->ExitSignInPendingStateForPrimaryAccount();
  }
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

// Session total duration is not instrumented on ChromeOS.
#if !BUILDFLAG(IS_CHROMEOS)
IN_PROC_BROWSER_TEST_P(SingleClientHistorySyncTest,
                       PRE_ReportsSessionTotalDurationWhilePaused) {
  {
    ASSERT_TRUE(SetupSync());

    base::HistogramTester histograms;
    metrics::DesktopSessionDurationTracker::Get()->EndSessionForTesting();

    histograms.ExpectTotalCount(kMetricNameWithHistorySync, 1);
    histograms.ExpectTotalCount(kMetricNameWithHistorySyncWithoutAuthError, 1);
  }

  if (GetSetupSyncMode() == SetupSyncMode::kSyncTheFeature) {
    GetClient(0)->EnterSyncPausedStateForPrimaryAccount();
  } else {
    GetClient(0)->EnterSignInPendingStateForPrimaryAccount();
  }
  ASSERT_EQ(GetSyncService(0)->GetTransportState(),
            syncer::SyncService::TransportState::PAUSED);
  ASSERT_TRUE(GetSyncService(0)->HasCachedPersistentAuthErrorForMetrics());

  base::HistogramTester histograms;
  metrics::DesktopSessionDurationTracker::Get()->EndSessionForTesting();

  histograms.ExpectTotalCount(kMetricNameWithHistorySync, 1);
  histograms.ExpectTotalCount(kMetricNameWithHistorySyncWithoutAuthError, 0);
}

IN_PROC_BROWSER_TEST_P(SingleClientHistorySyncTest,
                       ReportsSessionTotalDurationWhilePaused) {
  // Invoke the base class directly as this test doesn't need a tab, and it
  // allows verifying that the very initial transport state is *not* PAUSED.
  ASSERT_TRUE(SyncTest::SetupClients());

  ASSERT_NE(GetSyncService(0)->GetTransportState(),
            syncer::SyncService::TransportState::PAUSED);
  ASSERT_TRUE(GetSyncService(0)->HasCachedPersistentAuthErrorForMetrics());

  {
    base::HistogramTester histograms;
    metrics::DesktopSessionDurationTracker::Get()->EndSessionForTesting();

    histograms.ExpectTotalCount(kMetricNameWithHistorySync, 1);
    histograms.ExpectTotalCount(kMetricNameWithHistorySyncWithoutAuthError, 0);
  }

  ASSERT_TRUE(GetClient(0)->AwaitSyncTransportPaused());

  if (GetSetupSyncMode() == SetupSyncMode::kSyncTheFeature) {
    GetClient(0)->ExitSyncPausedStateForPrimaryAccount();
  } else {
    GetClient(0)->ExitSignInPendingStateForPrimaryAccount();
  }
  ASSERT_EQ(GetSyncService(0)->GetTransportState(),
            syncer::SyncService::TransportState::ACTIVE);

  {
    base::HistogramTester histograms;
    metrics::DesktopSessionDurationTracker::Get()->EndSessionForTesting();

    histograms.ExpectTotalCount(kMetricNameWithHistorySync, 1);
    histograms.ExpectTotalCount(kMetricNameWithHistorySyncWithoutAuthError, 1);
  }
}
#endif  // !BUILDFLAG(IS_CHROMEOS)
#endif  // !BUILDFLAG(IS_ANDROID)

IN_PROC_BROWSER_TEST_P(SingleClientHistorySyncTest, UploadsAllFields) {
  ASSERT_TRUE(SetupSync());

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

IN_PROC_BROWSER_TEST_P(SingleClientHistorySyncTest,
                       UploadsMarkVisitAsKnownToSync) {
  ASSERT_TRUE(SetupSync());

  // Navigate to some URL, and make sure it shows up on the server.
  GURL url1 =
      embedded_test_server()->GetURL("www.host1.com", "/sync/simple.html");
  NavigateToURL(url1, ui::PAGE_TRANSITION_AUTO_BOOKMARK);
  ASSERT_TRUE(WaitForServerHistory(UnorderedElementsAre(
      AllOf(StandardFieldsArePopulated(), UrlIs(url1.spec())))));

  // Now also verify that the local visit is marked as known to sync.
  history::VisitVector visits =
      history_helper::GetVisitsForURLFromClient(/*index=*/0, url1);
  ASSERT_EQ(visits.size(), 1U);
  EXPECT_TRUE(visits[0].is_known_to_sync);
}

IN_PROC_BROWSER_TEST_P(SingleClientHistorySyncTest, UploadsServerRedirect) {
  ASSERT_TRUE(SetupSync());

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

IN_PROC_BROWSER_TEST_P(SingleClientHistorySyncTest, UploadsClientMetaRedirect) {
  ASSERT_TRUE(SetupSync());

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

IN_PROC_BROWSER_TEST_P(SingleClientHistorySyncTest, UploadsClientJSRedirect) {
  ASSERT_TRUE(SetupSync());

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

IN_PROC_BROWSER_TEST_P(SingleClientHistorySyncTest,
                       UploadsReplaceStateNavigation) {
  ASSERT_TRUE(SetupSync());

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

IN_PROC_BROWSER_TEST_P(SingleClientHistorySyncTest, UploadsExternalReferrer) {
  ASSERT_TRUE(SetupSync());

  // Navigate to some URL, and specify a referrer that is not actually in the
  // history DB.
  GURL referrer("https://www.referrer.com/");
  GURL url =
      embedded_test_server()->GetURL("www.host.com", "/sync/simple.html");
  NavigateToURL(url, ui::PAGE_TRANSITION_LINK, referrer);

  EXPECT_TRUE(WaitForServerHistory(UnorderedElementsAre(
      AllOf(StandardFieldsArePopulated(), UrlIs(url.spec()),
            Not(HasReferringVisit()), ReferrerURLIs(referrer.spec())))));
}

IN_PROC_BROWSER_TEST_P(SingleClientHistorySyncTest, DownloadsAndMerges) {
  ASSERT_TRUE(SetupClients());

  // Before Sync gets enabled, one URL exists locally, one remotely, and one in
  // both places.
  const GURL url_local("https://www.url-local.com");
  const GURL url_remote("https://www.url-remote.com");
  const GURL url_both("https://www.url-both.com");

  history_helper::AddUrlToHistory(/*index=*/0, url_local);
  history_helper::AddUrlToHistory(/*index=*/0, url_both);

  GetFakeServer()->InjectEntity(CreateFakeServerEntity(CreateSpecifics(
      base::Time::Now() - base::Minutes(5), "other_cache_guid", url_remote)));
  GetFakeServer()->InjectEntity(CreateFakeServerEntity(CreateSpecifics(
      base::Time::Now() - base::Minutes(4), "other_cache_guid", url_both)));

  // Turn on Sync - this should cause the two remote URLs to get downloaded and
  // merged with the existing local ones.
  ASSERT_TRUE(SetupSync());

  // Now the "local" and "remote" URLs should have one visit each, while the
  // "both" one should have two.
  history::URLRow row_local;
  EXPECT_TRUE(
      history_helper::GetUrlFromClient(/*index=*/0, url_local, &row_local));
  EXPECT_EQ(row_local.visit_count(), 1);

  history::URLRow row_remote;
  EXPECT_TRUE(
      history_helper::GetUrlFromClient(/*index=*/0, url_remote, &row_remote));
  EXPECT_EQ(row_remote.visit_count(), 1);

  history::URLRow row_both;
  EXPECT_TRUE(
      history_helper::GetUrlFromClient(/*index=*/0, url_both, &row_both));
  EXPECT_EQ(row_both.visit_count(), 2);
}

IN_PROC_BROWSER_TEST_P(SingleClientHistorySyncTest,
                       ObserversCallBothOnURLVisitedForSyncedVisits) {
  ASSERT_TRUE(SetupClients());

  history::HistoryService* history_service =
      HistoryServiceFactory::GetForProfile(GetProfile(0),
                                           ServiceAccessType::EXPLICIT_ACCESS);

  MockHistoryServiceObserver mock_observer;
  history_service->AddObserver(&mock_observer);

  const GURL url_remote("https://www.url-remote.com");
  GetFakeServer()->InjectEntity(CreateFakeServerEntity(CreateSpecifics(
      base::Time::Now() - base::Minutes(5), "other_cache_guid", url_remote)));

  // The History Service Observer should be called for the synced visit.
  history::VisitRow visit_row;
  history::VisitRow visit_row2;
  EXPECT_CALL(mock_observer, OnURLVisited(history_service, _))
      .WillOnce(
          [&](history::HistoryService*, const history::VisitedURLInfo& info) {
            visit_row = info.visit_row;
          });
  EXPECT_CALL(mock_observer, OnURLVisitedWithNavigationId(history_service, _))
      .WillOnce(
          [&](history::HistoryService*, const history::VisitedURLInfo& info) {
            visit_row2 = info.visit_row;
          });

  // Turn on Sync - this should cause the remote URL to get downloaded.
  ASSERT_TRUE(SetupSync());

  // The remote URL should have one visit marked as known to Sync.
  history::URLRow row_remote;
  EXPECT_TRUE(
      history_helper::GetUrlFromClient(/*index=*/0, url_remote, &row_remote));
  EXPECT_EQ(row_remote.visit_count(), 1);

  history::VisitVector visits =
      history_helper::GetVisitsFromClient(/*index=*/0, row_remote.id());
  ASSERT_EQ(visits.size(), 1U);
  EXPECT_TRUE(visits[0].is_known_to_sync);

  // Both observer calls should have received the same fields as the synced
  // visit.
  EXPECT_EQ(visit_row.url_id, visits[0].url_id);
  EXPECT_EQ(visit_row.originator_cache_guid, visits[0].originator_cache_guid);

  EXPECT_EQ(visit_row2.url_id, visits[0].url_id);
  EXPECT_EQ(visit_row2.originator_cache_guid, visits[0].originator_cache_guid);

  history_service->RemoveObserver(&mock_observer);
}

IN_PROC_BROWSER_TEST_P(SingleClientHistorySyncTest,
                       DownloadsAndMarksRemoteVisitAsKnownToSync) {
  ASSERT_TRUE(SetupClients());

  // This simple test only has a single remote visit.
  const GURL url_remote("https://www.url-remote.com");
  GetFakeServer()->InjectEntity(CreateFakeServerEntity(CreateSpecifics(
      base::Time::Now() - base::Minutes(5), "other_cache_guid", url_remote)));

  // Turn on Sync - this should download the single remote visit.
  ASSERT_TRUE(SetupSync());

  // The "remote" URLs should have one visit marked as known to Sync.
  history::URLRow row_remote;
  EXPECT_TRUE(
      history_helper::GetUrlFromClient(/*index=*/0, url_remote, &row_remote));
  EXPECT_EQ(row_remote.visit_count(), 1);

  history::VisitVector visits =
      history_helper::GetVisitsFromClient(/*index=*/0, row_remote.id());
  ASSERT_EQ(visits.size(), 1U);
  EXPECT_TRUE(visits[0].is_known_to_sync);
}

IN_PROC_BROWSER_TEST_P(SingleClientHistorySyncTest,
                       DownloadsServerRedirectChain) {
  const GURL url1("https://www.url1.com");
  const GURL url2("https://www.url2.com");
  const GURL url3("https://www.url3.com");

  GetFakeServer()->InjectEntity(CreateFakeServerEntity(
      CreateSpecifics(base::Time::Now() - base::Minutes(5), "other_cache_guid",
                      {url1, url2, url3}, {101, 102, 103})));

  ASSERT_TRUE(SetupSync());

  // Make sure the chain arrived intact.
  history::URLRow url_row;
  EXPECT_TRUE(history_helper::GetUrlFromClient(/*index=*/0, url3, &url_row));
  history::VisitVector visits =
      history_helper::GetVisitsFromClient(/*index=*/0, url_row.id());
  ASSERT_EQ(visits.size(), 1u);
  history::VisitVector redirect_chain =
      history_helper::GetRedirectChainFromClient(/*index=*/0, visits[0]);
  ASSERT_EQ(redirect_chain.size(), 3u);

  history::URLRow url_row1;
  EXPECT_TRUE(history_helper::GetUrlFromClient(
      /*index=*/0, redirect_chain[0].url_id, &url_row1));
  EXPECT_EQ(url_row1.url(), url1);
  history::URLRow url_row2;
  EXPECT_TRUE(history_helper::GetUrlFromClient(
      /*index=*/0, redirect_chain[1].url_id, &url_row2));
  EXPECT_EQ(url_row2.url(), url2);
  history::URLRow url_row3;
  EXPECT_TRUE(history_helper::GetUrlFromClient(
      /*index=*/0, redirect_chain[2].url_id, &url_row3));
  EXPECT_EQ(url_row3.url(), url3);
}

IN_PROC_BROWSER_TEST_P(SingleClientHistorySyncTest,
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

  ASSERT_TRUE(SetupSync());

  // Make sure the chain arrived intact (i.e. was stitched back together).
  history::URLRow url_row;
  EXPECT_TRUE(history_helper::GetUrlFromClient(/*index=*/0, url2, &url_row));
  history::VisitVector visits =
      history_helper::GetVisitsFromClient(/*index=*/0, url_row.id());
  ASSERT_EQ(visits.size(), 1u);
  history::VisitVector redirect_chain =
      history_helper::GetRedirectChainFromClient(/*index=*/0, visits[0]);
  ASSERT_EQ(redirect_chain.size(), 2u);

  history::URLRow url_row1;
  EXPECT_TRUE(history_helper::GetUrlFromClient(
      /*index=*/0, redirect_chain[0].url_id, &url_row1));
  EXPECT_EQ(url_row1.url(), url1);
  history::URLRow url_row2;
  EXPECT_TRUE(history_helper::GetUrlFromClient(
      /*index=*/0, redirect_chain[1].url_id, &url_row2));
  EXPECT_EQ(url_row2.url(), url2);
}

IN_PROC_BROWSER_TEST_P(SingleClientHistorySyncTest,
                       DownloadsAndRemapsReferrer) {
  const GURL url1("https://www.url1.com");
  const GURL url2("https://www.url2.com");

  sync_pb::HistorySpecifics specifics1 = CreateSpecifics(
      base::Time::Now() - base::Minutes(5), "other_cache_guid", url1, 101);
  sync_pb::HistorySpecifics specifics2 = CreateSpecifics(
      base::Time::Now() - base::Minutes(4), "other_cache_guid", url2, 102);
  // The second visit has the first one as a referrer.
  specifics2.set_originator_referring_visit_id(101);
  specifics2.set_referrer_url(url1.spec());

  GetFakeServer()->InjectEntity(CreateFakeServerEntity(specifics1));
  GetFakeServer()->InjectEntity(CreateFakeServerEntity(specifics2));

  ASSERT_TRUE(SetupSync());

  // Make sure the visits arrived, and the referrer link got properly remapped.
  // Also grab their local visit IDs.
  history::VisitID visit_id1 = history::kInvalidVisitID;
  history::VisitID visit_id2 = history::kInvalidVisitID;
  {
    history::VisitVector visits1 =
        history_helper::GetVisitsForURLFromClient(/*index=*/0, url1);
    ASSERT_EQ(visits1.size(), 1u);
    visit_id1 = visits1[0].visit_id;

    history::VisitVector visits2 =
        history_helper::GetVisitsForURLFromClient(/*index=*/0, url2);
    ASSERT_EQ(visits2.size(), 1u);
    visit_id2 = visits2[0].visit_id;

    EXPECT_EQ(visits2[0].referring_visit, visits1[0].visit_id);
    // Since there is an actual referrer visit, the external referrer URL should
    // be empty.
    EXPECT_TRUE(visits2[0].external_referrer_url.is_empty());
  }

  // Update the visits on the server.
  specifics1.set_visit_duration_micros(1234);
  specifics2.set_visit_duration_micros(5678);
  GetFakeServer()->InjectEntity(CreateFakeServerEntity(specifics1));
  GetFakeServer()->InjectEntity(CreateFakeServerEntity(specifics2));

#if BUILDFLAG(IS_ANDROID)
  // On Android, invalidations for HISTORY are disabled, so trigger an explicit
  // refresh to fetch the updated data.
  GetSyncService(0)->TriggerRefresh(
      syncer::SyncService::TriggerRefreshSource::kUnknown, {syncer::HISTORY});
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
        history_helper::GetVisitsForURLFromClient(/*index=*/0, url1);
    ASSERT_EQ(visits1.size(), 1u);

    history::VisitVector visits2 =
        history_helper::GetVisitsForURLFromClient(/*index=*/0, url2);
    ASSERT_EQ(visits2.size(), 1u);

    // The local visit IDs shouldn't have changed.
    EXPECT_EQ(visits1[0].visit_id, visit_id1);
    EXPECT_EQ(visits2[0].visit_id, visit_id2);

    // The updated visit durations should've been applied.
    EXPECT_EQ(visits1[0].visit_duration, base::Microseconds(1234));
    EXPECT_EQ(visits2[0].visit_duration, base::Microseconds(5678));

    // And finally, the referrer link should still exist.
    EXPECT_EQ(visits2[0].referring_visit, visits1[0].visit_id);
    // Since there is an actual referrer visit, the external referrer URL should
    // still be empty.
    EXPECT_TRUE(visits2[0].external_referrer_url.is_empty());
  }
}

IN_PROC_BROWSER_TEST_P(SingleClientHistorySyncTest, DownloadsExternalReferrer) {
  const GURL url("https://www.url.com");
  const GURL referrer("https://www.referrer.com");

  sync_pb::HistorySpecifics specifics = CreateSpecifics(
      base::Time::Now() - base::Minutes(5), "other_cache_guid", url, 101);
  // The foreign visit has a referrer URL, but no referring visit ID.
  specifics.set_referrer_url(referrer.spec());

  GetFakeServer()->InjectEntity(CreateFakeServerEntity(specifics));

  ASSERT_TRUE(SetupSync());

  // Make sure the visit arrived, and its referrer URL was stored as an
  // "external" referrer.
  history::VisitVector visits =
      history_helper::GetVisitsForURLFromClient(/*index=*/0, url);
  ASSERT_EQ(visits.size(), 1u);
  history::VisitRow visit = visits[0];
  EXPECT_EQ(visit.referring_visit, history::kInvalidVisitID);
  EXPECT_EQ(visit.external_referrer_url, referrer);
}

IN_PROC_BROWSER_TEST_P(SingleClientHistorySyncTest,
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

  ASSERT_TRUE(SetupSync());

  // None of these should have made it into the history DB.
  EXPECT_TRUE(
      history_helper::GetVisitsForURLFromClient(/*index=*/0, url1).empty());
  EXPECT_TRUE(
      history_helper::GetVisitsForURLFromClient(/*index=*/0, url2).empty());
  EXPECT_TRUE(
      history_helper::GetVisitsForURLFromClient(/*index=*/0, url3).empty());
}

IN_PROC_BROWSER_TEST_P(SingleClientHistorySyncTest,
                       RecordsLatencyForIncrementalUpdates) {
  const base::Time now = base::Time::Now();
  // Lots of history exists on the server - enough to require multiple
  // GetUpdates requests.
  GetFakeServer()->SetMaxGetUpdatesBatchSize(10);
  for (int i = 0; i < 30; i++) {
    const GURL url(base::StringPrintf("https://www.url%i.com", i));
    GetFakeServer()->InjectEntity(CreateFakeServerEntity(
        CreateSpecifics(now - base::Seconds(60 + i), "other_cache_guid", url)));
  }

  base::HistogramTester histograms;

  // Turn on Sync - this causes all of the remote URLs to get downloaded.
  ASSERT_TRUE(SetupSync());

  // Spot-check that the URLs made it to the client.
  history::URLRow row0;
  ASSERT_TRUE(history_helper::GetUrlFromClient(
      /*index=*/0, GURL("https://www.url0.com"), &row0));
  ASSERT_EQ(row0.visit_count(), 1);

  history::URLRow row29;
  ASSERT_TRUE(history_helper::GetUrlFromClient(
      /*index=*/0, GURL("https://www.url29.com"), &row29));
  ASSERT_EQ(row29.visit_count(), 1);

  // Since this was all the initial sync (even across multiple GetUpdates
  // requests), no latency metrics should have been reported.
  histograms.ExpectTotalCount(
      "Sync.NonReflectionUpdateFreshnessPossiblySkewed2.HISTORY", 0);

  // Add another URL to the server, simulating that the user is browsing on a
  // different device.
  const GURL new_url("https://www.new-url.com");
  GetFakeServer()->InjectEntity(CreateFakeServerEntity(
      CreateSpecifics(now - base::Seconds(1), "other_cache_guid", new_url)));
#if BUILDFLAG(IS_ANDROID)
  // On Android, invalidations for HISTORY are disabled by default, so
  // explicitly trigger a GetUpdates.
  GetSyncService(0)->TriggerRefresh(
      syncer::SyncService::TriggerRefreshSource::kUnknown, {syncer::HISTORY});
#endif  // BUILDFLAG(IS_ANDROID)
  WaitForLocalHistory({{new_url, testing::SizeIs(1)}});

  // The latency of this update should've been recorded.
  histograms.ExpectTotalCount(
      "Sync.NonReflectionUpdateFreshnessPossiblySkewed2.HISTORY", 1);
}

// Signing out or turning off Sync isn't possible on ChromeOS.
#if !BUILDFLAG(IS_CHROMEOS)

IN_PROC_BROWSER_TEST_P(SingleClientHistorySyncTest,
                       ClearsForeignHistoryOnTurningSyncOff) {
  ASSERT_TRUE(SetupClients());

  // Before Sync gets enabled, one URL exists locally, one remotely, and one
  // redirect chain consisting of 3 URLs also remotely.
  const GURL url_local("https://www.url-local.com");

  const GURL url_remote("https://www.url-remote.com");
  const GURL url_remote_chain1("https://www.url-remote1.com");
  const GURL url_remote_chain2("https://www.url-remote2.com");
  const GURL url_remote_chain3("https://www.url-remote3.com");

  history_helper::AddUrlToHistory(/*index=*/0, url_local);

  GetFakeServer()->InjectEntity(CreateFakeServerEntity(CreateSpecifics(
      base::Time::Now() - base::Minutes(5), "other_cache_guid", url_remote)));

  GetFakeServer()->InjectEntity(CreateFakeServerEntity(
      CreateSpecifics(base::Time::Now() - base::Minutes(5), "other_cache_guid",
                      {url_remote_chain1, url_remote_chain2, url_remote_chain3},
                      {101, 102, 103})));

  // Turn on Sync - this will cause the remote URLs to get downloaded.
  ASSERT_TRUE(SetupSync());

  // Make sure the "local" and "remote" URLs both exist in the DB.
  history::URLRow row;
  ASSERT_TRUE(history_helper::GetUrlFromClient(/*index=*/0, url_local, &row));
  ASSERT_TRUE(history_helper::GetUrlFromClient(/*index=*/0, url_remote, &row));
  ASSERT_TRUE(
      history_helper::GetUrlFromClient(/*index=*/0, url_remote_chain1, &row));
  ASSERT_TRUE(
      history_helper::GetUrlFromClient(/*index=*/0, url_remote_chain2, &row));
  ASSERT_TRUE(
      history_helper::GetUrlFromClient(/*index=*/0, url_remote_chain3, &row));

  // Turn Sync off by removing the primary account.
  GetClient(0)->SignOutPrimaryAccount();
  ASSERT_EQ(GetSyncService(0)->GetTransportState(),
            syncer::SyncService::TransportState::DISABLED);

  // This should have triggered the deletion of foreign history, both the
  // individual visit and the redirect chain (but left local history alone).
  EXPECT_TRUE(history_helper::GetUrlFromClient(/*index=*/0, url_local, &row));
  EXPECT_FALSE(history_helper::GetUrlFromClient(/*index=*/0, url_remote, &row));
  EXPECT_FALSE(
      history_helper::GetUrlFromClient(/*index=*/0, url_remote_chain1, &row));
  EXPECT_FALSE(
      history_helper::GetUrlFromClient(/*index=*/0, url_remote_chain2, &row));
  EXPECT_FALSE(
      history_helper::GetUrlFromClient(/*index=*/0, url_remote_chain3, &row));
}

IN_PROC_BROWSER_TEST_P(SingleClientHistorySyncTest,
                       ClearsForeignHistoryOnTurningSyncOffInTwoSteps) {
  ASSERT_TRUE(SetupClients());

  // Before Sync gets enabled, one URL exists locally, one remotely.
  const GURL url_local("https://www.url-local.com");
  const GURL url_remote("https://www.url-remote.com");

  history_helper::AddUrlToHistory(/*index=*/0, url_local);

  GetFakeServer()->InjectEntity(CreateFakeServerEntity(CreateSpecifics(
      base::Time::Now() - base::Minutes(5), "other_cache_guid", url_remote)));

  // Turn on Sync - this will cause the remote URL to get downloaded.
  ASSERT_TRUE(SetupSync());

  // Make sure the "local" and "remote" URLs both exist in the DB.
  history::URLRow row;
  ASSERT_TRUE(history_helper::GetUrlFromClient(/*index=*/0, url_local, &row));
  ASSERT_TRUE(history_helper::GetUrlFromClient(/*index=*/0, url_remote, &row));

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
  EXPECT_TRUE(history_helper::GetUrlFromClient(/*index=*/0, url_local, &row));
  EXPECT_FALSE(history_helper::GetUrlFromClient(/*index=*/0, url_remote, &row));
}

IN_PROC_BROWSER_TEST_P(SingleClientHistorySyncTest,
                       DoesNotDuplicateEntriesWhenTurningSyncOffAndOnAgain) {
  ASSERT_TRUE(SetupClients());

  // One URL exists on the server already.
  const GURL url_other_client("https://www.other-client.com");
  GetFakeServer()->InjectEntity(CreateFakeServerEntity(
      CreateSpecifics(base::Time::Now() - base::Minutes(5), "other_cache_guid",
                      url_other_client)));

  // Turn on Sync.
  ASSERT_TRUE(SetupSync());

  // After Sync was enabled, navigate somewhere, and make sure this arrives on
  // the server.
  GURL url_this_client =
      embedded_test_server()->GetURL("this-client.com", "/sync/simple.html");
  NavigateToURL(url_this_client);
  ASSERT_TRUE(WaitForServerHistory(UnorderedElementsAre(
      UrlIs(url_other_client.spec()), UrlIs(url_this_client.spec()))));

  // Turn Sync off by removing the primary account.
  GetClient(0)->SignOutPrimaryAccount();
  ASSERT_EQ(GetSyncService(0)->GetTransportState(),
            syncer::SyncService::TransportState::DISABLED);

  // The visit that happened on this device is still here.
  history::URLRow row;
  ASSERT_TRUE(
      history_helper::GetUrlFromClient(/*index=*/0, url_this_client, &row));
  ASSERT_EQ(history_helper::GetVisitsFromClient(0, row.id()).size(), 1u);
  // ..but the remote visit isn't
  ASSERT_FALSE(
      history_helper::GetUrlFromClient(/*index=*/0, url_other_client, &row));

  // Turn Sync back on.
  ASSERT_TRUE(GetClient(0)->SetupSync());
  ASSERT_TRUE(
      GetSyncService(0)->GetActiveDataTypes().Has(syncer::DataType::HISTORY));

  // Wait for the remote data to be re-downloaded.
  ASSERT_TRUE(
      WaitForLocalHistory({{url_other_client, UnorderedElementsAre(_)}}));

  // Sanity check: The remote URL came back.
  ASSERT_TRUE(
      history_helper::GetUrlFromClient(/*index=*/0, url_other_client, &row));
  // There should still be only a single visit for the synced URL.
  ASSERT_TRUE(
      history_helper::GetUrlFromClient(/*index=*/0, url_this_client, &row));
  EXPECT_EQ(history_helper::GetVisitsFromClient(0, row.id()).size(), 1u);
}

#endif  // !BUILDFLAG(IS_CHROMEOS)

IN_PROC_BROWSER_TEST_P(SingleClientHistorySyncTest,
                       HistorySyncDisabledForManagedAccount) {
  ASSERT_TRUE(SetupSync(SyncTestAccount::kEnterpriseAccount1));

  ASSERT_FALSE(GetSyncService(0)->GetActiveDataTypes().empty());
  EXPECT_FALSE(GetSyncService(0)->GetActiveDataTypes().Has(syncer::HISTORY));
}

IN_PROC_BROWSER_TEST_P(SingleClientHistorySyncTest,
                       HistorySyncEnabledForNonManagedAccount) {
  ASSERT_TRUE(SetupSync(SyncTestAccount::kConsumerAccount1));

  EXPECT_TRUE(GetSyncService(0)->GetActiveDataTypes().Has(syncer::HISTORY));
}

}  // namespace
