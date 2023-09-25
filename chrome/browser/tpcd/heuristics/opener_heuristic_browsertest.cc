// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/test/bind.h"
#include "base/test/simple_test_clock.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/dips/dips_service.h"
#include "chrome/browser/dips/dips_storage.h"
#include "chrome/browser/dips/dips_test_utils.h"
#include "chrome/browser/dips/dips_utils.h"
#include "chrome/browser/tpcd/heuristics/opener_heuristic_metrics.h"
#include "chrome/browser/tpcd/heuristics/opener_heuristic_tab_helper.h"
#include "chrome/browser/tpcd/heuristics/opener_heuristic_utils.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "components/ukm/test_ukm_recorder.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/hit_test_region_observer.h"
#include "net/dns/mock_host_resolver.h"
#include "services/metrics/public/cpp/ukm_source.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "third_party/blink/public/common/switches.h"

using content::NavigationHandle;
using content::RenderFrameHost;
using content::WebContents;
using content::WebContentsObserver;
using testing::ElementsAre;
using testing::Pair;

namespace {

// Waits for a pop-up to open.
class PopupObserver : public WebContentsObserver {
 public:
  explicit PopupObserver(
      WebContents* web_contents,
      WindowOpenDisposition open_disposition = WindowOpenDisposition::NEW_POPUP)
      : WebContentsObserver(web_contents),
        open_disposition_(open_disposition) {}

  void Wait() { run_loop_.Run(); }
  WebContents* popup() { return popup_; }

 private:
  // WebContentsObserver overrides:
  void DidOpenRequestedURL(WebContents* new_contents,
                           RenderFrameHost* source_render_frame_host,
                           const GURL& url,
                           const content::Referrer& referrer,
                           WindowOpenDisposition disposition,
                           ui::PageTransition transition,
                           bool started_from_context_menu,
                           bool renderer_initiated) override {
    if (!popup_ && disposition == open_disposition_) {
      popup_ = new_contents;
      run_loop_.Quit();
    }
  }

  const WindowOpenDisposition open_disposition_;
  raw_ptr<WebContents> popup_ = nullptr;
  base::RunLoop run_loop_;
};

// Waits for a navigation in the primary main frame to finish.
class NavigationFinishObserver : public WebContentsObserver {
 public:
  explicit NavigationFinishObserver(WebContents* web_contents)
      : WebContentsObserver(web_contents) {}

  void Wait() { run_loop_.Run(); }

 private:
  // WebContentsObserver overrides:
  void DidFinishNavigation(NavigationHandle* navigation_handle) override {
    if (!navigation_handle->IsInPrimaryMainFrame()) {
      return;
    }
    run_loop_.Quit();
  }

  base::RunLoop run_loop_;
};

}  // namespace

class OpenerHeuristicBrowserTest : public PlatformBrowserTest {
 public:
  void SetUp() override {
    OpenerHeuristicTabHelper::SetClockForTesting(&clock_);
    PlatformBrowserTest::SetUp();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    // Prevents flakiness by handling clicks even before content is drawn.
    command_line->AppendSwitch(blink::switches::kAllowPreCommitInput);
  }

  void SetUpOnMainThread() override {
    ASSERT_TRUE(embedded_test_server()->Start());
    host_resolver()->AddRule("a.test", "127.0.0.1");
    host_resolver()->AddRule("b.test", "127.0.0.1");
    host_resolver()->AddRule("sub.b.test", "127.0.0.1");
    host_resolver()->AddRule("c.test", "127.0.0.1");
    host_resolver()->AddRule("google.com", "127.0.0.1");
    DIPSService::Get(GetActiveWebContents()->GetBrowserContext())
        ->SetStorageClockForTesting(&clock_);
  }

  content::WebContents* GetActiveWebContents() {
    return chrome_test_utils::GetActiveWebContents(this);
  }

  OpenerHeuristicTabHelper* GetTabHelper() {
    return OpenerHeuristicTabHelper::FromWebContents(GetActiveWebContents());
  }

  DIPSService* GetDipsService() {
    return DIPSService::Get(GetActiveWebContents()->GetBrowserContext());
  }

  void RecordInteraction(const GURL& url, base::Time time) {
    auto* dips = GetDipsService();
    dips->storage()
        ->AsyncCall(&DIPSStorage::RecordInteraction)
        .WithArgs(url, time, dips->GetCookieMode());
    dips->storage()->FlushPostedTasksForTesting();
  }

  // Open a popup window with the given URL and return its WebContents.
  base::expected<WebContents*, std::string> OpenPopup(const GURL& url) {
    auto* web_contents = GetActiveWebContents();
    PopupObserver observer(web_contents);
    if (!content::ExecJs(
            web_contents,
            content::JsReplace("window.open($1, '', 'popup');", url))) {
      return base::unexpected("window.open failed");
    }
    observer.Wait();

    // Wait for the popup to finish navigating to its initial URL.
    NavigationFinishObserver(observer.popup()).Wait();

    // Wait for the read of the past interaction from the DIPS DB to complete,
    // so the PopupPastInteraction UKM event is reported.
    GetDipsService()->storage()->FlushPostedTasksForTesting();

    return observer.popup();
  }

  void SimulateMouseClick(WebContents* web_contents) {
    content::WaitForHitTestData(web_contents->GetPrimaryMainFrame());
    UserActivationObserver observer(web_contents,
                                    web_contents->GetPrimaryMainFrame());
    content::SimulateMouseClick(web_contents, 0,
                                blink::WebMouseEvent::Button::kLeft);
    observer.Wait();
  }

