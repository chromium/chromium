// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/dips/dips_helper.h"

#include "base/memory/raw_ptr.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/simple_test_clock.h"
#include "base/time/time.h"
#include "chrome/browser/dips/dips_service.h"
#include "chrome/browser/dips/dips_service_factory.h"
#include "chrome/browser/dips/dips_storage.h"
#include "chrome/browser/dips/dips_utils.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/hit_test_region_observer.h"
#include "net/dns/mock_host_resolver.h"
#include "third_party/blink/public/common/switches.h"

namespace {

class UserActivationObserver : public content::WebContentsObserver {
 public:
  explicit UserActivationObserver(content::WebContents* web_contents,
                                  content::RenderFrameHost* render_frame_host)
      : WebContentsObserver(web_contents),
        render_frame_host_(render_frame_host) {}

  // Wait until the frame receives user activation.
  void Wait() { run_loop_.Run(); }

  // WebContentsObserver override
  void FrameReceivedUserActivation(
      content::RenderFrameHost* render_frame_host) override {
    if (render_frame_host_ == render_frame_host) {
      run_loop_.Quit();
    }
  }

 private:
  raw_ptr<content::RenderFrameHost> const render_frame_host_;
  base::RunLoop run_loop_;
};

class CookieAccessObserver : public content::WebContentsObserver {
 public:
  explicit CookieAccessObserver(content::WebContents* web_contents,
                                content::RenderFrameHost* render_frame_host)
      : WebContentsObserver(web_contents),
        render_frame_host_(render_frame_host) {}

  // Wait until the frame accesses cookies.
  void Wait() { run_loop_.Run(); }

  // WebContentsObserver override
  void OnCookiesAccessed(content::RenderFrameHost* render_frame_host,
                         const content::CookieAccessDetails& details) override {
    if (render_frame_host_ == render_frame_host) {
      run_loop_.Quit();
    }
  }

 private:
  const raw_ptr<content::RenderFrameHost> render_frame_host_;
  base::RunLoop run_loop_;
};

using StateForURLCallback = base::OnceCallback<void(const DIPSState&)>;

// Histogram names
constexpr char kTimeToInteraction[] =
    "Privacy.DIPS.TimeFromStorageToInteraction.Standard";
constexpr char kTimeToStorage[] =
    "Privacy.DIPS.TimeFromInteractionToStorage.Standard";
constexpr char kTimeToInteraction_OTR_Block3PC[] =
    "Privacy.DIPS.TimeFromStorageToInteraction.OffTheRecord_Block3PC";

}  // namespace

class DIPSTabHelperBrowserTest : public InProcessBrowserTest {
 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    // Prevents flakiness by handling clicks even before content is drawn.
    command_line->AppendSwitch(blink::switches::kAllowPreCommitInput);
  }

  void SetUpInProcessBrowserTestFixture() override {
    DIPSTabHelper::SetClockForTesting(&test_clock_);
  }

  void SetUpOnMainThread() override {
    ASSERT_TRUE(embedded_test_server()->Start());
    host_resolver()->AddRule("a.test", "127.0.0.1");
    host_resolver()->AddRule("b.test", "127.0.0.1");
  }

  void TearDownInProcessBrowserTestFixture() override {
    DIPSTabHelper::SetClockForTesting(nullptr);
  }

  content::WebContents* GetActiveWebContents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  void BlockUntilHelperProcessesPendingRequests() {
    base::SequenceBound<DIPSStorage>* storage =
        DIPSServiceFactory::GetForBrowserContext(
            GetActiveWebContents()->GetBrowserContext())
            ->storage();
    storage->FlushPostedTasksForTesting();
  }

  void SetDIPSTime(base::Time time) { test_clock_.SetNow(time); }

  void StateForURL(const GURL& url, StateForURLCallback callback) {
    DIPSService* dips_service = DIPSServiceFactory::GetForBrowserContext(
        GetActiveWebContents()->GetBrowserContext());
    dips_service->storage()
        ->AsyncCall(&DIPSStorage::Read)
        .WithArgs(url)
        .Then(std::move(callback));
  }

  absl::optional<StateValue> GetDIPSState(const GURL& url) {
    absl::optional<StateValue> state;

    StateForURL(url,
                base::BindLambdaForTesting([&](const DIPSState& loaded_state) {
                  if (loaded_state.was_loaded())
                    state = loaded_state.ToStateValue();
                }));
    BlockUntilHelperProcessesPendingRequests();

    return state;
  }

 private:
  base::SimpleTestClock test_clock_;
};

