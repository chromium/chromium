// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/raw_ptr.h"
#include "base/rand_util.h"
#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/simple_test_clock.h"
#include "base/test/test_file_util.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browsing_data/browsing_data_important_sites_util.h"
#include "chrome/browser/browsing_data/chrome_browsing_data_remover_constants.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/dips/dips_bounce_detector.h"
#include "chrome/browser/dips/dips_features.h"
#include "chrome/browser/dips/dips_service.h"
#include "chrome/browser/dips/dips_service_factory.h"
#include "chrome/browser/dips/dips_storage.h"
#include "chrome/browser/dips/dips_test_utils.h"
#include "chrome/browser/dips/dips_utils.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "chrome/test/base/testing_profile.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
#include "components/ukm/test_ukm_recorder.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/hit_test_region_observer.h"
#include "content/public/test/test_launcher.h"
#include "net/dns/mock_host_resolver.h"
#include "third_party/blink/public/common/switches.h"

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/test/base/ui_test_utils.h"
#endif  // !BUILDFLAG(IS_ANDROID)

using content::CookieAccessDetails;
using content::NavigationHandle;
using content::RenderFrameHost;
using content::WebContents;
using testing::Optional;
using testing::Pair;

using testing::Optional;
using testing::Pair;

namespace {

// Histogram names
constexpr char kTimeToInteraction[] =
    "Privacy.DIPS.TimeFromStorageToInteraction.Block3PC";
constexpr char kTimeToStorage[] =
    "Privacy.DIPS.TimeFromInteractionToStorage.Block3PC";
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
          dips::kFeature,
          {{"persist_database", "true"}, {"triggering_action", "bounce"}});
    } else {
      scoped_feature_list_.InitAndEnableFeatureWithParameters(
          dips::kFeature, {{"triggering_action", "bounce"}});
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
    host_resolver()->AddRule("c.test", "127.0.0.1");
    host_resolver()->AddRule("d.test", "127.0.0.1");
    DIPSWebContentsObserver::FromWebContents(GetActiveWebContents())
        ->SetClockForTesting(&test_clock_);
    DIPSService* dips_service = DIPSServiceFactory::GetForBrowserContext(
        GetActiveWebContents()->GetBrowserContext());
    dips_service->WaitForInitCompleteForTesting();

    // Initialize exceptions for 1P sites with embedded 3P cookies. Block 3PC by
    // default on a.test and d.test, since those are used as the initial and
    // final URL in the redirect chains. This avoids trimming bounces due to 1P
    // exceptions (e.g. Chrome Guard).
    map_ = HostContentSettingsMapFactory::GetForProfile(
        chrome_test_utils::GetActiveWebContents(this)->GetBrowserContext());
    map_->SetContentSettingCustomScope(
        ContentSettingsPattern::Wildcard(),
        ContentSettingsPattern::FromString("[*.]a.test"),
        ContentSettingsType::COOKIES, ContentSetting::CONTENT_SETTING_BLOCK);
    map_->SetContentSettingCustomScope(
        ContentSettingsPattern::Wildcard(),
        ContentSettingsPattern::FromString("[*.]d.test"),
        ContentSettingsType::COOKIES, ContentSetting::CONTENT_SETTING_BLOCK);
  }

  WebContents* GetActiveWebContents() {
    if (!web_contents_) {
      web_contents_ = chrome_test_utils::GetActiveWebContents(this);
    }
    return web_contents_;
  }

  void SetDIPSTime(base::Time time) { test_clock_.SetNow(time); }

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

  void EndRedirectChain() {
    WebContents* web_contents = GetActiveWebContents();
    DIPSService* dips_service = DIPSServiceFactory::GetForBrowserContext(
        web_contents->GetBrowserContext());
    GURL expected_url = web_contents->GetLastCommittedURL();

    RedirectChainObserver chain_observer(dips_service, expected_url);
    // Performing a browser-based navigation terminates the current redirect
    // chain.
    ASSERT_TRUE(content::NavigateToURL(
        web_contents,
        embedded_test_server()->GetURL("a.test", "/title1.html")));
    chain_observer.Wait();
  }

 private:
  raw_ptr<WebContents, AcrossTasksDanglingUntriaged> web_contents_ = nullptr;
  base::SimpleTestClock test_clock_;
  base::test::ScopedFeatureList scoped_feature_list_;
  raw_ptr<HostContentSettingsMap> map_;
};

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
  EXPECT_FALSE(GetDIPSState(GetDipsService(web_contents), url_a).has_value());
  EXPECT_FALSE(GetDIPSState(GetDipsService(web_contents), url_b).has_value());

  // Click on the a.test top-level site.
  SetDIPSTime(time);
  UserActivationObserver observer_a(web_contents,
                                    web_contents->GetPrimaryMainFrame());
  SimulateMouseClick(web_contents, 0, blink::WebMouseEvent::Button::kLeft);
  observer_a.Wait();

  // User interaction is recorded for a.test (the top-level frame).
  absl::optional<StateValue> state_a =
      GetDIPSState(GetDipsService(web_contents), url_a);
  ASSERT_TRUE(state_a.has_value());
  EXPECT_FALSE(state_a->site_storage_times.has_value());
  EXPECT_EQ(absl::make_optional(time), state_a->user_interaction_times->first);

  // Update the top-level page to have an iframe pointing to b.test.
  ASSERT_TRUE(content::NavigateIframeToURL(web_contents, kIframeId, url_b));
  content::RenderFrameHost* iframe = content::FrameMatchingPredicate(
      web_contents->GetPrimaryPage(),
      base::BindRepeating(&content::FrameIsChildOfMainFrame));
  // Wait until we can click on the iframe.
  content::WaitForHitTestData(iframe);

  // Click on the b.test iframe.
  base::Time frame_interaction_time =
      time + DIPSBounceDetector::kTimestampUpdateInterval;
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
  state_a = GetDIPSState(GetDipsService(web_contents), url_a);
  ASSERT_TRUE(state_a.has_value());
  EXPECT_FALSE(state_a->site_storage_times.has_value());
  EXPECT_EQ(absl::make_optional(frame_interaction_time),
            state_a->user_interaction_times->second);

  // The iframe site doesn't have any state.
  EXPECT_FALSE(GetDIPSState(GetDipsService(web_contents), url_b).has_value());
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
  EXPECT_FALSE(GetDIPSState(GetDipsService(web_contents), url).has_value());

  UserActivationObserver observer1(web_contents, frame);
  SimulateMouseClick(web_contents, 0, blink::WebMouseEvent::Button::kLeft);
  observer1.Wait();

  // One instance of user interaction is recorded.
  absl::optional<StateValue> state_1 =
      GetDIPSState(GetDipsService(web_contents), url);
  ASSERT_TRUE(state_1.has_value());
  EXPECT_FALSE(state_1->site_storage_times.has_value());
  EXPECT_EQ(absl::make_optional(time), state_1->user_interaction_times->first);
  EXPECT_EQ(state_1->user_interaction_times->first,
            state_1->user_interaction_times->second);

  SetDIPSTime(time + DIPSBounceDetector::kTimestampUpdateInterval +
              base::Seconds(10));
  UserActivationObserver observer_2(web_contents, frame);
  SimulateMouseClick(web_contents, 0, blink::WebMouseEvent::Button::kLeft);
  observer_2.Wait();

  // A second, different, instance of user interaction is recorded for the same
  // site.
  absl::optional<StateValue> state_2 =
      GetDIPSState(GetDipsService(web_contents), url);
  ASSERT_TRUE(state_2.has_value());
  EXPECT_FALSE(state_2->site_storage_times.has_value());
  EXPECT_NE(state_2->user_interaction_times->second,
            state_2->user_interaction_times->first);
  EXPECT_EQ(absl::make_optional(time), state_2->user_interaction_times->first);
  EXPECT_EQ(
      absl::make_optional(time + DIPSBounceDetector::kTimestampUpdateInterval +
                          base::Seconds(10)),
      state_2->user_interaction_times->second);
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
  EXPECT_FALSE(GetDIPSState(GetDipsService(web_contents), url_a).has_value());
  EXPECT_FALSE(GetDIPSState(GetDipsService(web_contents), url_b).has_value());

  // Write a cookie in the b.test iframe.
  SetDIPSTime(time);
  FrameCookieAccessObserver observer(web_contents, iframe,
                                     CookieAccessDetails::Type::kChange);
  ASSERT_TRUE(content::ExecJs(
      iframe, "document.cookie = 'foo=bar; SameSite=None; Secure';",
      content::EXECUTE_SCRIPT_NO_USER_GESTURE));
  observer.Wait();

  // Nothing recorded for a.test (the top-level frame).
  absl::optional<StateValue> state_a =
      GetDIPSState(GetDipsService(web_contents), url_a);
  EXPECT_FALSE(state_a.has_value());
  // Nothing recorded for b.test (the iframe), since we don't record non main
  // frame URLs to DIPS State.
  absl::optional<StateValue> state_b =
      GetDIPSState(GetDipsService(web_contents), url_b);
  EXPECT_FALSE(state_b.has_value());
}