  void DestroyWebContents(WebContents* web_contents) {
    content::WebContentsDestroyedWatcher destruction_watcher(web_contents);
    web_contents->Close();
    destruction_watcher.Wait();
  }

  base::expected<OptionalBool, std::string> GetOpenerHasSameSiteIframe(
      ukm::TestUkmRecorder& ukm_recorder,
      const std::string& entry_name) {
    auto entries =
        ukm_recorder.GetEntries(entry_name, {"OpenerHasSameSiteIframe"});
    if (entries.size() != 1) {
      return base::unexpected(
          base::StringPrintf("Expected 1 %s entry, found %zu",
                             entry_name.c_str(), entries.size()));
    }
    return static_cast<OptionalBool>(
        entries[0].metrics["OpenerHasSameSiteIframe"]);
  }

  base::SimpleTestClock clock_;
};

IN_PROC_BROWSER_TEST_F(OpenerHeuristicBrowserTest,
                       RootWindowDoesntHavePopupState) {
  ASSERT_FALSE(GetTabHelper()->popup_observer_for_testing());
}

// TODO(crbug.com/1465642): Test is flaky on Android.
#if BUILDFLAG(IS_ANDROID)
#define MAYBE_PopupsWithOpenerHavePopupState \
  DISABLED_PopupsWithOpenerHavePopupState
#else
#define MAYBE_PopupsWithOpenerHavePopupState PopupsWithOpenerHavePopupState
#endif
IN_PROC_BROWSER_TEST_F(OpenerHeuristicBrowserTest,
                       MAYBE_PopupsWithOpenerHavePopupState) {
  WebContents* web_contents = GetActiveWebContents();
  GURL popup_url = embedded_test_server()->GetURL("a.test", "/title1.html");

  PopupObserver observer(web_contents);
  ASSERT_TRUE(content::ExecJs(
      web_contents,
      content::JsReplace("window.open($1, '', 'popup');", popup_url)));
  observer.Wait();

  auto* popup_tab_helper =
      OpenerHeuristicTabHelper::FromWebContents(observer.popup());
  ASSERT_TRUE(popup_tab_helper);
  ASSERT_TRUE(popup_tab_helper->popup_observer_for_testing());
}

// TODO(https://crbug.com/1469394): Flaky on android.
#if BUILDFLAG(IS_ANDROID)
#define MAYBE_PopupsWithoutOpenerDoNotHavePopupState \
  DISABLED_PopupsWithoutOpenerDoNotHavePopupState
#else
#define MAYBE_PopupsWithoutOpenerDoNotHavePopupState \
  PopupsWithoutOpenerDoNotHavePopupState
#endif
IN_PROC_BROWSER_TEST_F(OpenerHeuristicBrowserTest,
                       MAYBE_PopupsWithoutOpenerDoNotHavePopupState) {
  WebContents* web_contents = GetActiveWebContents();
  GURL popup_url = embedded_test_server()->GetURL("a.test", "/title1.html");

  PopupObserver observer(web_contents);
  ASSERT_TRUE(content::ExecJs(
      web_contents,
      content::JsReplace("window.open($1, '', 'popup,noopener');", popup_url)));
  observer.Wait();

  auto* popup_tab_helper =
      OpenerHeuristicTabHelper::FromWebContents(observer.popup());
  ASSERT_TRUE(popup_tab_helper);
  ASSERT_FALSE(popup_tab_helper->popup_observer_for_testing());
}

// TODO(crbug.com/1469394): Flaky on android.
#if BUILDFLAG(IS_ANDROID)
#define MAYBE_NewTabsDoNotHavePopupState DISABLED_NewTabsDoNotHavePopupState
#else
#define MAYBE_NewTabsDoNotHavePopupState NewTabsDoNotHavePopupState
#endif
IN_PROC_BROWSER_TEST_F(OpenerHeuristicBrowserTest,
                       MAYBE_NewTabsDoNotHavePopupState) {
  WebContents* web_contents = GetActiveWebContents();
  GURL popup_url = embedded_test_server()->GetURL("a.test", "/title1.html");

  PopupObserver observer(web_contents,
                         WindowOpenDisposition::NEW_FOREGROUND_TAB);
  ASSERT_TRUE(content::ExecJs(
      web_contents, content::JsReplace("window.open($1);", popup_url)));
  observer.Wait();

  auto* popup_tab_helper =
      OpenerHeuristicTabHelper::FromWebContents(observer.popup());
  ASSERT_TRUE(popup_tab_helper);
  ASSERT_FALSE(popup_tab_helper->popup_observer_for_testing());
}

IN_PROC_BROWSER_TEST_F(OpenerHeuristicBrowserTest,
                       PopupPastInteractionIsNotReportedWithoutInteraction) {
  ukm::TestAutoSetUkmRecorder ukm_recorder;
  GURL popup_url = embedded_test_server()->GetURL("a.test", "/title1.html");

  // Note: no previous interaction on a.test.

  ASSERT_TRUE(OpenPopup(popup_url).has_value());

  auto entries =
      ukm_recorder.GetEntriesByName("OpenerHeuristic.PopupPastInteraction");
  ASSERT_EQ(entries.size(), 0u);
}