IN_PROC_BROWSER_TEST_F(DIPSTabHelperBrowserTest,
                       InteractionsRecordedInAncestorFrames) {
  GURL url_a = embedded_test_server()->GetURL("a.test", "/iframe_blank.html");
  GURL url_b = embedded_test_server()->GetURL("b.test", "/title1.html");
  const std::string kIframeId = "test";  // defined in iframe_blank.html
  base::Time time = base::Time::FromDoubleT(1);
  content::WebContents* web_contents = GetActiveWebContents();

  // The top-level page is on a.test.
  ASSERT_TRUE(content::NavigateToURL(web_contents, url_a));

  // Before clicking, no DIPS state for either site.
  EXPECT_FALSE(GetDIPSState(url_a).has_value());
  EXPECT_FALSE(GetDIPSState(url_b).has_value());

  // Click on the a.test top-level site.
  SetDIPSTime(time);
  UserActivationObserver observer_a(web_contents,
                                    web_contents->GetPrimaryMainFrame());
  SimulateMouseClick(web_contents, 0, blink::WebMouseEvent::Button::kLeft);
  observer_a.Wait();

  // User interaction is recorded for a.test (the top-level frame).
  absl::optional<StateValue> state_a = GetDIPSState(url_a);
  ASSERT_TRUE(state_a.has_value());
  EXPECT_FALSE(state_a->site_storage_times.first.has_value());
  EXPECT_EQ(absl::make_optional(time), state_a->user_interaction_times.first);

  // Update the top-level page to have an iframe pointing to b.test.
  ASSERT_TRUE(content::NavigateIframeToURL(web_contents, kIframeId, url_b));
  content::RenderFrameHost* iframe = content::FrameMatchingPredicate(
      web_contents->GetPrimaryPage(),
      base::BindRepeating(&content::FrameIsChildOfMainFrame));
  // Wait until we can click on the iframe.
  content::WaitForHitTestData(iframe);

  // Click on the b.test iframe.
  base::Time frame_interaction_time = base::Time::FromDoubleT(2);
  SetDIPSTime(frame_interaction_time);
  UserActivationObserver observer_b(web_contents, iframe);
  content::SimulateMouseClickOrTapElementWithId(web_contents, kIframeId);
  observer_b.Wait();

  // User interaction on the top-level is updated by interacting with b.test
  // (the iframe).
  state_a = GetDIPSState(url_a);
  ASSERT_TRUE(state_a.has_value());
  EXPECT_FALSE(state_a->site_storage_times.first.has_value());
  EXPECT_EQ(absl::make_optional(frame_interaction_time),
            state_a->user_interaction_times.last);

  // The iframe site doesn't have any state.
  EXPECT_FALSE(GetDIPSState(url_b).has_value());
}