IN_PROC_BROWSER_TEST_P(DIPSTabHelperBrowserTest,
                       StorageNotRecordedForThirdPartySubresource) {
  // We host the "image" on an HTTPS server, because for it to write a
  // cookie, the cookie needs to be SameSite=None and Secure.
  net::EmbeddedTestServer https_server(net::EmbeddedTestServer::TYPE_HTTPS);
  https_server.SetSSLConfig(net::EmbeddedTestServer::CERT_TEST_NAMES);
  https_server.AddDefaultHandlers(
      base::FilePath(FILE_PATH_LITERAL("chrome/test/data")));
  ASSERT_TRUE(https_server.Start());

  GURL page_url = embedded_test_server()->GetURL("a.test", "/title1.html");
  GURL image_url =
      https_server.GetURL("b.test", "/set-cookie?foo=bar;Secure;SameSite=None");
  content::WebContents* web_contents = GetActiveWebContents();
  base::Time time = base::Time::FromDoubleT(1);

  SetDIPSTime(time);
  // Set SameSite=None cookie on b.test.
  ASSERT_TRUE(content::NavigateToURL(
      web_contents, https_server.GetURL(
                        "b.test", "/set-cookie?foo=bar;Secure;SameSite=None")));
  ASSERT_TRUE(
      GetDIPSState(GetDipsService(web_contents), image_url).has_value());
  EXPECT_EQ(GetDIPSState(GetDipsService(web_contents), image_url)
                .value()
                .site_storage_times->second,
            time);

  // Navigate top-level page to a.test.
  ASSERT_TRUE(content::NavigateToURL(web_contents, page_url));

  // Advance time and cause a third-party cookie read by loading an "image" from
  // b.test.
  SetDIPSTime(time + base::Seconds(10));
  FrameCookieAccessObserver observer(web_contents,
                                     web_contents->GetPrimaryMainFrame(),
                                     CookieAccessDetails::Type::kRead);
  ASSERT_TRUE(content::ExecJs(web_contents,
                              content::JsReplace(
                                  R"(
    let img = document.createElement('img');
    img.src = $1;
    document.body.appendChild(img);)",
                                  image_url),
                              content::EXECUTE_SCRIPT_NO_USER_GESTURE));
  observer.Wait();

  // Nothing recorded for a.test (the top-level frame).
  EXPECT_FALSE(
      GetDIPSState(GetDipsService(web_contents), page_url).has_value());

  // The last site storage timestamp for b.test (the site hosting the image)
  // should be unchanged, since we don't record cookie accesses from loading
  // third-party resources.
  EXPECT_EQ(GetDIPSState(GetDipsService(web_contents), image_url)
                .value()
                .site_storage_times->second,
            time);
}