IN_PROC_BROWSER_TEST_F(OpenerHeuristicBrowserTest,
                       PopupPastInteractionIsReported_WithoutRedirect) {
  ukm::TestAutoSetUkmRecorder ukm_recorder;
  GURL popup_url = embedded_test_server()->GetURL("a.test", "/title1.html");

  RecordInteraction(GURL("https://a.test"), clock_.Now() - base::Hours(3));

  ASSERT_TRUE(OpenPopup(popup_url).has_value());

  auto entries = ukm_recorder.GetEntries("OpenerHeuristic.PopupPastInteraction",
                                         {"HoursSinceLastInteraction"});
  ASSERT_EQ(entries.size(), 1u);
  // Since the user landed on the page the popup was opened to, the UKM event
  // has source type NAVIGATION_ID.
  EXPECT_EQ(ukm::GetSourceIdType(entries[0].source_id),
            ukm::SourceIdType::NAVIGATION_ID);
  EXPECT_EQ(ukm_recorder.GetSourceForSourceId(entries[0].source_id)->url(),
            popup_url);
  EXPECT_THAT(entries[0].metrics,
              ElementsAre(Pair("HoursSinceLastInteraction", 3)));
}

// TODO(crbug.com/1457925): Test is flaky on Android.
#if BUILDFLAG(IS_ANDROID)
#define MAYBE_PopupPastInteractionIsReported_ServerRedirect \
  DISABLED_PopupPastInteractionIsReported_ServerRedirect
#else
#define MAYBE_PopupPastInteractionIsReported_ServerRedirect \
  PopupPastInteractionIsReported_ServerRedirect
#endif
IN_PROC_BROWSER_TEST_F(OpenerHeuristicBrowserTest,
                       MAYBE_PopupPastInteractionIsReported_ServerRedirect) {
  ukm::TestAutoSetUkmRecorder ukm_recorder;
  GURL popup_url =
      embedded_test_server()->GetURL("a.test", "/server-redirect?title1.html");

  RecordInteraction(GURL("https://a.test"), clock_.Now() - base::Hours(3));

  ASSERT_TRUE(OpenPopup(popup_url).has_value());

  auto entries = ukm_recorder.GetEntries("OpenerHeuristic.PopupPastInteraction",
                                         {"HoursSinceLastInteraction"});
  ASSERT_EQ(entries.size(), 1u);
  // Server redirect causes the UKM event to have source type REDIRECT_ID.
  EXPECT_EQ(ukm::GetSourceIdType(entries[0].source_id),
            ukm::SourceIdType::REDIRECT_ID);
  EXPECT_EQ(ukm_recorder.GetSourceForSourceId(entries[0].source_id)->url(),
            popup_url);
  EXPECT_THAT(entries[0].metrics,
              ElementsAre(Pair("HoursSinceLastInteraction", 3)));
}

// TODO(crbug.com/1485029): Flaky on Android.
#if BUILDFLAG(IS_ANDROID)
#define MAYBE_PopupPastInteractionIsReported_ClientRedirect \
  DISABLED_PopupPastInteractionIsReported_ClientRedirect
#else
#define MAYBE_PopupPastInteractionIsReported_ClientRedirect \
  PopupPastInteractionIsReported_ClientRedirect
#endif
IN_PROC_BROWSER_TEST_F(OpenerHeuristicBrowserTest,
                       MAYBE_PopupPastInteractionIsReported_ClientRedirect) {
  ukm::TestAutoSetUkmRecorder ukm_recorder;
  GURL popup_url =
      embedded_test_server()->GetURL("a.test", "/client-redirect?title1.html");

  RecordInteraction(GURL("https://a.test"), clock_.Now() - base::Hours(3));

  ASSERT_TRUE(OpenPopup(popup_url).has_value());

  auto entries = ukm_recorder.GetEntries("OpenerHeuristic.PopupPastInteraction",
                                         {"HoursSinceLastInteraction"});
  ASSERT_EQ(entries.size(), 1u);
  // With a client redirect, we still get a source of type NAVIGATION_ID (since
  // the URL committed).
  EXPECT_EQ(ukm::GetSourceIdType(entries[0].source_id),
            ukm::SourceIdType::NAVIGATION_ID);
  EXPECT_EQ(ukm_recorder.GetSourceForSourceId(entries[0].source_id)->url(),
            popup_url);
  EXPECT_THAT(entries[0].metrics,
              ElementsAre(Pair("HoursSinceLastInteraction", 3)));
}

IN_PROC_BROWSER_TEST_F(OpenerHeuristicBrowserTest,
                       PopupPastInteractionIsReportedOnlyOnce) {
  ukm::TestAutoSetUkmRecorder ukm_recorder;
  GURL popup_url = embedded_test_server()->GetURL("a.test", "/title1.html");

  RecordInteraction(GURL("https://a.test"), clock_.Now() - base::Hours(3));

  auto maybe_popup = OpenPopup(popup_url);
  ASSERT_TRUE(maybe_popup.has_value()) << maybe_popup.error();

  ASSERT_EQ(
      ukm_recorder.GetEntriesByName("OpenerHeuristic.PopupPastInteraction")
          .size(),
      1u);

  ASSERT_TRUE(content::NavigateToURL(
      *maybe_popup, embedded_test_server()->GetURL("b.test", "/title1.html")));

  // After another navigation, PopupPastInteraction isn't reported again (i.e.,
  // still once total).
  ASSERT_EQ(
      ukm_recorder.GetEntriesByName("OpenerHeuristic.PopupPastInteraction")
          .size(),
      1u);
}