IN_PROC_BROWSER_TEST_F(DIPSTabHelperBrowserTest,
                       MultipleUserInteractionsRecorded) {
  GURL url = embedded_test_server()->GetURL("a.test", "/title1.html");
  base::Time time = base::Time::FromDoubleT(1);
  content::WebContents* web_contents = GetActiveWebContents();

  SetDIPSTime(time);
  // Navigate to a.test.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  content::RenderFrameHost* frame = web_contents->GetPrimaryMainFrame();
  content::WaitForHitTestData(frame);  // Wait until we can click.

  // Before clicking, there's no DIPS state for the site.
  EXPECT_FALSE(GetDIPSState(url).has_value());

  UserActivationObserver observer1(web_contents, frame);
  SimulateMouseClick(web_contents, 0, blink::WebMouseEvent::Button::kLeft);
  observer1.Wait();

  // One instance of user interaction is recorded.
  absl::optional<StateValue> state_1 = GetDIPSState(url);
  ASSERT_TRUE(state_1.has_value());
  EXPECT_FALSE(state_1->site_storage_times.first.has_value());
  EXPECT_EQ(absl::make_optional(time), state_1->user_interaction_times.first);
  EXPECT_EQ(state_1->user_interaction_times.last,
            state_1->user_interaction_times.first);

  SetDIPSTime(time + base::Seconds(10));
  UserActivationObserver observer_2(web_contents, frame);
  SimulateMouseClick(web_contents, 0, blink::WebMouseEvent::Button::kLeft);
  observer_2.Wait();

  // A second, different, instance of user interaction is recorded for the same
  // site.
  absl::optional<StateValue> state_2 = GetDIPSState(url);
  ASSERT_TRUE(state_2.has_value());
  EXPECT_FALSE(state_2->site_storage_times.first.has_value());
  EXPECT_NE(state_2->user_interaction_times.last,
            state_2->user_interaction_times.first);
  EXPECT_EQ(absl::make_optional(time), state_2->user_interaction_times.first);
  EXPECT_EQ(absl::make_optional(time + base::Seconds(10)),
            state_2->user_interaction_times.last);
}

IN_PROC_BROWSER_TEST_F(DIPSTabHelperBrowserTest, StorageRecordedInSingleFrame) {
  // We host the iframe content on an HTTPS server, because for it to write a
  // cookie, the cookie needs to be SameSite=None and Secure.
  net::EmbeddedTestServer https_server(net::EmbeddedTestServer::TYPE_HTTPS);
  https_server.SetSSLConfig(net::EmbeddedTestServer::CERT_TEST_NAMES);
  https_server.AddDefaultHandlers(GetChromeTestDataDir());
  ASSERT_TRUE(https_server.Start());

  GURL url_a = embedded_test_server()->GetURL("a.test", "/iframe_blank.html");
  GURL url_b = https_server.GetURL("b.test", "/title1.html");
  const std::string kIframeId = "test";  // defined in iframe_blank.html
  base::Time time = base::Time::FromDoubleT(1);
  content::WebContents* web_contents = GetActiveWebContents();

  // The top-level page is on a.test, containing an iframe pointing at b.test.
  ASSERT_TRUE(content::NavigateToURL(web_contents, url_a));
  ASSERT_TRUE(content::NavigateIframeToURL(web_contents, kIframeId, url_b));

  content::RenderFrameHost* iframe = content::FrameMatchingPredicate(
      web_contents->GetPrimaryPage(),
      base::BindRepeating(&content::FrameIsChildOfMainFrame));

  // Initially, no DIPS state for either site.
  EXPECT_FALSE(GetDIPSState(url_a).has_value());
  EXPECT_FALSE(GetDIPSState(url_b).has_value());

  // Write a cookie in the b.test iframe.
  SetDIPSTime(time);
  CookieAccessObserver observer(web_contents, iframe);
  ASSERT_TRUE(content::ExecJs(
      iframe, "document.cookie = 'foo=bar; SameSite=None; Secure';",
      content::EXECUTE_SCRIPT_NO_USER_GESTURE));
  observer.Wait();

  // Nothing recorded for a.test (the top-level frame).
  absl::optional<StateValue> state_a = GetDIPSState(url_a);
  EXPECT_FALSE(state_a.has_value());
  // Nothing recorded for b.test (the iframe), since we don't record non main
  // frame URLs to DIPS State.
  absl::optional<StateValue> state_b = GetDIPSState(url_b);
  EXPECT_FALSE(state_b.has_value());
}

