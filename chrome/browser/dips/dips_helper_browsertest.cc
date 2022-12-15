// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/dips/dips_bounce_detector.h"

#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/simple_test_clock.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/browsing_data/browsing_data_important_sites_util.h"
#include "chrome/browser/browsing_data/chrome_browsing_data_remover_constants.h"
#include "chrome/browser/dips/dips_features.h"
#include "chrome/browser/dips/dips_service.h"
#include "chrome/browser/dips/dips_service_factory.h"
#include "chrome/browser/dips/dips_storage.h"
#include "chrome/browser/dips/dips_utils.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/hit_test_region_observer.h"
#include "net/dns/mock_host_resolver.h"
#include "third_party/blink/public/common/switches.h"

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/ui_test_utils.h"
#endif  // !BUILDFLAG(IS_ANDROID)

using content::CookieAccessDetails;
using content::NavigationHandle;
using content::RenderFrameHost;
using content::WebContents;

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

class FrameCookieAccessObserver : public content::WebContentsObserver {
 public:
  explicit FrameCookieAccessObserver(WebContents* web_contents,
                                     RenderFrameHost* render_frame_host)
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

class URLCookieAccessObserver : public content::WebContentsObserver {
 public:
  explicit URLCookieAccessObserver(WebContents* web_contents,
                                   const GURL& url,
                                   CookieAccessDetails::Type access_type)
      : WebContentsObserver(web_contents),
        url_(url),
        access_type_(access_type) {}

  // Wait until the frame accesses cookies.
  void Wait() { run_loop_.Run(); }

  // WebContentsObserver overrides
  void OnCookiesAccessed(RenderFrameHost* render_frame_host,
                         const CookieAccessDetails& details) override {
    if (details.type == access_type_ && details.url == url_) {
      run_loop_.Quit();
    }
  }
  void OnCookiesAccessed(NavigationHandle* navigation_handle,
                         const CookieAccessDetails& details) override {
    if (details.type == access_type_ && details.url == url_) {
      run_loop_.Quit();
    }
  }

 private:
  GURL url_;
  CookieAccessDetails::Type access_type_;
  base::RunLoop run_loop_;
};

using StateForURLCallback = base::OnceCallback<void(const DIPSState&)>;

// Histogram names
constexpr char kTimeToInteraction[] =
    "Privacy.DIPS.TimeFromStorageToInteraction.Standard";
constexpr char kTimeToStorage[] =
    "Privacy.DIPS.TimeFromInteractionToStorage.Standard";
#if !BUILDFLAG(IS_ANDROID)
constexpr char kTimeToInteraction_OTR_Block3PC[] =
    "Privacy.DIPS.TimeFromStorageToInteraction.OffTheRecord_Block3PC";
#endif
}  // namespace