IN_PROC_BROWSER_TEST_F(OpenerHeuristicBrowserTest,
                       PopupPastInteractionIsFollowedByPostPopupCookieAccess) {
  ukm::TestAutoSetUkmRecorder ukm_recorder;
  GURL opener_url = embedded_test_server()->GetURL("a.test", "/title1.html");
  GURL popup_url = embedded_test_server()->GetURL("b.test", "/title1.html");

  // Initialize interaction and popup.
  RecordInteraction(popup_url, clock_.Now() - base::Hours(3));
  ASSERT_TRUE(content::NavigateToURL(GetActiveWebContents(), opener_url));
  ASSERT_TRUE(OpenPopup(popup_url).has_value());
  GetDipsService()->storage()->FlushPostedTasksForTesting();

  // Assert that the UKM events and DIPS entries were recorded.
  int64_t access_id;
  ASSERT_EQ(
      ukm_recorder.GetEntriesByName("OpenerHeuristic.PopupPastInteraction")
          .size(),
      1u);
  auto top_level_entries =
      ukm_recorder.GetEntries("OpenerHeuristic.TopLevel", {"AccessId"});
  ASSERT_EQ(top_level_entries.size(), 1u);
  EXPECT_EQ(
      ukm_recorder.GetSourceForSourceId(top_level_entries[0].source_id)->url(),
      opener_url);
  access_id = top_level_entries[0].metrics["AccessId"];

  base::OnceCallback<void(absl::optional<PopupsStateValue>)> assert_popup =
      base::BindLambdaForTesting([&](absl::optional<PopupsStateValue> state) {
        ASSERT_TRUE(state.has_value());
        EXPECT_EQ(access_id, static_cast<int64_t>(state->access_id));
      });
  GetDipsService()
      ->storage()
      ->AsyncCall(&DIPSStorage::ReadPopup)
      .WithArgs(GetSiteForDIPS(opener_url), GetSiteForDIPS(popup_url))
      .Then(std::move(assert_popup));
  GetDipsService()->storage()->FlushPostedTasksForTesting();

  // We host the "image" on an HTTPS server, because for it to write a
  // cookie, the cookie needs to be SameSite=None and Secure.
  net::EmbeddedTestServer https_server(net::EmbeddedTestServer::TYPE_HTTPS);
  https_server.SetSSLConfig(net::EmbeddedTestServer::CERT_TEST_NAMES);
  https_server.AddDefaultHandlers(
      base::FilePath(FILE_PATH_LITERAL("chrome/test/data")));
  ASSERT_TRUE(https_server.Start());

  // Add a cookie access by popup_url on opener_url.
  ASSERT_TRUE(NavigateToSetCookie(GetActiveWebContents(), &https_server,
                                  "sub.b.test",
                                  /*is_secure_cookie_set=*/true));
  ASSERT_TRUE(content::NavigateToURL(GetActiveWebContents(), opener_url));
  CreateImageAndWaitForCookieAccess(
      GetActiveWebContents(),
      https_server.GetURL("sub.b.test", "/favicon/icon.png"));
  GetDipsService()->storage()->FlushPostedTasksForTesting();

  // Assert that the UKM event for the PostPopupCookieAccess was recorded.
  auto access_entries = ukm_recorder.GetEntries(
      "OpenerHeuristic.PostPopupCookieAccess",
      {"AccessId", "AccessSucceeded", "HoursSincePopupOpened"});
  ASSERT_EQ(access_entries.size(), 1u);
  EXPECT_EQ(
      ukm_recorder.GetSourceForSourceId(access_entries[0].source_id)->url(),
      opener_url);
  EXPECT_EQ(access_entries[0].metrics["AccessId"], access_id);
  EXPECT_EQ(access_entries[0].metrics["AccessSucceeded"], true);
  EXPECT_EQ(access_entries[0].metrics["HoursSincePopupOpened"], 0);
}

IN_PROC_BROWSER_TEST_F(OpenerHeuristicBrowserTest, PopupInteraction) {
  ukm::TestAutoSetUkmRecorder ukm_recorder;
  GURL popup_url = embedded_test_server()->GetURL("a.test", "/title1.html");
  GURL redirect_url =
      embedded_test_server()->GetURL("b.test", "/server-redirect?title1.html");
  GURL final_url = embedded_test_server()->GetURL("b.test", "/title1.html");

  auto maybe_popup = OpenPopup(popup_url);
  ASSERT_TRUE(maybe_popup.has_value()) << maybe_popup.error();

  clock_.Advance(base::Minutes(1));
  ASSERT_TRUE(content::NavigateToURL(*maybe_popup, redirect_url, final_url));

  ASSERT_EQ(
      ukm_recorder.GetEntriesByName("OpenerHeuristic.PopupInteraction").size(),
      0u);

  clock_.Advance(base::Minutes(1));
  SimulateMouseClick(*maybe_popup);

  auto entries = ukm_recorder.GetEntries("OpenerHeuristic.PopupInteraction",
                                         {"SecondsSinceCommitted", "UrlIndex"});
  ASSERT_EQ(entries.size(), 1u);
  EXPECT_EQ(ukm::GetSourceIdType(entries[0].source_id),
            ukm::SourceIdType::NAVIGATION_ID);
  EXPECT_EQ(ukm_recorder.GetSourceForSourceId(entries[0].source_id)->url(),
            final_url);
  // The time between *popup_url* committing and the click.
  EXPECT_EQ(entries[0].metrics["SecondsSinceCommitted"],
            Bucketize3PCDHeuristicTimeDelta(
                base::Minutes(2), base::Minutes(3),
                base::BindRepeating(&base::TimeDelta::InSeconds)));
  // The user clicked on *final_url*, which was the third URL.
  EXPECT_EQ(entries[0].metrics["UrlIndex"], 3);
}