IN_PROC_BROWSER_TEST_F(DIPSTabHelperBrowserTest, MultipleSiteStoragesRecorded) {
  GURL url = embedded_test_server()->GetURL("a.test", "/set-cookie?foo=bar");
  base::Time time = base::Time::FromDoubleT(1);

  SetDIPSTime(time);
  // Navigating to this URL sets a cookie.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  // One instance of site storage is recorded.
  absl::optional<StateValue> state_1 = GetDIPSState(url);
  ASSERT_TRUE(state_1.has_value());
  EXPECT_FALSE(state_1->user_interaction_times.first.has_value());
  EXPECT_EQ(absl::make_optional(time), state_1->site_storage_times.first);
  EXPECT_EQ(state_1->site_storage_times.last,
            state_1->site_storage_times.first);

  SetDIPSTime(time + base::Seconds(10));
  // Navigate to the URL again to rewrite the cookie.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  // A second, different, instance of site storage is recorded for the same
  // site.
  absl::optional<StateValue> state_2 = GetDIPSState(url);
  ASSERT_TRUE(state_2.has_value());
  EXPECT_FALSE(state_2->user_interaction_times.first.has_value());
  EXPECT_NE(state_2->site_storage_times.last,
            state_2->site_storage_times.first);
  EXPECT_EQ(absl::make_optional(time), state_2->site_storage_times.first);
  EXPECT_EQ(absl::make_optional(time + base::Seconds(10)),
            state_2->site_storage_times.last);
}

IN_PROC_BROWSER_TEST_F(DIPSTabHelperBrowserTest, Histograms_StorageThenClick) {
  base::HistogramTester histograms;
  GURL url = embedded_test_server()->GetURL("a.test", "/set-cookie?foo=bar");
  base::Time time = base::Time::FromDoubleT(1);
  content::WebContents* web_contents = GetActiveWebContents();

  SetDIPSTime(time);
  // Navigating to this URL sets a cookie.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  // Wait until we can click.
  content::WaitForHitTestData(web_contents->GetPrimaryMainFrame());
  BlockUntilHelperProcessesPendingRequests();

  histograms.ExpectTotalCount(kTimeToInteraction, 0);
  histograms.ExpectTotalCount(kTimeToStorage, 0);

  SetDIPSTime(time + base::Seconds(10));
  UserActivationObserver observer(web_contents,
                                  web_contents->GetPrimaryMainFrame());
  SimulateMouseClick(web_contents, 0, blink::WebMouseEvent::Button::kLeft);
  observer.Wait();
  BlockUntilHelperProcessesPendingRequests();

  histograms.ExpectTotalCount(kTimeToInteraction, 1);
  histograms.ExpectTotalCount(kTimeToStorage, 0);
  histograms.ExpectUniqueTimeSample(kTimeToInteraction, base::Seconds(10), 1);
}

IN_PROC_BROWSER_TEST_F(DIPSTabHelperBrowserTest,
                       Histograms_StorageThenClick_Incognito) {
  base::HistogramTester histograms;
  GURL url = embedded_test_server()->GetURL("a.test", "/set-cookie?foo=bar");
  base::Time time = base::Time::FromDoubleT(1);
  Browser* browser = CreateIncognitoBrowser();
  content::WebContents* web_contents =
      browser->tab_strip_model()->GetActiveWebContents();

  SetDIPSTime(time);
  // Navigating to this URL sets a cookie.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser, url));
  // Wait until we can click.
  content::WaitForHitTestData(web_contents->GetPrimaryMainFrame());
  BlockUntilHelperProcessesPendingRequests();

  histograms.ExpectTotalCount(kTimeToInteraction, 0);
  histograms.ExpectTotalCount(kTimeToInteraction_OTR_Block3PC, 0);
  histograms.ExpectTotalCount(kTimeToStorage, 0);

  SetDIPSTime(time + base::Seconds(10));
  UserActivationObserver observer(web_contents,
                                  web_contents->GetPrimaryMainFrame());
  SimulateMouseClick(web_contents, 0, blink::WebMouseEvent::Button::kLeft);
  observer.Wait();
  BlockUntilHelperProcessesPendingRequests();

  histograms.ExpectTotalCount(kTimeToInteraction, 0);
  // Incognito Mode defaults to blocking third-party cookies.
  histograms.ExpectTotalCount(kTimeToInteraction_OTR_Block3PC, 1);
  histograms.ExpectTotalCount(kTimeToStorage, 0);
  histograms.ExpectUniqueTimeSample(kTimeToInteraction_OTR_Block3PC,
                                    base::Seconds(10), 1);
}