IN_PROC_BROWSER_TEST_P(DIPSTabHelperBrowserTest, MultipleSiteStoragesRecorded) {
  GURL url = embedded_test_server()->GetURL("b.test", "/set-cookie?foo=bar");
  base::Time time = base::Time::FromDoubleT(1);

  SetDIPSTime(time);
  // Navigating to this URL sets a cookie.
  ASSERT_TRUE(NavigateToURLAndWaitForCookieWrite(url));

  // One instance of site storage is recorded.
  absl::optional<StateValue> state_1 =
      GetDIPSState(GetDipsService(GetActiveWebContents()), url);
  ASSERT_TRUE(state_1.has_value());
  EXPECT_FALSE(state_1->user_interaction_times.has_value());
  EXPECT_EQ(absl::make_optional(time), state_1->site_storage_times->first);
  EXPECT_EQ(state_1->site_storage_times->second,
            state_1->site_storage_times->first);

  SetDIPSTime(time + base::Seconds(10));
  // Navigate to the URL again to rewrite the cookie.
  ASSERT_TRUE(NavigateToURLAndWaitForCookieWrite(url));

  // A second, different, instance of site storage is recorded for the same
  // site.
  absl::optional<StateValue> state_2 =
      GetDIPSState(GetDipsService(GetActiveWebContents()), url);
  ASSERT_TRUE(state_2.has_value());
  EXPECT_FALSE(state_2->user_interaction_times.has_value());
  EXPECT_NE(state_2->site_storage_times->second,
            state_2->site_storage_times->first);
  EXPECT_EQ(absl::make_optional(time), state_2->site_storage_times->first);
  EXPECT_EQ(absl::make_optional(time + base::Seconds(10)),
            state_2->site_storage_times->second);
}

IN_PROC_BROWSER_TEST_P(DIPSTabHelperBrowserTest, Histograms_StorageThenClick) {
  base::HistogramTester histograms;
  GURL url = embedded_test_server()->GetURL("b.test", "/set-cookie?foo=bar");
  base::Time time = base::Time::FromDoubleT(1);
  content::WebContents* web_contents = GetActiveWebContents();

  SetDIPSTime(time);
  // Navigating to this URL sets a cookie.
  ASSERT_TRUE(NavigateToURLAndWaitForCookieWrite(url));
  // Wait until we can click.
  content::WaitForHitTestData(web_contents->GetPrimaryMainFrame());
  WaitOnStorage(GetDipsService(web_contents));

  histograms.ExpectTotalCount(kTimeToInteraction, 0);
  histograms.ExpectTotalCount(kTimeToStorage, 0);

  SetDIPSTime(time + base::Seconds(10));
  UserActivationObserver observer(web_contents,
                                  web_contents->GetPrimaryMainFrame());
  SimulateMouseClick(web_contents, 0, blink::WebMouseEvent::Button::kLeft);
  observer.Wait();
  WaitOnStorage(GetDipsService(web_contents));

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
  GURL url = embedded_test_server()->GetURL("b.test", "/set-cookie?foo=bar");
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
  WaitOnStorage(GetDipsService(web_contents));

  histograms.ExpectTotalCount(kTimeToInteraction, 0);
  histograms.ExpectTotalCount(kTimeToInteraction_OTR_Block3PC, 0);
  histograms.ExpectTotalCount(kTimeToStorage, 0);

  SetDIPSTime(time + base::Seconds(10));
  UserActivationObserver observer(web_contents,
                                  web_contents->GetPrimaryMainFrame());
  SimulateMouseClick(web_contents, 0, blink::WebMouseEvent::Button::kLeft);
  observer.Wait();
  WaitOnStorage(GetDipsService(web_contents));

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
      web_contents, embedded_test_server()->GetURL("b.test", "/title1.html")));
  content::RenderFrameHost* frame = web_contents->GetPrimaryMainFrame();
  content::WaitForHitTestData(frame);  // wait until we can click.
  SetDIPSTime(time);
  UserActivationObserver click_observer(web_contents, frame);
  SimulateMouseClick(web_contents, 0, blink::WebMouseEvent::Button::kLeft);
  click_observer.Wait();
  WaitOnStorage(GetDipsService(web_contents));

  histograms.ExpectTotalCount(kTimeToInteraction, 0);
  histograms.ExpectTotalCount(kTimeToStorage, 0);

  // Write a cookie now that the click has been handled.
  SetDIPSTime(time + base::Seconds(10));
  FrameCookieAccessObserver cookie_observer(web_contents, frame,
                                            CookieAccessDetails::Type::kChange);
  ASSERT_TRUE(content::ExecJs(frame, "document.cookie = 'foo=bar';",
                              content::EXECUTE_SCRIPT_NO_USER_GESTURE));
  cookie_observer.Wait();
  WaitOnStorage(GetDipsService(web_contents));

  histograms.ExpectTotalCount(kTimeToInteraction, 0);
  histograms.ExpectTotalCount(kTimeToStorage, 1);
  histograms.ExpectUniqueTimeSample(kTimeToStorage, base::Seconds(10), 1);
}