IN_PROC_BROWSER_TEST_F(OpenerHeuristicBrowserTest,
                       PopupInteractionIsOnlyReportedOnce) {
  ukm::TestAutoSetUkmRecorder ukm_recorder;
  GURL popup_url = embedded_test_server()->GetURL("a.test", "/title1.html");
  GURL interaction_url =
      embedded_test_server()->GetURL("b.test", "/title1.html");
  GURL final_url = embedded_test_server()->GetURL("c.test", "/title1.html");

  auto maybe_popup = OpenPopup(popup_url);
  ASSERT_TRUE(maybe_popup.has_value()) << maybe_popup.error();

  ASSERT_TRUE(content::NavigateToURL(*maybe_popup, interaction_url));
  SimulateMouseClick(*maybe_popup);

  ASSERT_EQ(
      ukm_recorder.GetEntriesByName("OpenerHeuristic.PopupInteraction").size(),
      1u);

  ASSERT_TRUE(content::NavigateToURL(*maybe_popup, final_url));
  SimulateMouseClick(*maybe_popup);

  // The second click was not reported (still only 1 total).
  ASSERT_EQ(
      ukm_recorder.GetEntriesByName("OpenerHeuristic.PopupInteraction").size(),
      1u);
}

IN_PROC_BROWSER_TEST_F(OpenerHeuristicBrowserTest,
                       PopupInteraction_IgnoreUncommitted) {
  ukm::TestAutoSetUkmRecorder ukm_recorder;
  GURL popup_url = embedded_test_server()->GetURL("a.test", "/title1.html");
  GURL uncommitted_url = embedded_test_server()->GetURL("c.test", "/nocontent");

  auto maybe_popup = OpenPopup(popup_url);
  ASSERT_TRUE(maybe_popup.has_value()) << maybe_popup.error();

  clock_.Advance(base::Minutes(1));
  // Attempt a navigation which won't commit (because the HTTP response is No
  // Content).
  ASSERT_TRUE(content::NavigateToURL(*maybe_popup, uncommitted_url, popup_url));

  clock_.Advance(base::Minutes(1));
  SimulateMouseClick(*maybe_popup);

  auto entries = ukm_recorder.GetEntries("OpenerHeuristic.PopupInteraction",
                                         {"SecondsSinceCommitted", "UrlIndex"});
  ASSERT_EQ(entries.size(), 1u);
  EXPECT_EQ(ukm::GetSourceIdType(entries[0].source_id),
            ukm::SourceIdType::NAVIGATION_ID);
  EXPECT_EQ(ukm_recorder.GetSourceForSourceId(entries[0].source_id)->url(),
            popup_url);
  // The uncommitted navigation was ignored. UrlIndex is still 1.
  EXPECT_EQ(entries[0].metrics["SecondsSinceCommitted"],
            Bucketize3PCDHeuristicTimeDelta(
                base::Minutes(2), base::Minutes(3),
                base::BindRepeating(&base::TimeDelta::InSeconds)));
  EXPECT_EQ(entries[0].metrics["UrlIndex"], 1);
}

#if BUILDFLAG(IS_MAC)
// Very flaky on macOS 11 Tests: https://crbug.com/1486448
#define MAYBE_PopupInteraction_IsFollowedByPostPopupCookieAccess \
  DISABLED_PopupInteraction_IsFollowedByPostPopupCookieAccess
#else
#define MAYBE_PopupInteraction_IsFollowedByPostPopupCookieAccess \
  PopupInteraction_IsFollowedByPostPopupCookieAccess