IN_PROC_BROWSER_TEST_F(DIPSTabHelperBrowserTest, Histograms_ClickThenStorage) {
  base::HistogramTester histograms;
  base::Time time = base::Time::FromDoubleT(1);
  content::WebContents* web_contents = GetActiveWebContents();

  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("a.test", "/title1.html")));
  content::RenderFrameHost* frame = web_contents->GetPrimaryMainFrame();
  content::WaitForHitTestData(frame);  // wait until we can click.
  SetDIPSTime(time);
  UserActivationObserver click_observer(web_contents, frame);
  SimulateMouseClick(web_contents, 0, blink::WebMouseEvent::Button::kLeft);
  click_observer.Wait();
  BlockUntilHelperProcessesPendingRequests();

  histograms.ExpectTotalCount(kTimeToInteraction, 0);
  histograms.ExpectTotalCount(kTimeToStorage, 0);

  // Write a cookie now that the click has been handled.
  SetDIPSTime(time + base::Seconds(10));
  CookieAccessObserver cookie_observer(web_contents, frame);
  ASSERT_TRUE(content::ExecJs(frame, "document.cookie = 'foo=bar';",
                              content::EXECUTE_SCRIPT_NO_USER_GESTURE));
  cookie_observer.Wait();
  BlockUntilHelperProcessesPendingRequests();

  histograms.ExpectTotalCount(kTimeToInteraction, 0);
  histograms.ExpectTotalCount(kTimeToStorage, 1);
  histograms.ExpectUniqueTimeSample(kTimeToStorage, base::Seconds(10), 1);
}

IN_PROC_BROWSER_TEST_F(DIPSTabHelperBrowserTest,
                       Histograms_MultipleStoragesThenClick) {
  base::HistogramTester histograms;
  GURL url = embedded_test_server()->GetURL("a.test", "/set-cookie?foo=bar");
  base::Time time = base::Time::FromDoubleT(1);
  content::WebContents* web_contents = GetActiveWebContents();

  SetDIPSTime(time);
  // Navigating to this URL sets a cookie.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  BlockUntilHelperProcessesPendingRequests();

  // Navigate to the URL, setting the cookie again.
  SetDIPSTime(time + base::Seconds(3));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  content::RenderFrameHost* frame = web_contents->GetPrimaryMainFrame();
  // Wait until we can click.
  content::WaitForHitTestData(frame);
  BlockUntilHelperProcessesPendingRequests();

  // Verify both cookie writes were recorded.
  absl::optional<StateValue> state = GetDIPSState(url);
  ASSERT_TRUE(state.has_value());
  EXPECT_NE(state->site_storage_times.first, state->site_storage_times.last);
  EXPECT_EQ(absl::make_optional(time), state->site_storage_times.first);
  EXPECT_EQ(absl::make_optional(time + base::Seconds(3)),
            state->site_storage_times.last);
  EXPECT_FALSE(state->user_interaction_times.first.has_value());

  histograms.ExpectTotalCount(kTimeToInteraction, 0);
  histograms.ExpectTotalCount(kTimeToStorage, 0);

  SetDIPSTime(time + base::Seconds(10));
  UserActivationObserver observer(web_contents, frame);
  SimulateMouseClick(web_contents, 0, blink::WebMouseEvent::Button::kLeft);
  observer.Wait();
  BlockUntilHelperProcessesPendingRequests();

  histograms.ExpectTotalCount(kTimeToInteraction, 1);
  histograms.ExpectTotalCount(kTimeToStorage, 0);
  // Unlike for TimeToStorage metrics, we want to know the time from the
  // first site storage, not the most recent, so the reported time delta
  // should be 10 seconds (not 7).
  histograms.ExpectUniqueTimeSample(kTimeToInteraction, base::Seconds(10), 1);
}