IN_PROC_BROWSER_TEST_P(DIPSTabHelperBrowserTest,
                       Histograms_MultipleStoragesThenClick) {
  base::HistogramTester histograms;
  GURL url = embedded_test_server()->GetURL("b.test", "/set-cookie?foo=bar");
  base::Time time = base::Time::FromDoubleT(1);
  content::WebContents* web_contents = GetActiveWebContents();

  SetDIPSTime(time);
  // Navigating to this URL sets a cookie.
  ASSERT_TRUE(NavigateToURLAndWaitForCookieWrite(url));
  WaitOnStorage(GetDipsService(web_contents));

  // Navigate to the URL, setting the cookie again.
  SetDIPSTime(time + base::Seconds(3));
  ASSERT_TRUE(NavigateToURLAndWaitForCookieWrite(url));
  content::RenderFrameHost* frame = web_contents->GetPrimaryMainFrame();
  // Wait until we can click.
  content::WaitForHitTestData(frame);
  WaitOnStorage(GetDipsService(web_contents));

  // Verify both cookie writes were recorded.
  absl::optional<StateValue> state =
      GetDIPSState(GetDipsService(web_contents), url);
  ASSERT_TRUE(state.has_value());
  EXPECT_NE(state->site_storage_times->first,
            state->site_storage_times->second);
  EXPECT_EQ(absl::make_optional(time), state->site_storage_times->first);
  EXPECT_EQ(absl::make_optional(time + base::Seconds(3)),
            state->site_storage_times->second);
  EXPECT_FALSE(state->user_interaction_times.has_value());

  histograms.ExpectTotalCount(kTimeToInteraction, 0);
  histograms.ExpectTotalCount(kTimeToStorage, 0);

  SetDIPSTime(time + base::Seconds(10));
  UserActivationObserver observer(web_contents, frame);
  SimulateMouseClick(web_contents, 0, blink::WebMouseEvent::Button::kLeft);
  observer.Wait();
  WaitOnStorage(GetDipsService(web_contents));

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
  GURL url = embedded_test_server()->GetURL("b.test", "/title1.html");
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
  WaitOnStorage(GetDipsService(web_contents));

  // Click a second time.
  SetDIPSTime(time + DIPSBounceDetector::kTimestampUpdateInterval +
              base::Seconds(3));
  UserActivationObserver click_observer_2(web_contents, frame);
  SimulateMouseClick(web_contents, 0, blink::WebMouseEvent::Button::kLeft);
  click_observer_2.Wait();
  WaitOnStorage(GetDipsService(web_contents));

  // Verify both clicks were recorded.
  absl::optional<StateValue> state =
      GetDIPSState(GetDipsService(web_contents), url);
  ASSERT_TRUE(state.has_value());
  EXPECT_NE(state->user_interaction_times->first,
            state->user_interaction_times->second);
  EXPECT_EQ(absl::make_optional(time), state->user_interaction_times->first);
  EXPECT_EQ(
      absl::make_optional(time + DIPSBounceDetector::kTimestampUpdateInterval +
                          base::Seconds(3)),
      state->user_interaction_times->second);
  EXPECT_FALSE(state->site_storage_times.has_value());

  histograms.ExpectTotalCount(kTimeToInteraction, 0);
  histograms.ExpectTotalCount(kTimeToStorage, 0);

  // Write a cookie now that both clicks have been handled.
  SetDIPSTime(time + DIPSBounceDetector::kTimestampUpdateInterval +
              base::Seconds(10));
  FrameCookieAccessObserver cookie_observer(web_contents, frame,
                                            CookieAccessDetails::Type::kChange);
  ASSERT_TRUE(content::ExecJs(frame, "document.cookie = 'foo=bar';",
                              content::EXECUTE_SCRIPT_NO_USER_GESTURE));
  cookie_observer.Wait();
  WaitOnStorage(GetDipsService(web_contents));

  histograms.ExpectTotalCount(kTimeToInteraction, 0);
  histograms.ExpectTotalCount(kTimeToStorage, 1);
  // Unlike for TimeToInteraction metrics, we want to know the time from the
  // most recent user interaction, not the first, so the reported time delta
  // should be 7 seconds (not 10).
  histograms.ExpectUniqueTimeSample(kTimeToStorage, base::Seconds(7), 1);
}