#endif
IN_PROC_BROWSER_TEST_F(
    OpenerHeuristicBrowserTest,
    MAYBE_PopupInteraction_IsFollowedByPostPopupCookieAccess) {
  ukm::TestAutoSetUkmRecorder ukm_recorder;
  GURL opener_url = embedded_test_server()->GetURL("a.test", "/title1.html");
  GURL popup_url_1 = embedded_test_server()->GetURL("c.test", "/title1.html");
  GURL popup_url_2 =
      embedded_test_server()->GetURL("b.test", "/server-redirect?title1.html");
  GURL popup_url_3 = embedded_test_server()->GetURL("b.test", "/title1.html");

  // Initialize popup and interaction.
  ASSERT_TRUE(content::NavigateToURL(GetActiveWebContents(), opener_url));
  auto maybe_popup = OpenPopup(popup_url_1);
  ASSERT_TRUE(maybe_popup.has_value()) << maybe_popup.error();

  clock_.Advance(base::Minutes(1));
  ASSERT_TRUE(content::NavigateToURL(*maybe_popup, popup_url_2, popup_url_3));

  clock_.Advance(base::Minutes(1));
  SimulateMouseClick(*maybe_popup);
  GetDipsService()->storage()->FlushPostedTasksForTesting();

  // Assert that the UKM events and DIPS entries were recorded.
  int64_t access_id;
  ASSERT_EQ(
      ukm_recorder.GetEntriesByName("OpenerHeuristic.PopupInteraction").size(),
      1u);
  auto top_level_entries =
      ukm_recorder.GetEntries("OpenerHeuristic.TopLevel", {"AccessId"});
  ASSERT_EQ(top_level_entries.size(), 1u);
  EXPECT_EQ(
      ukm_recorder.GetSourceForSourceId(top_level_entries[0].source_id)->url(),
      opener_url);
  access_id = top_level_entries[0].metrics["AccessId"];

  base::OnceCallback<void(absl::optional<PopupsStateValue>)> assert_popup =
      base::BindLambdaForTesting([&](absl::optional<PopupsStateValue> state) {
        ASSERT_TRUE(state.has_value());
        EXPECT_EQ(access_id, static_cast<int64_t>(state->access_id));
      });
  GetDipsService()
      ->storage()
      ->AsyncCall(&DIPSStorage::ReadPopup)
      .WithArgs(GetSiteForDIPS(opener_url), GetSiteForDIPS(popup_url_3))
      .Then(std::move(assert_popup));
  GetDipsService()->storage()->FlushPostedTasksForTesting();

  // We host the "image" on an HTTPS server, because for it to write a
  // cookie, the cookie needs to be SameSite=None and Secure.
  net::EmbeddedTestServer https_server(net::EmbeddedTestServer::TYPE_HTTPS);
  https_server.SetSSLConfig(net::EmbeddedTestServer::CERT_TEST_NAMES);
  https_server.AddDefaultHandlers(
      base::FilePath(FILE_PATH_LITERAL("chrome/test/data")));
  ASSERT_TRUE(https_server.Start());

  // Add a cookie access by popup_url on opener_url.
  ASSERT_TRUE(NavigateToSetCookie(GetActiveWebContents(), &https_server,
                                  "sub.b.test",
                                  /*is_secure_cookie_set=*/true));
  ASSERT_TRUE(content::NavigateToURL(GetActiveWebContents(), opener_url));
  CreateImageAndWaitForCookieAccess(
      GetActiveWebContents(),
      https_server.GetURL("sub.b.test", "/favicon/icon.png"));
  GetDipsService()->storage()->FlushPostedTasksForTesting();

  // Assert that the UKM event for the PostPopupCookieAccess was recorded.
  auto access_entries = ukm_recorder.GetEntries(
      "OpenerHeuristic.PostPopupCookieAccess",
      {"AccessId", "AccessSucceeded", "HoursSincePopupOpened"});
  ASSERT_EQ(access_entries.size(), 1u);
  EXPECT_EQ(
      ukm_recorder.GetSourceForSourceId(access_entries[0].source_id)->url(),
      opener_url);
  EXPECT_EQ(access_entries[0].metrics["AccessId"], access_id);
  EXPECT_EQ(access_entries[0].metrics["AccessSucceeded"], true);
  EXPECT_EQ(access_entries[0].metrics["HoursSincePopupOpened"], 0);
}

IN_PROC_BROWSER_TEST_F(OpenerHeuristicBrowserTest,
                       TopLevelIsReported_PastInteraction_NoSameSiteIframe) {
  ukm::TestAutoSetUkmRecorder ukm_recorder;
  GURL toplevel_url = embedded_test_server()->GetURL("a.test", "/title1.html");
  GURL popup_url = embedded_test_server()->GetURL("b.test", "/title1.html");
  WebContents* web_contents = GetActiveWebContents();

  RecordInteraction(GURL("https://b.test"), clock_.Now() - base::Hours(3));

  ASSERT_TRUE(content::NavigateToURL(web_contents, toplevel_url));
  ASSERT_TRUE(OpenPopup(popup_url).has_value());

  auto entries = ukm_recorder.GetEntries("OpenerHeuristic.TopLevel",
                                         {"HasSameSiteIframe"});
  ASSERT_EQ(entries.size(), 1u);
  EXPECT_EQ(ukm::GetSourceIdType(entries[0].source_id),
            ukm::SourceIdType::NAVIGATION_ID);
  EXPECT_EQ(ukm_recorder.GetSourceForSourceId(entries[0].source_id)->url(),
            toplevel_url);
  EXPECT_EQ(entries[0].metrics["HasSameSiteIframe"],
            static_cast<int32_t>(OptionalBool::kFalse));

  auto opener_has_iframe = GetOpenerHasSameSiteIframe(
      ukm_recorder, "OpenerHeuristic.PopupPastInteraction");
  ASSERT_TRUE(opener_has_iframe.has_value()) << opener_has_iframe.error();
  EXPECT_EQ(opener_has_iframe.value(), OptionalBool::kFalse);
}