class DIPSTabHelperBrowserTest : public PlatformBrowserTest,
                                 public testing::WithParamInterface<bool> {
 protected:
  void SetUp() override {
    if (IsPersistentStorageEnabled()) {
      scoped_feature_list_.InitAndEnableFeatureWithParameters(
          dips::kFeature, {{"persist_database", "true"}});
    }
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
    DIPSWebContentsObserver::FromWebContents(GetActiveWebContents())
        ->SetClockForTesting(&test_clock_);
  }

  WebContents* GetActiveWebContents() {
    if (!web_contents_) {
      web_contents_ = chrome_test_utils::GetActiveWebContents(this);
    }
    return web_contents_;
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

  [[nodiscard]] bool NavigateToURLAndWaitForCookieWrite(const GURL& url) {
    URLCookieAccessObserver observer(GetActiveWebContents(), url,
                                     CookieAccessDetails::Type::kChange);
    bool success = content::NavigateToURL(GetActiveWebContents(), url);
    if (!success) {
      return false;
    }
    observer.Wait();
    return true;
  }

  bool IsPersistentStorageEnabled() { return GetParam(); }
  base::Clock* test_clock() { return &test_clock_; }

  // Make GetActiveWebContents() return the given value instead of the default.
  // Helpful for tests that use other WebContents (e.g. in incognito windows).
  void OverrideActiveWebContents(WebContents* web_contents) {
    web_contents_ = web_contents;
  }

 private:
  raw_ptr<WebContents> web_contents_ = nullptr;
  base::SimpleTestClock test_clock_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

INSTANTIATE_TEST_SUITE_P(All, DIPSTabHelperBrowserTest, ::testing::Bool());

IN_PROC_BROWSER_TEST_P(DIPSTabHelperBrowserTest,
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
  base::Time frame_interaction_time =
      time + DIPSBounceDetector::kInteractionUpdateInterval;
  SetDIPSTime(frame_interaction_time);
  UserActivationObserver observer_b(web_contents, iframe);

  // TODO(crbug.com/1386142): Remove the ExecJs workaround once
  // SimulateMouseClickOrTapElementWithId is able to activate iframes on Android
#if !BUILDFLAG(IS_ANDROID)
  content::SimulateMouseClickOrTapElementWithId(web_contents, kIframeId);
#else
  ASSERT_TRUE(content::ExecJs(iframe, "// empty script to activate iframe"));
#endif
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

IN_PROC_BROWSER_TEST_P(DIPSTabHelperBrowserTest,
                       MultipleUserInteractionsRecorded) {
  GURL url = embedded_test_server()->GetURL("a.test", "/title1.html");
  base::Time time = base::Time::FromDoubleT(1);
  content::WebContents* web_contents = GetActiveWebContents();

  SetDIPSTime(time);
  // Navigate to a.test.
  ASSERT_TRUE(content::NavigateToURL(web_contents, url));
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

  SetDIPSTime(time + DIPSBounceDetector::kInteractionUpdateInterval +
              base::Seconds(10));
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
  EXPECT_EQ(absl::make_optional(time +
                                DIPSBounceDetector::kInteractionUpdateInterval +
                                base::Seconds(10)),
            state_2->user_interaction_times.last);
}

IN_PROC_BROWSER_TEST_P(DIPSTabHelperBrowserTest, StorageRecordedInSingleFrame) {
  // We host the iframe content on an HTTPS server, because for it to write a
  // cookie, the cookie needs to be SameSite=None and Secure.
  net::EmbeddedTestServer https_server(net::EmbeddedTestServer::TYPE_HTTPS);
  https_server.SetSSLConfig(net::EmbeddedTestServer::CERT_TEST_NAMES);
  https_server.AddDefaultHandlers(
      base::FilePath(FILE_PATH_LITERAL("chrome/test/data")));
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
  FrameCookieAccessObserver observer(web_contents, iframe);
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

IN_PROC_BROWSER_TEST_P(DIPSTabHelperBrowserTest, MultipleSiteStoragesRecorded) {
  GURL url = embedded_test_server()->GetURL("a.test", "/set-cookie?foo=bar");
  base::Time time = base::Time::FromDoubleT(1);

  SetDIPSTime(time);
  // Navigating to this URL sets a cookie.
  ASSERT_TRUE(NavigateToURLAndWaitForCookieWrite(url));

  // One instance of site storage is recorded.
  absl::optional<StateValue> state_1 = GetDIPSState(url);
  ASSERT_TRUE(state_1.has_value());
  EXPECT_FALSE(state_1->user_interaction_times.first.has_value());
  EXPECT_EQ(absl::make_optional(time), state_1->site_storage_times.first);
  EXPECT_EQ(state_1->site_storage_times.last,
            state_1->site_storage_times.first);

  SetDIPSTime(time + base::Seconds(10));
  // Navigate to the URL again to rewrite the cookie.
  ASSERT_TRUE(NavigateToURLAndWaitForCookieWrite(url));

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

IN_PROC_BROWSER_TEST_P(DIPSTabHelperBrowserTest, Histograms_StorageThenClick) {
  base::HistogramTester histograms;
  GURL url = embedded_test_server()->GetURL("a.test", "/set-cookie?foo=bar");
  base::Time time = base::Time::FromDoubleT(1);
  content::WebContents* web_contents = GetActiveWebContents();

  SetDIPSTime(time);
  // Navigating to this URL sets a cookie.
  ASSERT_TRUE(NavigateToURLAndWaitForCookieWrite(url));
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

IN_PROC_BROWSER_TEST_P(DIPSTabHelperBrowserTest,
                       Histograms_StorageThenClick_Incognito) {
// TODO(crbug.com/1380410): Enable this test on Android once the logic to
// create an Incognito browser without regard to platform is public.
#if !BUILDFLAG(IS_ANDROID)
  base::HistogramTester histograms;
  GURL url = embedded_test_server()->GetURL("a.test", "/set-cookie?foo=bar");
  base::Time time = base::Time::FromDoubleT(1);
  Browser* browser = CreateIncognitoBrowser();
  content::WebContents* web_contents =
      browser->tab_strip_model()->GetActiveWebContents();
  // Make our helper methods use the incognito WebContents.
  OverrideActiveWebContents(web_contents);
  DIPSWebContentsObserver::FromWebContents(web_contents)
      ->SetClockForTesting(test_clock());
  SetDIPSTime(time);
  // Navigating to this URL sets a cookie.
  ASSERT_TRUE(NavigateToURLAndWaitForCookieWrite(url));
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
#endif
}

IN_PROC_BROWSER_TEST_P(DIPSTabHelperBrowserTest, Histograms_ClickThenStorage) {
  base::HistogramTester histograms;
  base::Time time = base::Time::FromDoubleT(1);
  content::WebContents* web_contents = GetActiveWebContents();

  ASSERT_TRUE(content::NavigateToURL(
      web_contents, embedded_test_server()->GetURL("a.test", "/title1.html")));
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
  FrameCookieAccessObserver cookie_observer(web_contents, frame);
  ASSERT_TRUE(content::ExecJs(frame, "document.cookie = 'foo=bar';",
                              content::EXECUTE_SCRIPT_NO_USER_GESTURE));
  cookie_observer.Wait();
  BlockUntilHelperProcessesPendingRequests();

  histograms.ExpectTotalCount(kTimeToInteraction, 0);
  histograms.ExpectTotalCount(kTimeToStorage, 1);
  histograms.ExpectUniqueTimeSample(kTimeToStorage, base::Seconds(10), 1);
}

IN_PROC_BROWSER_TEST_P(DIPSTabHelperBrowserTest,
                       Histograms_MultipleStoragesThenClick) {
  base::HistogramTester histograms;
  GURL url = embedded_test_server()->GetURL("a.test", "/set-cookie?foo=bar");
  base::Time time = base::Time::FromDoubleT(1);
  content::WebContents* web_contents = GetActiveWebContents();

  SetDIPSTime(time);
  // Navigating to this URL sets a cookie.
  ASSERT_TRUE(NavigateToURLAndWaitForCookieWrite(url));
  BlockUntilHelperProcessesPendingRequests();

  // Navigate to the URL, setting the cookie again.
  SetDIPSTime(time + base::Seconds(3));
  ASSERT_TRUE(NavigateToURLAndWaitForCookieWrite(url));
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

IN_PROC_BROWSER_TEST_P(DIPSTabHelperBrowserTest,
                       Histograms_MultipleClicksThenStorage) {
  base::HistogramTester histograms;
  GURL url = embedded_test_server()->GetURL("a.test", "/title1.html");
  base::Time time = base::Time::FromDoubleT(1);
  content::WebContents* web_contents = GetActiveWebContents();

  ASSERT_TRUE(content::NavigateToURL(web_contents, url));
  content::RenderFrameHost* frame = web_contents->GetPrimaryMainFrame();
  content::WaitForHitTestData(frame);  // Wait until we can click.

  // Click once.
  SetDIPSTime(time);
  UserActivationObserver click_observer1(web_contents, frame);
  SimulateMouseClick(web_contents, 0, blink::WebMouseEvent::Button::kLeft);
  click_observer1.Wait();
  BlockUntilHelperProcessesPendingRequests();

  // Click a second time.
  SetDIPSTime(time + DIPSBounceDetector::kInteractionUpdateInterval +
              base::Seconds(3));
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
  EXPECT_EQ(absl::make_optional(time +
                                DIPSBounceDetector::kInteractionUpdateInterval +
                                base::Seconds(3)),
            state->user_interaction_times.last);
  EXPECT_FALSE(state->site_storage_times.first.has_value());

  histograms.ExpectTotalCount(kTimeToInteraction, 0);
  histograms.ExpectTotalCount(kTimeToStorage, 0);

  // Write a cookie now that both clicks have been handled.
  SetDIPSTime(time + DIPSBounceDetector::kInteractionUpdateInterval +
              base::Seconds(10));
  FrameCookieAccessObserver cookie_observer(web_contents, frame);
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

// TODO(crbug.com/654704): Android does not support PRE_ tests.
#if !BUILDFLAG(IS_ANDROID)
IN_PROC_BROWSER_TEST_P(DIPSTabHelperBrowserTest, PRE_PrepopulateTest) {
  // Simulate the user typing the URL to visit the page, which will record site
  // engagement.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("a.test", "/title1.html")));
}

IN_PROC_BROWSER_TEST_P(DIPSTabHelperBrowserTest, PrepopulateTest) {
  // Since there was previous site engagement, the DIPS DB should be
  // prepopulated with a user interaction timestamp.
  auto state = GetDIPSState(GURL("http://a.test"));
  ASSERT_TRUE(state.has_value());
  EXPECT_TRUE(state->user_interaction_times.first.has_value());
}

IN_PROC_BROWSER_TEST_P(DIPSTabHelperBrowserTest,
                       PRE_ChromeBrowsingDataRemover_Basic) {
  // Simulate the user typing the URL to visit the page, which will record site
  // engagement.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("a.test", "/title1.html")));
}

IN_PROC_BROWSER_TEST_P(DIPSTabHelperBrowserTest,
                       ChromeBrowsingDataRemover_Basic) {
  base::Time time = base::Time::Now();
  uint64_t remove_mask = chrome_browsing_data_remover::DATA_TYPE_HISTORY |
                         chrome_browsing_data_remover::DATA_TYPE_SITE_DATA;
  std::unique_ptr<content::BrowsingDataFilterBuilder> filter_builder(
      content::BrowsingDataFilterBuilder::Create(
          content::BrowsingDataFilterBuilder::Mode::kPreserve));
  content::BrowsingDataRemover* remover =
      GetActiveWebContents()->GetBrowserContext()->GetBrowsingDataRemover();

  // Since there was previous site engagement, the DIPS DB should be
  // prepopulated with a user interaction timestamp.
  absl::optional<StateValue> state_initial =
      GetDIPSState(GURL("http://a.test"));
  ASSERT_TRUE(state_initial.has_value());
  ASSERT_TRUE(state_initial->user_interaction_times.first.has_value());
  EXPECT_LE(state_initial->user_interaction_times.first.value(), time);
  EXPECT_GT(state_initial->user_interaction_times.first.value(),
            time - base::Days(1));

  base::RunLoop run_loop;
  base::OnceCallback<void(uint64_t)> callback =
      (base::BindLambdaForTesting([&](uint64_t) { run_loop.Quit(); }));
  browsing_data_important_sites_util::Remove(
      remove_mask, content::BrowsingDataRemover::ORIGIN_TYPE_UNPROTECTED_WEB,
      browsing_data::TimePeriod::LAST_DAY, std::move(filter_builder), remover,
      std::move(callback));
  run_loop.Run();

  // Verify that the user interaction has been cleared from the DIPS DB.
  absl::optional<StateValue> state_final = GetDIPSState(GURL("http://a.test"));
  EXPECT_FALSE(state_final.has_value());
}
#endif  // !BUILDFLAG(IS_ANDROID)