IN_PROC_BROWSER_TEST_P(DIPSTabHelperBrowserTest,
                       ChromeBrowsingDataRemover_Basic) {
  content::WebContents* web_contents = GetActiveWebContents();
  base::Time interaction_time = base::Time::Now() - base::Seconds(10);
  SetDIPSTime(interaction_time);

  // Perform a click to get a.test added to the DIPS DB.
  ASSERT_TRUE(content::NavigateToURL(
      web_contents, embedded_test_server()->GetURL("a.test", "/title1.html")));
  UserActivationObserver observer(web_contents,
                                  web_contents->GetPrimaryMainFrame());
  SimulateMouseClick(web_contents, 0, blink::WebMouseEvent::Button::kLeft);
  observer.Wait();

  // Verify it was added.
  absl::optional<StateValue> state_initial =
      GetDIPSState(GetDipsService(web_contents), GURL("http://a.test"));
  ASSERT_TRUE(state_initial.has_value());
  ASSERT_TRUE(state_initial->user_interaction_times.has_value());
  EXPECT_EQ(state_initial->user_interaction_times->first, interaction_time);

  // Remove browsing data for the past day.
  uint64_t remove_mask = chrome_browsing_data_remover::DATA_TYPE_HISTORY |
                         chrome_browsing_data_remover::DATA_TYPE_SITE_DATA;
  std::unique_ptr<content::BrowsingDataFilterBuilder> filter_builder(
      content::BrowsingDataFilterBuilder::Create(
          content::BrowsingDataFilterBuilder::Mode::kPreserve));
  content::BrowsingDataRemover* remover =
      GetActiveWebContents()->GetBrowserContext()->GetBrowsingDataRemover();

  base::RunLoop run_loop;
  browsing_data_important_sites_util::Remove(
      remove_mask, content::BrowsingDataRemover::ORIGIN_TYPE_UNPROTECTED_WEB,
      browsing_data::TimePeriod::LAST_DAY, std::move(filter_builder), remover,
      base::IgnoreArgs<uint64_t>(run_loop.QuitClosure()));
  run_loop.Run();

  // Verify that the user interaction has been cleared from the DIPS DB.
  absl::optional<StateValue> state_final =
      GetDIPSState(GetDipsService(web_contents), GURL("http://a.test"));
  EXPECT_FALSE(state_final.has_value());
}

INSTANTIATE_TEST_SUITE_P(All, DIPSTabHelperBrowserTest, ::testing::Bool());