IN_PROC_BROWSER_TEST_F(OpenerHeuristicBrowserTest,
                       TopLevelIsReported_NewInteraction_NoSameSiteIframe) {
  ukm::TestAutoSetUkmRecorder ukm_recorder;
  GURL toplevel_url = embedded_test_server()->GetURL("a.test", "/title1.html");
  GURL popup_url = embedded_test_server()->GetURL("b.test", "/title1.html");
  WebContents* web_contents = GetActiveWebContents();

  ASSERT_TRUE(content::NavigateToURL(web_contents, toplevel_url));

  auto maybe_popup = OpenPopup(popup_url);
  ASSERT_TRUE(maybe_popup.has_value()) << maybe_popup.error();

  ASSERT_EQ(ukm_recorder.GetEntriesByName("OpenerHeuristic.TopLevel").size(),
            0u);

  SimulateMouseClick(*maybe_popup);

  auto entries = ukm_recorder.GetEntries("OpenerHeuristic.TopLevel",
                                         {"HasSameSiteIframe"});
  ASSERT_EQ(entries.size(), 1u);
  EXPECT_EQ(ukm::GetSourceIdType(entries[0].source_id),
            ukm::SourceIdType::NAVIGATION_ID);
  EXPECT_EQ(ukm_recorder.GetSourceForSourceId(entries[0].source_id)->url(),
            toplevel_url);
  EXPECT_EQ(entries[0].metrics["HasSameSiteIframe"],
            static_cast<int32_t>(OptionalBool::kFalse));

  auto opener_has_iframe = GetOpenerHasSameSiteIframe(
      ukm_recorder, "OpenerHeuristic.PopupInteraction");
  ASSERT_TRUE(opener_has_iframe.has_value()) << opener_has_iframe.error();
  EXPECT_EQ(opener_has_iframe.value(), OptionalBool::kFalse);
}

IN_PROC_BROWSER_TEST_F(OpenerHeuristicBrowserTest,
                       TopLevelIsReported_HasSameSiteIframe) {
  ukm::TestAutoSetUkmRecorder ukm_recorder;
  GURL toplevel_url =
      embedded_test_server()->GetURL("a.test", "/iframe_blank.html");
  GURL iframe_url =
      embedded_test_server()->GetURL("sub.b.test", "/title1.html");
  GURL popup_url = embedded_test_server()->GetURL("b.test", "/title1.html");
  const std::string iframe_id = "test";
  WebContents* web_contents = GetActiveWebContents();

  RecordInteraction(GURL("https://b.test"), clock_.Now() - base::Hours(3));

  ASSERT_TRUE(content::NavigateToURL(web_contents, toplevel_url));
  ASSERT_TRUE(content::NavigateIframeToURL(GetActiveWebContents(), iframe_id,
                                           iframe_url));
  ASSERT_TRUE(OpenPopup(popup_url).has_value());

  auto entries = ukm_recorder.GetEntries("OpenerHeuristic.TopLevel",
                                         {"HasSameSiteIframe"});
  ASSERT_EQ(entries.size(), 1u);
  EXPECT_EQ(ukm::GetSourceIdType(entries[0].source_id),
            ukm::SourceIdType::NAVIGATION_ID);
  EXPECT_EQ(ukm_recorder.GetSourceForSourceId(entries[0].source_id)->url(),
            toplevel_url);
  EXPECT_EQ(entries[0].metrics["HasSameSiteIframe"],
            static_cast<int32_t>(OptionalBool::kTrue));

  auto opener_has_iframe = GetOpenerHasSameSiteIframe(
      ukm_recorder, "OpenerHeuristic.PopupPastInteraction");
  ASSERT_TRUE(opener_has_iframe.has_value()) << opener_has_iframe.error();
  EXPECT_EQ(opener_has_iframe.value(), OptionalBool::kTrue);
}

IN_PROC_BROWSER_TEST_F(
    OpenerHeuristicBrowserTest,
    TopLevelIsReported_UnknownSameSiteIframe_OpenerWasClosed) {
  ukm::TestAutoSetUkmRecorder ukm_recorder;
  GURL toplevel_url = embedded_test_server()->GetURL("a.test", "/title1.html");
  GURL popup_url = embedded_test_server()->GetURL("b.test", "/title1.html");
  WebContents* web_contents = GetActiveWebContents();

  ASSERT_TRUE(content::NavigateToURL(web_contents, toplevel_url));

  auto maybe_popup = OpenPopup(popup_url);
  ASSERT_TRUE(maybe_popup.has_value()) << maybe_popup.error();

  DestroyWebContents(web_contents);

  ASSERT_EQ(ukm_recorder.GetEntriesByName("OpenerHeuristic.TopLevel").size(),
            0u);

  SimulateMouseClick(*maybe_popup);

  auto entries = ukm_recorder.GetEntries("OpenerHeuristic.TopLevel",
                                         {"HasSameSiteIframe"});
  ASSERT_EQ(entries.size(), 1u);
  EXPECT_EQ(ukm::GetSourceIdType(entries[0].source_id),
            ukm::SourceIdType::NAVIGATION_ID);
  EXPECT_EQ(ukm_recorder.GetSourceForSourceId(entries[0].source_id)->url(),
            toplevel_url);
  EXPECT_EQ(entries[0].metrics["HasSameSiteIframe"],
            static_cast<int32_t>(OptionalBool::kUnknown));

  auto opener_has_iframe = GetOpenerHasSameSiteIframe(
      ukm_recorder, "OpenerHeuristic.PopupInteraction");
  ASSERT_TRUE(opener_has_iframe.has_value()) << opener_has_iframe.error();
  EXPECT_EQ(opener_has_iframe.value(), OptionalBool::kUnknown);
}