IN_PROC_BROWSER_TEST_F(DIPSTabHelperBrowserTest,
                       Histograms_MultipleClicksThenStorage) {
  base::HistogramTester histograms;
  GURL url = embedded_test_server()->GetURL("a.test", "/title1.html");
  base::Time time = base::Time::FromDoubleT(1);
  content::WebContents* web_contents = GetActiveWebContents();

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  content::RenderFrameHost* frame = web_contents->GetPrimaryMainFrame();
  content::WaitForHitTestData(frame);  // Wait until we can click.

  // Click once.
  SetDIPSTime(time);
  UserActivationObserver click_observer1(web_contents, frame);
  SimulateMouseClick(web_contents, 0, blink::WebMouseEvent::Button::kLeft);
  click_observer1.Wait();
  BlockUntilHelperProcessesPendingRequests();

  // Click a second time.
  SetDIPSTime(time + base::Seconds(3));
  UserActivationObserver click_observer_2(web_contents, frame);
  SimulateMouseClick(web_contents, 0, blink::WebMouseEvent::Button::kLeft);
  click_observer_2.Wait();
  BlockUntilHelperProcessesPendingRequests();

  // Verify both clicks were recorded.
  absl::optional<StateValue> state = GetDIPSState(url);
  ASSERT_TRUE(state.has_value());
  EXPECT_NE(state->user_interaction_times.first,
            state->user_interaction_times.last);
  EXPECT_EQ(absl::make_optional(time), state->user_interaction_times.first);
  EXPECT_EQ(absl::make_optional(time + base::Seconds(3)),
            state->user_interaction_times.last);
  EXPECT_FALSE(state->site_storage_times.first.has_value());

  histograms.ExpectTotalCount(kTimeToInteraction, 0);
  histograms.ExpectTotalCount(kTimeToStorage, 0);

  // Write a cookie now that both clicks have been handled.
  SetDIPSTime(time + base::Seconds(10));
  CookieAccessObserver cookie_observer(web_contents, frame);
  ASSERT_TRUE(content::ExecJs(frame, "document.cookie = 'foo=bar';",
                              content::EXECUTE_SCRIPT_NO_USER_GESTURE));
  cookie_observer.Wait();
  BlockUntilHelperProcessesPendingRequests();

  histograms.ExpectTotalCount(kTimeToInteraction, 0);
  histograms.ExpectTotalCount(kTimeToStorage, 1);
  // Unlike for TimeToInteraction metrics, we want to know the time from the
  // most recent user interaction, not the first, so the reported time delta
  // should be 7 seconds (not 10).
  histograms.ExpectUniqueTimeSample(kTimeToStorage, base::Seconds(7), 1);
}

IN_PROC_BROWSER_TEST_F(DIPSTabHelperBrowserTest, PRE_PrepopulateTest) {
  // Simulate the user typing the URL to visit the page, which will record site
  // engagement.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("a.test", "/title1.html")));
}

IN_PROC_BROWSER_TEST_F(DIPSTabHelperBrowserTest, PrepopulateTest) {
  // Since there was previous site engagement, the DIPS DB should be
  // prepopulated with a user interaction timestamp.
  auto state = GetDIPSState(GURL("http://a.test"));
  ASSERT_TRUE(state.has_value());
  EXPECT_TRUE(state->user_interaction_times.first.has_value());
}