// TODO(crbug.com/654704): Android does not support PRE_ tests.
#if !BUILDFLAG(IS_ANDROID)
class DIPSPrepopulateTest : public PlatformBrowserTest {
 public:
  void SetUp() override {
    if (content::IsPreTest() && GetTestPreCount() % 2 != 0) {
      // Alternate between disabling and enabling DIPS in `PRE_` tests.
      // Only disable explicitly since the feature is on by default.
      feature_list_.InitAndDisableFeature(dips::kFeature);
    } else {
      feature_list_.InitAndEnableFeatureWithParameters(
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
    host_resolver()->AddRule("c.test", "127.0.0.1");
    dips_service = GetDipsService(GetActiveWebContents());
    if (dips_service) {
      dips_service->WaitForInitCompleteForTesting();
    }
  }

  WebContents* GetActiveWebContents() {
    return chrome_test_utils::GetActiveWebContents(this);
  }

 protected:
  void FlushLossyWebsiteSettings() {
    HostContentSettingsMapFactory::GetForProfile(
        GetActiveWebContents()->GetBrowserContext())
        ->FlushLossyWebsiteSettings();
  }

  raw_ptr<DIPSService, DanglingUntriaged> dips_service;

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(DIPSPrepopulateTest, PRE_PrepopulateTest) {
  ASSERT_EQ(dips_service, nullptr);  // Verify that DIPS is off.
  // Simulate the user typing the URL to visit the page, which will record site
  // engagement.
  ASSERT_TRUE(ui_test_utils::NavigateToURLBlockUntilNavigationsComplete(
      browser(), embedded_test_server()->GetURL("c.test", "/title1.html"), 1));
  FlushLossyWebsiteSettings();
}

IN_PROC_BROWSER_TEST_F(DIPSPrepopulateTest, PrepopulateTest) {
  ASSERT_NE(dips_service, nullptr);  // Verify that DIPS is on.
  // Since there was previous site engagement, the DIPS DB should be
  // prepopulated with a user interaction timestamp.
  auto state = GetDIPSState(GetDipsService(GetActiveWebContents()),
                            GURL("http://c.test"));
  ASSERT_TRUE(state.has_value());
  EXPECT_TRUE(state->user_interaction_times.has_value());
}

IN_PROC_BROWSER_TEST_F(DIPSPrepopulateTest,
                       PRE_PRE_PRE_PrepopulateExactlyOnce) {
  ASSERT_EQ(dips_service, nullptr);  // Verify that DIPS is off.
  // Record site engagement on a.test.
  ASSERT_TRUE(ui_test_utils::NavigateToURLBlockUntilNavigationsComplete(
      browser(), embedded_test_server()->GetURL("a.test", "/title1.html"), 1));
  FlushLossyWebsiteSettings();
}

IN_PROC_BROWSER_TEST_F(DIPSPrepopulateTest, PRE_PRE_PrepopulateExactlyOnce) {
  // Verify that a.test is prepopulated with the earlier interaction.
  auto state = GetDIPSState(GetDipsService(GetActiveWebContents()),
                            GURL("http://a.test"));
  ASSERT_TRUE(state.has_value());
  EXPECT_TRUE(state->user_interaction_times.has_value());
}

IN_PROC_BROWSER_TEST_F(DIPSPrepopulateTest, PRE_PrepopulateExactlyOnce) {
  ASSERT_EQ(dips_service, nullptr);  // Verify that DIPS is off.
  // Record site engagement on b.test.
  ASSERT_TRUE(ui_test_utils::NavigateToURLBlockUntilNavigationsComplete(
      browser(), embedded_test_server()->GetURL("b.test", "/title1.html"), 1));
  FlushLossyWebsiteSettings();
}

// TODO (crbug.com/1418692): Rework this test to work without enabling and
// disabling the DIPS feature, as opening a profile with the feature disabled
// now causes any existing db files it has to be removed.
IN_PROC_BROWSER_TEST_F(DIPSPrepopulateTest, DISABLED_PrepopulateExactlyOnce) {
  ASSERT_NE(dips_service, nullptr);  // Verify that DIPS is on.
  // Only the sites that were prepopulated the first time is in the database.
  auto a_state = GetDIPSState(GetDipsService(GetActiveWebContents()),
                              GURL("http://a.test"));
  ASSERT_TRUE(a_state.has_value());
  EXPECT_TRUE(a_state->user_interaction_times.has_value());

  auto b_state = GetDIPSState(GetDipsService(GetActiveWebContents()),
                              GURL("http://b.test"));
  EXPECT_FALSE(b_state.has_value());
}

#endif  // !BUILDFLAG(IS_ANDROID)

// Makes a long URL involving several stateful stateful bounces on b.test,
// ultimately landing on c.test. Returns both the full redirect URL and the URL
// for the landing page. The landing page URL has a param appended to it to
// ensure it's unique to URLs from previous calls (to prevent caching).
std::pair<GURL, GURL> MakeRedirectAndFinalUrl(net::EmbeddedTestServer* server) {
  uint64_t unique_value = base::RandUint64();
  std::string final_dest =
      base::StrCat({"/title1.html?i=", base::NumberToString(unique_value)});
  std::string redirect_path =
      "/cross-site-with-cookie/b.test/cross-site-with-cookie/"
      "b.test/cross-site-with-cookie/b.test/cross-site-with-cookie/"
      "b.test/cross-site-with-cookie/b.test/cross-site-with-cookie/"
      "b.test/cross-site-with-cookie/b.test/cross-site-with-cookie/"
      "b.test/cross-site-with-cookie/b.test/cross-site-with-cookie/d.test";
  redirect_path += final_dest;
  return std::make_pair(server->GetURL("b.test", redirect_path),
                        server->GetURL("d.test", final_dest));
}

// Attempt to detect flakiness in waiting for DIPS storage by repeatedly
// visiting long redirect chains, deleting the relevant rows, and verifying the
// rows don't come back.
IN_PROC_BROWSER_TEST_P(DIPSTabHelperBrowserTest,
                       DetectRedirectHandlingFlakiness) {
  WebContents* web_contents = GetActiveWebContents();

  auto* dips_storage = DIPSServiceFactory::GetForBrowserContext(
                           web_contents->GetBrowserContext())
                           ->storage();

  for (int i = 0; i < 10; i++) {
    const base::Time bounce_time = base::Time::FromDoubleT(i + 1);
    SetDIPSTime(bounce_time);
    LOG(INFO) << "*** i=" << i << " ***";
    // Make b.test statefully bounce.
    ASSERT_TRUE(content::NavigateToURL(
        web_contents,
        embedded_test_server()->GetURL("a.test", "/title1.html")));
    auto [redirect_url, final_url] =
        MakeRedirectAndFinalUrl(embedded_test_server());
    ASSERT_TRUE(content::NavigateToURLFromRenderer(web_contents, redirect_url,
                                                   final_url));
    // End the chain so the bounce is recorded.
    EndRedirectChain();

    // Verify the bounces were recorded.
    absl::optional<StateValue> b_state =
        GetDIPSState(GetDipsService(web_contents), GURL("http://b.test"));
    ASSERT_TRUE(b_state.has_value());
    ASSERT_THAT(b_state->site_storage_times,
                Optional(Pair(bounce_time, bounce_time)));
    ASSERT_THAT(b_state->bounce_times,
                Optional(Pair(bounce_time, bounce_time)));
    ASSERT_THAT(b_state->stateful_bounce_times,
                Optional(Pair(bounce_time, bounce_time)));

    dips_storage->AsyncCall(&DIPSStorage::RemoveRows)
        .WithArgs(std::vector<std::string>{"b.test"});

    // Verify the row was removed before repeating the test. If we did not
    // correctly wait for the whole chain to be handled before removing the row
    // for b.test, it will likely be written again and this check will fail.
    // (And if a write happens after this check, it will include a stale
    // timestamp and will cause one the of the checks above to fail on the next
    // loop iteration.)
    ASSERT_FALSE(
        GetDIPSState(GetDipsService(web_contents), GURL("http://b.test"))
            .has_value());
  }
}

IN_PROC_BROWSER_TEST_P(DIPSTabHelperBrowserTest,
                       UserClearedSitesAreNotReportedToUKM) {
  ukm::TestAutoSetUkmRecorder ukm_recorder;
  content::WebContents* web_contents = GetActiveWebContents();
  DIPSService* dips_service = DIPSServiceFactory::GetForBrowserContext(
      web_contents->GetBrowserContext());
  // A time more than an hour ago.
  base::Time old_bounce_time = base::Time::Now() - base::Hours(2);
  // A time within the past hour.
  base::Time recent_bounce_time = base::Time::Now() - base::Minutes(10);

  SetDIPSTime(old_bounce_time);
  // Make b.test statefully bounce to d.test.
  ASSERT_TRUE(content::NavigateToURL(
      web_contents, embedded_test_server()->GetURL("a.test", "/title1.html")));
  ASSERT_TRUE(content::NavigateToURLFromRenderer(
      web_contents,
      embedded_test_server()->GetURL(
          "b.test", "/cross-site-with-cookie/d.test/title1.html"),
      embedded_test_server()->GetURL("d.test", "/title1.html")));
  // End the chain so the bounce is recorded.
  ASSERT_TRUE(content::NavigateToURL(
      web_contents, embedded_test_server()->GetURL("a.test", "/title1.html")));

  SetDIPSTime(recent_bounce_time);
  // Make c.test statefully bounce to d.test.
  ASSERT_TRUE(content::NavigateToURL(
      web_contents, embedded_test_server()->GetURL("a.test", "/title1.html")));
  ASSERT_TRUE(content::NavigateToURLFromRenderer(
      web_contents,
      embedded_test_server()->GetURL(
          "c.test", "/cross-site-with-cookie/d.test/title1.html"),
      embedded_test_server()->GetURL("d.test", "/title1.html")));
  EndRedirectChain();

  // Verify the bounces were recorded. b.test:
  absl::optional<StateValue> state =
      GetDIPSState(GetDipsService(web_contents), GURL("http://b.test"));
  ASSERT_TRUE(state.has_value());
  ASSERT_THAT(state->stateful_bounce_times,
              Optional(Pair(old_bounce_time, old_bounce_time)));
  ASSERT_EQ(state->user_interaction_times, absl::nullopt);
  // c.test:
  state = GetDIPSState(GetDipsService(web_contents), GURL("http://c.test"));
  ASSERT_TRUE(state.has_value());
  ASSERT_THAT(state->stateful_bounce_times,
              Optional(Pair(recent_bounce_time, recent_bounce_time)));
  ASSERT_EQ(state->user_interaction_times, absl::nullopt);

  // Remove browsing data for the past hour. This should include c.test but not
  // b.test.
  base::RunLoop run_loop;
  browsing_data_important_sites_util::Remove(
      chrome_browsing_data_remover::DATA_TYPE_HISTORY |
          chrome_browsing_data_remover::DATA_TYPE_SITE_DATA,
      content::BrowsingDataRemover::ORIGIN_TYPE_UNPROTECTED_WEB,
      browsing_data::TimePeriod::LAST_HOUR,
      content::BrowsingDataFilterBuilder::Create(
          content::BrowsingDataFilterBuilder::Mode::kPreserve),
      web_contents->GetBrowserContext()->GetBrowsingDataRemover(),
      base::IgnoreArgs<uint64_t>(run_loop.QuitClosure()));
  run_loop.Run();

  // Verify only the DIPS record for c.test was deleted.
  ASSERT_TRUE(GetDIPSState(GetDipsService(web_contents), GURL("http://b.test"))
                  .has_value());
  ASSERT_FALSE(GetDIPSState(GetDipsService(web_contents), GURL("http://c.test"))
                   .has_value());

  // Trigger the DIPS timer which will delete tracker data.
  SetDIPSTime(recent_bounce_time + dips::kGracePeriod.Get() +
              base::Milliseconds(1));
  dips_service->OnTimerFiredForTesting();
  dips_service->storage()->FlushPostedTasksForTesting();
  base::RunLoop().RunUntilIdle();

  // Verify that both DIPS records are now gone.
  ASSERT_FALSE(GetDIPSState(GetDipsService(web_contents), GURL("http://b.test"))
                   .has_value());
  ASSERT_FALSE(GetDIPSState(GetDipsService(web_contents), GURL("http://c.test"))
                   .has_value());

  // Only b.test was reported to UKM.
  EXPECT_THAT(ukm_recorder, EntryUrlsAre("DIPS.Deletion", {"http://b.test/"}));
}

IN_PROC_BROWSER_TEST_P(DIPSTabHelperBrowserTest, SitesInOpenTabsAreExempt) {
  content::WebContents* web_contents = GetActiveWebContents();
  DIPSService* dips_service = DIPSServiceFactory::GetForBrowserContext(
      web_contents->GetBrowserContext());

  // A time within the past hour.
  base::Time bounce_time = base::Time::Now() - base::Minutes(10);
  SetDIPSTime(bounce_time);

  // Make b.test statefully bounce to d.test.
  ASSERT_TRUE(content::NavigateToURL(
      web_contents, embedded_test_server()->GetURL("a.test", "/title1.html")));
  ASSERT_TRUE(content::NavigateToURLFromRenderer(
      web_contents,
      embedded_test_server()->GetURL(
          "b.test", "/cross-site-with-cookie/d.test/title1.html"),
      embedded_test_server()->GetURL("d.test", "/title1.html")));
  EndRedirectChain();

  // Make c.test statefully bounce to d.test.
  ASSERT_TRUE(content::NavigateToURL(
      web_contents, embedded_test_server()->GetURL("a.test", "/title1.html")));
  ASSERT_TRUE(content::NavigateToURLFromRenderer(
      web_contents,
      embedded_test_server()->GetURL(
          "c.test", "/cross-site-with-cookie/d.test/title1.html"),
      embedded_test_server()->GetURL("d.test", "/title1.html")));
  EndRedirectChain();

  // Verify the bounces through b.test and c.test were recorded.
  absl::optional<StateValue> b_state =
      GetDIPSState(GetDipsService(web_contents), GURL("http://b.test"));
  ASSERT_TRUE(b_state.has_value());
  ASSERT_THAT(b_state->stateful_bounce_times,
              Optional(Pair(bounce_time, bounce_time)));
  ASSERT_EQ(b_state->user_interaction_times, absl::nullopt);

  absl::optional<StateValue> c_state =
      GetDIPSState(GetDipsService(web_contents), GURL("http://c.test"));
  ASSERT_TRUE(c_state.has_value());
  ASSERT_THAT(c_state->stateful_bounce_times,
              Optional(Pair(bounce_time, bounce_time)));
  ASSERT_EQ(c_state->user_interaction_times, absl::nullopt);

  // Open b.test in a new tab.
  auto new_tab = OpenInNewTab(
      web_contents, embedded_test_server()->GetURL("c.test", "/title1.html"));
  ASSERT_TRUE(new_tab.has_value()) << new_tab.error();

  // Navigate to c.test in the new tab.
  ASSERT_TRUE(content::NavigateToURL(
      *new_tab, embedded_test_server()->GetURL("c.test", "/title1.html")));

  // Trigger the DIPS timer which would delete tracker data.
  SetDIPSTime(bounce_time + dips::kGracePeriod.Get() + base::Milliseconds(1));
  dips_service->OnTimerFiredForTesting();
  dips_service->storage()->FlushPostedTasksForTesting();
  base::RunLoop().RunUntilIdle();

  // Verify that the DIPS record for b.test is now gone, because there is no
  // open tab on b.test.
  EXPECT_FALSE(GetDIPSState(GetDipsService(web_contents), GURL("http://b.test"))
                   .has_value());

  // Verify that the DIPS record for c.test is still present, because there is
  // an open tab on c.test.
  EXPECT_TRUE(GetDIPSState(GetDipsService(web_contents), GURL("http://c.test"))
                  .has_value());
}

IN_PROC_BROWSER_TEST_P(DIPSTabHelperBrowserTest,
                       SitesInDestroyedTabsAreNotExempt) {
  content::WebContents* web_contents = GetActiveWebContents();
  DIPSService* dips_service = DIPSServiceFactory::GetForBrowserContext(
      web_contents->GetBrowserContext());

  // A time within the past hour.
  base::Time bounce_time = base::Time::Now() - base::Minutes(10);
  SetDIPSTime(bounce_time);

  // Make b.test statefully bounce to d.test.
  ASSERT_TRUE(content::NavigateToURL(
      web_contents, embedded_test_server()->GetURL("a.test", "/title1.html")));
  ASSERT_TRUE(content::NavigateToURLFromRenderer(
      web_contents,
      embedded_test_server()->GetURL(
          "b.test", "/cross-site-with-cookie/d.test/title1.html"),
      embedded_test_server()->GetURL("d.test", "/title1.html")));
  EndRedirectChain();

  // Verify the bounce through b.test was recorded.
  absl::optional<StateValue> b_state =
      GetDIPSState(GetDipsService(web_contents), GURL("http://b.test"));
  ASSERT_TRUE(b_state.has_value());
  ASSERT_THAT(b_state->stateful_bounce_times,
              Optional(Pair(bounce_time, bounce_time)));
  ASSERT_EQ(b_state->user_interaction_times, absl::nullopt);

  // Open b.test in a new tab.
  auto new_tab = OpenInNewTab(
      web_contents, embedded_test_server()->GetURL("c.test", "/title1.html"));
  ASSERT_TRUE(new_tab.has_value()) << new_tab.error();

  // Close the new tab with b.test.
  CloseTab(*new_tab);

  // Trigger the DIPS timer which would delete tracker data.
  SetDIPSTime(bounce_time + dips::kGracePeriod.Get() + base::Milliseconds(1));
  dips_service->OnTimerFiredForTesting();
  dips_service->storage()->FlushPostedTasksForTesting();
  base::RunLoop().RunUntilIdle();

  // Verify that the DIPS record for b.test is now gone, because there is no
  // open tab on b.test.
  EXPECT_FALSE(GetDIPSState(GetDipsService(web_contents), GURL("http://b.test"))
                   .has_value());
}

// Multiple running profiles is not supported on Android or ChromeOS Ash.
#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_CHROMEOS_ASH)
IN_PROC_BROWSER_TEST_P(DIPSTabHelperBrowserTest,
                       SitesInOpenTabsForDifferentProfilesAreNotExempt) {
  content::WebContents* web_contents = GetActiveWebContents();
  DIPSService* dips_service = DIPSServiceFactory::GetForBrowserContext(
      web_contents->GetBrowserContext());

  // A time within the past hour.
  base::Time bounce_time = base::Time::Now() - base::Minutes(10);
  SetDIPSTime(bounce_time);

  // Make c.test statefully bounce to d.test.
  ASSERT_TRUE(content::NavigateToURL(
      web_contents, embedded_test_server()->GetURL("a.test", "/title1.html")));
  ASSERT_TRUE(content::NavigateToURLFromRenderer(
      web_contents,
      embedded_test_server()->GetURL(
          "c.test", "/cross-site-with-cookie/d.test/title1.html"),
      embedded_test_server()->GetURL("d.test", "/title1.html")));
  EndRedirectChain();

  // Verify the bounce through c.test was recorded.
  absl::optional<StateValue> c_state =
      GetDIPSState(GetDipsService(web_contents), GURL("http://c.test"));
  ASSERT_TRUE(c_state.has_value());
  ASSERT_THAT(c_state->stateful_bounce_times,
              Optional(Pair(bounce_time, bounce_time)));
  ASSERT_EQ(c_state->user_interaction_times, absl::nullopt);

  // Open c.test on a new tab in a new window/profile.
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  base::FilePath profile_path = profile_manager->user_data_dir().Append(
      FILE_PATH_LITERAL("OtherProfile"));
  Browser* new_browser = chrome::OpenEmptyWindow(
      &profiles::testing::CreateProfileSync(profile_manager, profile_path));
  chrome::NewTab(new_browser);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(new_browser, GURL("http://c.test")));

  // Trigger the DIPS timer which would delete tracker data.
  SetDIPSTime(bounce_time + dips::kGracePeriod.Get() + base::Milliseconds(1));
  dips_service->OnTimerFiredForTesting();
  dips_service->storage()->FlushPostedTasksForTesting();
  base::RunLoop().RunUntilIdle();

  // The DIPS record for c.test was removed, because open tabs in a different
  // profile are not exempt.
  EXPECT_FALSE(GetDIPSState(GetDipsService(web_contents), GURL("http://c.test"))
                   .has_value());
}
#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_CHROMEOS_ASH)