IN_PROC_BROWSER_TEST_F(
    OpenerHeuristicBrowserTest,
    TopLevelIsNotReported_UnknownSameSiteIframe_OpenerNavigatedAway) {
  ukm::TestAutoSetUkmRecorder ukm_recorder;
  GURL toplevel_url = embedded_test_server()->GetURL("a.test", "/title1.html");
  GURL other_url =
      embedded_test_server()->GetURL("a.test", "/title1.html?other");
  GURL popup_url = embedded_test_server()->GetURL("b.test", "/title1.html");
  WebContents* web_contents = GetActiveWebContents();

  ASSERT_TRUE(content::NavigateToURL(web_contents, toplevel_url));

  auto maybe_popup = OpenPopup(popup_url);
  ASSERT_TRUE(maybe_popup.has_value()) << maybe_popup.error();

  ASSERT_TRUE(content::NavigateToURL(web_contents, other_url));

  ASSERT_EQ(ukm_recorder.GetEntriesByName("OpenerHeuristic.TopLevel").size(),
            0u);

  SimulateMouseClick(*maybe_popup);

  auto entries = ukm_recorder.GetEntries("OpenerHeuristic.TopLevel",
                                         {"HasSameSiteIframe"});
  ASSERT_EQ(entries.size(), 1u);
  EXPECT_EQ(ukm::GetSourceIdType(entries[0].source_id),
            ukm::SourceIdType::NAVIGATION_ID);
  EXPECT_EQ(ukm_recorder.GetSourceForSourceId(entries[0].source_id)->url(),
            toplevel_url);
  EXPECT_EQ(entries[0].metrics["HasSameSiteIframe"],
            static_cast<int32_t>(OptionalBool::kUnknown));

  auto opener_has_iframe = GetOpenerHasSameSiteIframe(
      ukm_recorder, "OpenerHeuristic.PopupInteraction");
  ASSERT_TRUE(opener_has_iframe.has_value()) << opener_has_iframe.error();
  EXPECT_EQ(opener_has_iframe.value(), OptionalBool::kUnknown);
}

IN_PROC_BROWSER_TEST_F(OpenerHeuristicBrowserTest, TopLevel_PopupProvider) {
  ukm::TestAutoSetUkmRecorder ukm_recorder;
  GURL toplevel_url = embedded_test_server()->GetURL("a.test", "/title1.html");
  GURL popup_url = embedded_test_server()->GetURL("google.com", "/title1.html");
  WebContents* web_contents = GetActiveWebContents();

  RecordInteraction(GURL("https://google.com"), clock_.Now() - base::Hours(3));

  ASSERT_TRUE(content::NavigateToURL(web_contents, toplevel_url));
  ASSERT_TRUE(OpenPopup(popup_url).has_value());

  auto entries =
      ukm_recorder.GetEntries("OpenerHeuristic.TopLevel", {"PopupProvider"});
  ASSERT_EQ(entries.size(), 1u);
  EXPECT_EQ(ukm_recorder.GetSourceForSourceId(entries[0].source_id)->url(),
            toplevel_url);
  EXPECT_EQ(entries[0].metrics["PopupProvider"],
            static_cast<int64_t>(PopupProvider::kGoogle));
}

IN_PROC_BROWSER_TEST_F(OpenerHeuristicBrowserTest, TopLevel_PopupId) {
  ukm::TestAutoSetUkmRecorder ukm_recorder;
  GURL toplevel_url = embedded_test_server()->GetURL("a.test", "/title1.html");
  GURL popup_url = embedded_test_server()->GetURL("google.com", "/title1.html");
  WebContents* web_contents = GetActiveWebContents();

  RecordInteraction(GURL("https://google.com"), clock_.Now() - base::Hours(3));

  ASSERT_TRUE(content::NavigateToURL(web_contents, toplevel_url));
  auto maybe_popup = OpenPopup(popup_url);
  ASSERT_TRUE(maybe_popup.has_value()) << maybe_popup.error();

  SimulateMouseClick(*maybe_popup);

  // Verify all three events share the same popup id.
  auto tl_entries =
      ukm_recorder.GetEntries("OpenerHeuristic.TopLevel", {"PopupId"});
  ASSERT_EQ(tl_entries.size(), 1u);
  EXPECT_EQ(ukm_recorder.GetSourceForSourceId(tl_entries[0].source_id)->url(),
            toplevel_url);
  const int64_t popup_id = tl_entries[0].metrics["PopupId"];
  EXPECT_NE(popup_id, 0);

  auto pi_entries =
      ukm_recorder.GetEntries("OpenerHeuristic.PopupInteraction", {"PopupId"});
  ASSERT_EQ(pi_entries.size(), 1u);
  EXPECT_EQ(ukm_recorder.GetSourceForSourceId(pi_entries[0].source_id)->url(),
            popup_url);
  EXPECT_EQ(pi_entries[0].metrics["PopupId"], popup_id);

  auto ppi_entries = ukm_recorder.GetEntries(
      "OpenerHeuristic.PopupPastInteraction", {"PopupId"});
  ASSERT_EQ(ppi_entries.size(), 1u);
  EXPECT_EQ(ukm_recorder.GetSourceForSourceId(ppi_entries[0].source_id)->url(),
            popup_url);
  EXPECT_EQ(ppi_entries[0].metrics["PopupId"], popup_id);

  // Open second popup, verify different popup id.
  ASSERT_TRUE(OpenPopup(popup_url).has_value());
  tl_entries = ukm_recorder.GetEntries("OpenerHeuristic.TopLevel", {"PopupId"});
  ASSERT_EQ(tl_entries.size(), 2u);
  const int64_t popup_id2 = tl_entries[1].metrics["PopupId"];
  EXPECT_NE(popup_id2, 0);
  EXPECT_NE(popup_id, popup_id2);
}
