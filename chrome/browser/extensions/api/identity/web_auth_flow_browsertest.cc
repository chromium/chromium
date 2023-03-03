// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/identity/web_auth_flow.h"

#include "base/strings/strcat.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "base/test/test_mock_time_task_runner.h"
#include "base/time/time.h"
#include "chrome/browser/extensions/api/identity/web_auth_flow_info_bar_delegate.h"
#include "chrome/browser/profiles/keep_alive/profile_keep_alive_types.h"
#include "chrome/browser/profiles/keep_alive/scoped_profile_keep_alive.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/keep_alive_registry/keep_alive_types.h"
#include "components/keep_alive_registry/scoped_keep_alive.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/fenced_frame_test_util.h"
#include "content/public/test/test_navigation_observer.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

class MockWebAuthFlowDelegate : public WebAuthFlow::Delegate {
 public:
  MOCK_METHOD(void, OnAuthFlowURLChange, (const GURL&), (override));
  MOCK_METHOD(void, OnAuthFlowTitleChange, (const std::string&), (override));
  MOCK_METHOD(void, OnAuthFlowFailure, (WebAuthFlow::Failure), (override));
};

class WebAuthFlowBrowserTest : public InProcessBrowserTest {
 public:
  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    ASSERT_TRUE(embedded_test_server()->Start());

    // Delete the flow early if OnAuthFlowFailure is called. Simulates real
    // usages.
    ON_CALL(mock(), OnAuthFlowFailure(testing::_))
        .WillByDefault(
            [this](WebAuthFlow::Failure failure) { DeleteWebAuthFlow(); });
  }

  void DeleteWebAuthFlow() {
    DCHECK(web_auth_flow_);
    // Delete the web auth flow (uses DeleteSoon).
    web_auth_flow_.release()->DetachDelegateAndDelete();
  }

  void TearDownOnMainThread() override {
    // Ensures any timer tasks finish.
    timeout_task_runner_->RunUntilIdle();
    if (web_auth_flow_) {
      DeleteWebAuthFlow();
    }
    // Ensures `web_auth_flow_` is deleted before teardown. This cannot be run
    // in |DeleteWebAuthFlow| as it can be called from `timeout_task_runner_`'s
    // loop.
    base::RunLoop().RunUntilIdle();
    InProcessBrowserTest::TearDownOnMainThread();
  }

  void StartWebAuthFlow(
      const GURL& url,
      WebAuthFlow::Partition partition = WebAuthFlow::LAUNCH_WEB_AUTH_FLOW,
      WebAuthFlow::Mode mode = WebAuthFlow::Mode::INTERACTIVE,
      Profile* profile = nullptr,
      WebAuthFlow::AbortOnLoad abort_on_load_for_non_interactive =
          WebAuthFlow::AbortOnLoad::kYes,
      absl::optional<base::TimeDelta> timeout_for_non_interactive =
          absl::nullopt) {
    if (!profile)
      profile = browser()->profile();

    web_auth_flow_ = std::make_unique<WebAuthFlow>(
        &mock_web_auth_flow_delegate_, profile, url, mode, partition,
        abort_on_load_for_non_interactive, timeout_for_non_interactive);

    timeout_task_runner_ = base::MakeRefCounted<base::TestMockTimeTaskRunner>();
    web_auth_flow_->SetClockForTesting(timeout_task_runner_->GetMockTickClock(),
                                       timeout_task_runner_);
    web_auth_flow_->Start();
  }

  WebAuthFlow* web_auth_flow() { return web_auth_flow_.get(); }

  content::WebContents* web_contents() {
    if (!web_auth_flow_) {
      return nullptr;
    }
    return web_auth_flow_->web_contents();
  }

  MockWebAuthFlowDelegate& mock() { return mock_web_auth_flow_delegate_; }

  scoped_refptr<base::TestMockTimeTaskRunner> timeout_task_runner() {
    return timeout_task_runner_;
  }

 private:
  std::unique_ptr<WebAuthFlow> web_auth_flow_;
  MockWebAuthFlowDelegate mock_web_auth_flow_delegate_;
  scoped_refptr<base::TestMockTimeTaskRunner> timeout_task_runner_;
};

class WebAuthFlowInBrowserTabParamBrowserTest
    : public WebAuthFlowBrowserTest,
      public testing::WithParamInterface<bool> {
 public:
  WebAuthFlowInBrowserTabParamBrowserTest() {
    scoped_feature_list_.InitWithFeatureState(
        features::kWebAuthFlowInBrowserTab, use_tab_feature_enabled());
  }

  bool use_tab_feature_enabled() { return GetParam(); }

  bool JsRedirectToUrl(const GURL& url) {
    content::TestNavigationObserver redirect_observer(url);
    redirect_observer.WatchExistingWebContents();
    const std::string script =
        base::StringPrintf("window.location.href = '%s'", url.spec().c_str());
    bool result = content::ExecJs(web_contents(), script);
    if (result) {
      redirect_observer.Wait();
    }
    return result;
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_P(WebAuthFlowInBrowserTabParamBrowserTest,
                       OnAuthFlowURLChangeCalled) {
  const GURL auth_url = embedded_test_server()->GetURL("/title1.html");

  // Observer for waiting until a navigation to a url has finished.
  content::TestNavigationObserver navigation_observer(auth_url);
  navigation_observer.StartWatchingNewWebContents();

  // The delegate method OnAuthFlowURLChange should be called
  // by DidStartNavigation.
  EXPECT_CALL(mock(), OnAuthFlowURLChange(auth_url));
  StartWebAuthFlow(auth_url);

  navigation_observer.WaitForNavigationFinished();
}

IN_PROC_BROWSER_TEST_P(WebAuthFlowInBrowserTabParamBrowserTest,
                       OnAuthFlowFailureChangeCalled) {
  // Navigate to a url that doesn't exist.
  const GURL error_url = embedded_test_server()->GetURL("/error");

  content::TestNavigationObserver navigation_observer(error_url);
  navigation_observer.StartWatchingNewWebContents();

  // The delegate method OnAuthFlowFailure should be called
  // by DidFinishNavigation.
  EXPECT_CALL(mock(), OnAuthFlowFailure(WebAuthFlow::LOAD_FAILED));
  StartWebAuthFlow(error_url);

  navigation_observer.WaitForNavigationFinished();
}

// Tests that the flow launched in silent mode with default parameters will
// terminate immediately with the "interacation required" error if the page
// loads and does not navigate to the redirect URL.
IN_PROC_BROWSER_TEST_P(WebAuthFlowInBrowserTabParamBrowserTest,
                       OnAuthFlowFailureCalledInteractionRequired) {
  const GURL auth_url = embedded_test_server()->GetURL("/title1.html");

  content::TestNavigationObserver navigation_observer(auth_url);
  navigation_observer.StartWatchingNewWebContents();

  // The delegate method OnAuthFlowURLChange should be called
  // by DidStartNavigation.
  EXPECT_CALL(mock(), OnAuthFlowURLChange(auth_url));
  // In SILENT mode, DidStopLoading() will force the auth flow to fail if it has
  // not already redirected, because we did not specify a timeout.
  EXPECT_CALL(mock(), OnAuthFlowFailure(WebAuthFlow::INTERACTION_REQUIRED));
  StartWebAuthFlow(auth_url, WebAuthFlow::LAUNCH_WEB_AUTH_FLOW,
                   WebAuthFlow::SILENT);

  navigation_observer.Wait();
}

// Tests that the flow launched in silent mode with
// `abortOnLoadForNonInteractive` set to `false` will terminate with the
// "interaction required" after a specified timeout if the page loads and does
// not navigate to the redirect URL.
IN_PROC_BROWSER_TEST_P(WebAuthFlowInBrowserTabParamBrowserTest,
                       OnAuthFlowInteractionRequiredWithTimeout) {
  const GURL auth_url = embedded_test_server()->GetURL("/title1.html");

  content::TestNavigationObserver navigation_observer(auth_url);
  navigation_observer.StartWatchingNewWebContents();

  // The delegate method OnAuthFlowURLChange should be called
  // by DidStartNavigation.
  EXPECT_CALL(mock(), OnAuthFlowURLChange(auth_url));
  // In SILENT mode, DidStopLoading() will wait for our specified 50ms timeout
  // before calling OnAuthFlowFailure.
  EXPECT_CALL(mock(), OnAuthFlowFailure).Times(0);
  StartWebAuthFlow(auth_url, WebAuthFlow::LAUNCH_WEB_AUTH_FLOW,
                   WebAuthFlow::SILENT, /*profile=*/nullptr,
                   WebAuthFlow::AbortOnLoad::kNo,
                   /*timeout_for_non_interactive=*/base::Milliseconds(50));
  navigation_observer.Wait();
  testing::Mock::VerifyAndClearExpectations(&mock());

  // Increment the time by 40ms - nothing should happen.
  EXPECT_CALL(mock(), OnAuthFlowFailure).Times(0);
  timeout_task_runner()->FastForwardBy(base::Milliseconds(40));
  testing::Mock::VerifyAndClearExpectations(&mock());

  // Now we exceed our 50ms limit and expect a failure.
  EXPECT_CALL(mock(), OnAuthFlowFailure(WebAuthFlow::INTERACTION_REQUIRED));
  timeout_task_runner()->FastForwardBy(base::Milliseconds(20));
}

// Tests that the flow launched in silent mode with
// `abortOnLoadForNonInteractive` set to `false` will terminate with the
// "interaction required" error after a default timeout if the page loads and
// does not navigate to the redirect URL.
IN_PROC_BROWSER_TEST_P(WebAuthFlowInBrowserTabParamBrowserTest,
                       OnAuthFlowInteractionRequiredWithDefaultTimeout) {
  const GURL auth_url = embedded_test_server()->GetURL("/title1.html");

  content::TestNavigationObserver navigation_observer(auth_url);
  navigation_observer.StartWatchingNewWebContents();

  // The delegate method OnAuthFlowURLChange should be called
  // by DidStartNavigation.
  EXPECT_CALL(mock(), OnAuthFlowURLChange(auth_url));
  // In SILENT mode, DidStopLoading() will wait for the default 1 minute timeout
  // before calling OnAuthFlowFailure.
  EXPECT_CALL(mock(), OnAuthFlowFailure).Times(0);
  StartWebAuthFlow(auth_url, WebAuthFlow::LAUNCH_WEB_AUTH_FLOW,
                   WebAuthFlow::SILENT, /*profile=*/nullptr,
                   WebAuthFlow::AbortOnLoad::kNo);
  navigation_observer.Wait();
  testing::Mock::VerifyAndClearExpectations(&mock());

  // Increment the time by 59s - nothing should happen.
  EXPECT_CALL(mock(), OnAuthFlowFailure).Times(0);
  timeout_task_runner()->FastForwardBy(base::Seconds(59));
  testing::Mock::VerifyAndClearExpectations(&mock());

  // Now we exceed the 1 minute default limit and expect a failure.
  // The error is "interaction required" when the flow times out when the page
  // has already loaded.
  EXPECT_CALL(mock(), OnAuthFlowFailure(WebAuthFlow::INTERACTION_REQUIRED));
  timeout_task_runner()->FastForwardBy(base::Seconds(2));
}

// Tests that the flow launched in silent mode with `timeoutMsForNonInteractive`
// set will terminate with the "timed out" error after a timeout if the page
// fails to load (distinct from the flow failing to navigate to the redirect URL
// in time).
IN_PROC_BROWSER_TEST_P(WebAuthFlowInBrowserTabParamBrowserTest,
                       OnAuthFlowPageLoadTimeout) {
  const GURL auth_url = embedded_test_server()->GetURL("/hung-after-headers");

  // The delegate method OnAuthFlowURLChange should be called
  // by DidStartNavigation.
  base::RunLoop run_loop;
  ON_CALL(mock(), OnAuthFlowURLChange(testing::_))
      .WillByDefault([&run_loop](const GURL& url) { run_loop.Quit(); });
  EXPECT_CALL(mock(), OnAuthFlowURLChange(auth_url));
  // In SILENT mode, DidStopLoading() will wait for our specified 50ms timeout
  // before calling OnAuthFlowFailure.
  EXPECT_CALL(mock(), OnAuthFlowFailure).Times(0);
  StartWebAuthFlow(auth_url, WebAuthFlow::LAUNCH_WEB_AUTH_FLOW,
                   WebAuthFlow::SILENT, /*profile=*/nullptr,
                   WebAuthFlow::AbortOnLoad::kYes,
                   /*timeout_for_non_interactive=*/base::Milliseconds(50));
  // Wait for navigation to the failing page to start first.
  run_loop.Run();
  testing::Mock::VerifyAndClearExpectations(&mock());

  // Increment the time by 40ms - nothing should happen.
  EXPECT_CALL(mock(), OnAuthFlowFailure).Times(0);
  timeout_task_runner()->FastForwardBy(base::Milliseconds(40));
  testing::Mock::VerifyAndClearExpectations(&mock());

  // Now we exceed our 50ms limit and expect a failure. The error is "load
  // timed out" when the flow times out while the page is still loading.
  EXPECT_CALL(mock(), OnAuthFlowFailure(WebAuthFlow::TIMED_OUT));
  timeout_task_runner()->FastForwardBy(base::Milliseconds(20));
}

// Tests that the flow launched in silent mode with
// `abortOnLoadForNonInteractive` set to `false` and
// `timeoutMsForNonInteractive` set will succeed if it navigates to the redirect
// URL before the timeout.
IN_PROC_BROWSER_TEST_P(WebAuthFlowInBrowserTabParamBrowserTest,
                       OnAuthFlowRedirectBeforeTimeout) {
  const GURL auth_url = embedded_test_server()->GetURL("/title1.html");

  content::TestNavigationObserver navigation_observer(auth_url);
  navigation_observer.StartWatchingNewWebContents();

  // The delegate method OnAuthFlowURLChange should be called
  // by DidStartNavigation.
  EXPECT_CALL(mock(), OnAuthFlowURLChange(auth_url));
  // In SILENT mode, DidStopLoading() will wait for our specified 50ms timeout
  // before calling OnAuthFlowFailure.
  EXPECT_CALL(mock(), OnAuthFlowFailure).Times(0);
  StartWebAuthFlow(auth_url, WebAuthFlow::LAUNCH_WEB_AUTH_FLOW,
                   WebAuthFlow::SILENT, /*profile=*/nullptr,
                   WebAuthFlow::AbortOnLoad::kNo,
                   /*timeout_for_non_interactive=*/base::Milliseconds(50));

  navigation_observer.Wait();
  testing::Mock::VerifyAndClearExpectations(&mock());

  // Increment the time by 40ms - nothing should happen.
  EXPECT_CALL(mock(), OnAuthFlowFailure).Times(0);
  timeout_task_runner()->FastForwardBy(base::Milliseconds(40));
  testing::Mock::VerifyAndClearExpectations(&mock());

  // Redirect after page load and check we get OnAuthFlowURLChange.
  const GURL redirect_url = embedded_test_server()->GetURL("/title2.html");
  EXPECT_CALL(mock(), OnAuthFlowURLChange(redirect_url));
  EXPECT_TRUE(JsRedirectToUrl(redirect_url));
}

// Tests that the loaded auth page can redirect multiple times and fails only
// after the timeout.
IN_PROC_BROWSER_TEST_P(WebAuthFlowInBrowserTabParamBrowserTest,
                       OnAuthFlowMultipleRedirects) {
  const GURL auth_url = embedded_test_server()->GetURL("/title1.html");

  content::TestNavigationObserver navigation_observer(auth_url);
  navigation_observer.StartWatchingNewWebContents();

  // The delegate method OnAuthFlowURLChange should be called
  // by DidStartNavigation.
  EXPECT_CALL(mock(), OnAuthFlowURLChange(auth_url));
  // In SILENT mode, DidStopLoading() will wait for our specified 50ms timeout
  // before calling OnAuthFlowFailure.
  EXPECT_CALL(mock(), OnAuthFlowFailure).Times(0);
  StartWebAuthFlow(auth_url, WebAuthFlow::LAUNCH_WEB_AUTH_FLOW,
                   WebAuthFlow::SILENT, /*profile=*/nullptr,
                   WebAuthFlow::AbortOnLoad::kNo,
                   /*timeout_for_non_interactive=*/base::Milliseconds(50));

  navigation_observer.Wait();
  testing::Mock::VerifyAndClearExpectations(&mock());

  // Increment the time by 10ms - nothing should happen.
  EXPECT_CALL(mock(), OnAuthFlowFailure).Times(0);
  timeout_task_runner()->FastForwardBy(base::Milliseconds(10));
  testing::Mock::VerifyAndClearExpectations(&mock());

  // Redirect after page load and check we get OnAuthFlowURLChange.
  const GURL redirect_url = embedded_test_server()->GetURL("/title2.html");
  EXPECT_CALL(mock(), OnAuthFlowURLChange(redirect_url));
  EXPECT_TRUE(JsRedirectToUrl(redirect_url));

  // Increment the time by 10ms - nothing should happen.
  EXPECT_CALL(mock(), OnAuthFlowFailure).Times(0);
  timeout_task_runner()->FastForwardBy(base::Milliseconds(10));
  testing::Mock::VerifyAndClearExpectations(&mock());

  // Redirect after 2nd page load and check we get OnAuthFlowURLChange.
  const GURL redirect_url2 = embedded_test_server()->GetURL("/title3.html");
  EXPECT_CALL(mock(), OnAuthFlowURLChange(redirect_url2));
  EXPECT_TRUE(JsRedirectToUrl(redirect_url2));
  testing::Mock::VerifyAndClearExpectations(&mock());

  // Increment the time by 10ms - nothing should happen.
  EXPECT_CALL(mock(), OnAuthFlowFailure).Times(0);
  timeout_task_runner()->FastForwardBy(base::Milliseconds(10));
  testing::Mock::VerifyAndClearExpectations(&mock());

  // Now we exceed our 50ms limit and expect a failure.
  EXPECT_CALL(mock(), OnAuthFlowFailure(WebAuthFlow::INTERACTION_REQUIRED));
  timeout_task_runner()->FastForwardBy(base::Milliseconds(30));
}

INSTANTIATE_TEST_SUITE_P(
    ,
    WebAuthFlowInBrowserTabParamBrowserTest,
    testing::Bool(),
    [](const testing::TestParamInfo<
        WebAuthFlowInBrowserTabParamBrowserTest::ParamType>& info) {
      return base::StrCat(
          {info.param ? "With" : "Without", "WebAuthFlowInBrowserTab"});
    });

class WebAuthFlowGuestPartitionParamTest
    : public WebAuthFlowBrowserTest,
      public testing::WithParamInterface<
          std::tuple<bool, WebAuthFlow::Partition>> {
 public:
  WebAuthFlowGuestPartitionParamTest() {
    std::vector<base::test::FeatureRef> enabled_features;
    std::vector<base::test::FeatureRef> disabled_features;

    persist_storage_feature_enabled()
        ? enabled_features.push_back(kPersistentStorageForWebAuthFlow)
        : disabled_features.push_back(kPersistentStorageForWebAuthFlow);

    // Explicitly disable the `kWebAuthFlowInBrowserTab` feature as it is
    // incompatible with the Guest Partition tests and
    // `kPersistentStorageForWebAuthFlow`.
    disabled_features.push_back(features::kWebAuthFlowInBrowserTab);

    scoped_feature_list_.InitWithFeatures(enabled_features, disabled_features);
  }

  bool persist_storage_feature_enabled() { return std::get<0>(GetParam()); }

  WebAuthFlow::Partition partition() { return std::get<1>(GetParam()); }

  void LoadWebAuthFlow() {
    const GURL auth_url = embedded_test_server()->GetURL("/title1.html");

    // Observer for waiting until a navigation to a url has finished.
    content::TestNavigationObserver navigation_observer(auth_url);
    navigation_observer.StartWatchingNewWebContents();

    StartWebAuthFlow(auth_url, partition());
    EXPECT_CALL(mock(), OnAuthFlowURLChange(auth_url));

    navigation_observer.WaitForNavigationFinished();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Tests that the partition returned by `WebAuthFlow::GetGuestPartition()`
// matches the one used by the webview.
IN_PROC_BROWSER_TEST_P(WebAuthFlowGuestPartitionParamTest, GetGuestPartition) {
  LoadWebAuthFlow();

  // Set a test cookie on the page.
  ASSERT_TRUE(
      content::ExecJs(web_contents(), "document.cookie = \"testCookie=1\""));

  // Verify that the cookie was added to the guest partition.
  base::test::TestFuture<const net::CookieList&> get_cookies_future;
  web_auth_flow()
      ->GetGuestPartition()
      ->GetCookieManagerForBrowserProcess()
      ->GetAllCookies(get_cookies_future.GetCallback());
  const net::CookieList cookies = get_cookies_future.Get();
  ASSERT_EQ(1u, cookies.size());
  EXPECT_EQ("testCookie", cookies[0].Name());
  EXPECT_EQ("1", cookies[0].Value());
}

IN_PROC_BROWSER_TEST_P(WebAuthFlowGuestPartitionParamTest,
                       PRE_PersistenceTest) {
  LoadWebAuthFlow();
  // Set a test cookie on the page.
  ASSERT_TRUE(content::ExecJs(
      web_contents(), "document.cookie = \"testCookie=1; max-age=3600\""));
}

IN_PROC_BROWSER_TEST_P(WebAuthFlowGuestPartitionParamTest, PersistenceTest) {
  LoadWebAuthFlow();

  base::test::TestFuture<const net::CookieList&> get_cookies_future;
  web_auth_flow()
      ->GetGuestPartition()
      ->GetCookieManagerForBrowserProcess()
      ->GetAllCookies(get_cookies_future.GetCallback());
  const net::CookieList cookies = get_cookies_future.Get();

  // Verify that the cookie set in the previous test is persisted for the
  // webAuthFlow if the feature is enabled.
  // Read from the cookie store directly rather than execute a script on the
  // auth page because the page URL changes between test (test server doesn't
  // have a fixed port).
  if (persist_storage_feature_enabled() &&
      partition() == WebAuthFlow::LAUNCH_WEB_AUTH_FLOW) {
    ASSERT_EQ(1u, cookies.size());
    EXPECT_EQ("testCookie", cookies[0].Name());
    EXPECT_EQ("1", cookies[0].Value());
  } else {
    EXPECT_EQ(0u, cookies.size());
  }
}

INSTANTIATE_TEST_SUITE_P(
    ,
    WebAuthFlowGuestPartitionParamTest,
    testing::Combine(testing::Bool(),
                     testing::Values(WebAuthFlow::LAUNCH_WEB_AUTH_FLOW,
                                     WebAuthFlow::GET_AUTH_TOKEN)),
    [](const testing::TestParamInfo<
        WebAuthFlowGuestPartitionParamTest::ParamType>& info) {
      return base::StrCat(
          {std::get<0>(info.param) ? "FeatureOn" : "FeatureOff",
           std::get<1>(info.param) == WebAuthFlow::LAUNCH_WEB_AUTH_FLOW
               ? "WebAuthFlow"
               : "GetAuthToken"});
    });
class WebAuthFlowFencedFrameTest
    : public WebAuthFlowInBrowserTabParamBrowserTest {
 public:
  content::test::FencedFrameTestHelper& fenced_frame_test_helper() {
    return fenced_frame_helper_;
  }

 private:
  content::test::FencedFrameTestHelper fenced_frame_helper_;
};

IN_PROC_BROWSER_TEST_P(WebAuthFlowFencedFrameTest,
                       FencedFrameNavigationSuccess) {
  const GURL auth_url = embedded_test_server()->GetURL("/title1.html");

  // Observer for waiting until loading stops. A fenced frame will be created
  // after load has finished.
  content::TestNavigationObserver navigation_observer(auth_url);
  navigation_observer.set_wait_event(
      content::TestNavigationObserver::WaitEvent::kLoadStopped);
  navigation_observer.StartWatchingNewWebContents();

  EXPECT_CALL(mock(), OnAuthFlowURLChange(auth_url));
  StartWebAuthFlow(auth_url);

  navigation_observer.Wait();
  testing::Mock::VerifyAndClearExpectations(&mock());

  // Navigation for fenced frames should not affect to call the delegate methods
  // in the WebAuthFlow.
  EXPECT_CALL(mock(), OnAuthFlowURLChange(auth_url)).Times(0);

  // Create a fenced frame into the inner WebContents of the WebAuthFlow.
  ASSERT_TRUE(fenced_frame_test_helper().CreateFencedFrame(
      web_contents()->GetPrimaryMainFrame(),
      embedded_test_server()->GetURL("/fenced_frames/title1.html")));
}

IN_PROC_BROWSER_TEST_P(WebAuthFlowFencedFrameTest,
                       FencedFrameNavigationFailure) {
  const GURL auth_url = embedded_test_server()->GetURL("/title1.html");

  // Observer for waiting until loading stops. A fenced frame will be created
  // after load has finished.
  content::TestNavigationObserver navigation_observer(auth_url);
  navigation_observer.set_wait_event(
      content::TestNavigationObserver::WaitEvent::kLoadStopped);
  navigation_observer.StartWatchingNewWebContents();

  EXPECT_CALL(mock(), OnAuthFlowURLChange(auth_url));
  StartWebAuthFlow(auth_url);

  navigation_observer.Wait();
  testing::Mock::VerifyAndClearExpectations(&mock());

  // Navigation for fenced frames should not affect to call the delegate methods
  // in the WebAuthFlow.
  EXPECT_CALL(mock(), OnAuthFlowURLChange(auth_url)).Times(0);
  EXPECT_CALL(mock(), OnAuthFlowFailure).Times(0);

  // Create a fenced frame into the inner WebContents of the WebAuthFlow.
  ASSERT_TRUE(fenced_frame_test_helper().CreateFencedFrame(
      web_contents()->GetPrimaryMainFrame(),
      embedded_test_server()->GetURL("/error"), net::Error::ERR_FAILED));
}

INSTANTIATE_TEST_SUITE_P(,
                         WebAuthFlowFencedFrameTest,
                         testing::Bool(),
                         [](const testing::TestParamInfo<
                             WebAuthFlowFencedFrameTest::ParamType>& info) {
                           return base::StrCat({info.param ? "With" : "Without",
                                                "WebAuthFlowInBrowserTab"});
                         });

class WebAuthFlowWithBrowserTabBrowserTest : public WebAuthFlowBrowserTest {
 public:
  WebAuthFlowWithBrowserTabBrowserTest() {
    // By default the feature param is {{"browser_tab_mode", "new_tab"}}.
    scoped_feature_list_.InitAndEnableFeature(
        features::kWebAuthFlowInBrowserTab);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// This test is in two parts:
// - First create a WebAuthFlow in interactive mode that will create a new tab
// with the auth_url.
// - Close the new created tab, simulating the user declining the consent by
// closing the tab.
//
// These two tests are combined into one in order not to re-test the tab
// creation twice.
IN_PROC_BROWSER_TEST_F(WebAuthFlowWithBrowserTabBrowserTest,
                       InteractiveNewTabCreatedWithAuthURL_ThenCloseTab) {
  TabStripModel* tabs = browser()->tab_strip_model();
  int initial_tab_count = tabs->count();

  const GURL auth_url = embedded_test_server()->GetURL("/title1.html");
  content::TestNavigationObserver navigation_observer(auth_url);
  navigation_observer.StartWatchingNewWebContents();

  EXPECT_CALL(mock(), OnAuthFlowURLChange(auth_url));
  StartWebAuthFlow(auth_url, WebAuthFlow::Partition::LAUNCH_WEB_AUTH_FLOW,
                   WebAuthFlow::Mode::INTERACTIVE);

  const char extension_name[] = "extension_name";
  web_auth_flow()->SetShouldShowInfoBar(extension_name);

  navigation_observer.Wait();

  EXPECT_EQ(tabs->count(), initial_tab_count + 1);
  EXPECT_EQ(tabs->GetActiveWebContents()->GetLastCommittedURL(), auth_url);

  // Check info bar exists and displays proper message with extension name.
  base::WeakPtr<WebAuthFlowInfoBarDelegate> infobar_delegate =
      web_auth_flow()->GetInfoBarDelegateForTesting();
  EXPECT_TRUE(infobar_delegate);
  EXPECT_EQ(
      infobar_delegate->GetIdentifier(),
      infobars::InfoBarDelegate::EXTENSIONS_WEB_AUTH_FLOW_INFOBAR_DELEGATE);
  EXPECT_TRUE(infobar_delegate->GetMessageText().find(
      base::UTF8ToUTF16(std::string(extension_name))));

  //---------------------------------------------------------------------
  // Part of the test that closes the tab, simulating declining the consent.
  //---------------------------------------------------------------------
  EXPECT_CALL(mock(), OnAuthFlowFailure(WebAuthFlow::Failure::WINDOW_CLOSED));
  tabs->CloseWebContentsAt(tabs->active_index(), 0);
}

IN_PROC_BROWSER_TEST_F(
    WebAuthFlowWithBrowserTabBrowserTest,
    InteractiveNewTabCreatedWithAuthURL_ThenChangeURLBeforeAuthResult) {
  TabStripModel* tabs = browser()->tab_strip_model();
  int initial_tab_count = tabs->count();

  const GURL auth_url = embedded_test_server()->GetURL("/title1.html");
  content::TestNavigationObserver navigation_observer(auth_url);
  navigation_observer.StartWatchingNewWebContents();

  EXPECT_CALL(mock(), OnAuthFlowURLChange(auth_url));
  StartWebAuthFlow(auth_url, WebAuthFlow::Partition::LAUNCH_WEB_AUTH_FLOW,
                   WebAuthFlow::Mode::INTERACTIVE);
  web_auth_flow()->SetShouldShowInfoBar("extension name");

  navigation_observer.Wait();

  //---------------------------------------------------------------------
  // Browser-initiated URL change in the opened tab before completing the auth
  // flow should trigger an auth flow failure.
  //---------------------------------------------------------------------
  testing::Mock::VerifyAndClearExpectations(&mock());

  // Keeping a reference to the info bar delegate to check later.
  base::WeakPtr<WebAuthFlowInfoBarDelegate> auth_info_bar =
      web_auth_flow()->GetInfoBarDelegateForTesting();
  ASSERT_TRUE(auth_info_bar);

  GURL new_url = embedded_test_server()->GetURL("a.com", "/new.html");
  EXPECT_CALL(mock(),
              OnAuthFlowFailure(WebAuthFlow::Failure::USER_NAVIGATED_AWAY));

  content::TestNavigationObserver web_contents_observer(web_contents());
  content::NavigationController::LoadURLParams load_params(new_url);
  load_params.is_renderer_initiated = false;
  web_contents()->GetController().LoadURLWithParams(load_params);
  web_contents_observer.Wait();

  // New tab is not expected to be closed, it is now used for navigation and not
  // part of the flow anymore.
  EXPECT_EQ(web_contents(), nullptr);
  EXPECT_EQ(tabs->count(), initial_tab_count + 1);
  EXPECT_EQ(tabs->GetActiveWebContents()->GetLastCommittedURL(), new_url);
  // Infobar should be closed on navigation.
  EXPECT_FALSE(auth_info_bar);
}

IN_PROC_BROWSER_TEST_F(WebAuthFlowWithBrowserTabBrowserTest,
                       InteractiveNoBrowser_WebAuthCreatesBrowserWithTab) {
  Profile* profile = browser()->profile();
  // Simulates an extension being opened, in order for the profile not to be
  // added for destruction.
  ScopedProfileKeepAlive profile_keep_alive(
      profile, ProfileKeepAliveOrigin::kBackgroundMode);
  ScopedKeepAlive keep_alive{KeepAliveOrigin::BROWSER,
                             KeepAliveRestartOption::DISABLED};
  CloseBrowserSynchronously(browser());
  ASSERT_FALSE(chrome::FindBrowserWithProfile(profile));

  const GURL auth_url = embedded_test_server()->GetURL("/title1.html");
  content::TestNavigationObserver navigation_observer(auth_url);
  navigation_observer.StartWatchingNewWebContents();

  EXPECT_CALL(mock(), OnAuthFlowURLChange(auth_url));
  StartWebAuthFlow(auth_url, WebAuthFlow::Partition::LAUNCH_WEB_AUTH_FLOW,
                   WebAuthFlow::Mode::INTERACTIVE, profile);

  navigation_observer.Wait();

  Browser* new_browser = chrome::FindBrowserWithProfile(profile);
  EXPECT_TRUE(new_browser);
  EXPECT_EQ(new_browser->tab_strip_model()
                ->GetActiveWebContents()
                ->GetLastCommittedURL(),
            auth_url);
}

IN_PROC_BROWSER_TEST_F(WebAuthFlowWithBrowserTabBrowserTest,
                       SilentNewTabNotCreated) {
  TabStripModel* tabs = browser()->tab_strip_model();
  int initial_tab_count = tabs->count();

  const GURL auth_url = embedded_test_server()->GetURL("/title1.html");
  content::TestNavigationObserver navigation_observer(auth_url);
  navigation_observer.StartWatchingNewWebContents();

  EXPECT_CALL(mock(),
              OnAuthFlowFailure(WebAuthFlow::Failure::INTERACTION_REQUIRED));
  EXPECT_CALL(mock(), OnAuthFlowURLChange(auth_url));
  StartWebAuthFlow(auth_url, WebAuthFlow::Partition::LAUNCH_WEB_AUTH_FLOW,
                   WebAuthFlow::Mode::SILENT);

  navigation_observer.Wait();

  // Tab not created, tab count did not increase.
  EXPECT_EQ(tabs->count(), initial_tab_count);
}

IN_PROC_BROWSER_TEST_F(WebAuthFlowWithBrowserTabBrowserTest,
                       InteractiveNewTabCreatedWithAuthURL_NoInfoBarByDefault) {
  TabStripModel* tabs = browser()->tab_strip_model();
  int initial_tab_count = tabs->count();

  const GURL auth_url = embedded_test_server()->GetURL("/title1.html");
  content::TestNavigationObserver navigation_observer(auth_url);
  navigation_observer.StartWatchingNewWebContents();

  EXPECT_CALL(mock(), OnAuthFlowURLChange(auth_url));
  StartWebAuthFlow(auth_url, WebAuthFlow::Partition::GET_AUTH_TOKEN,
                   WebAuthFlow::Mode::INTERACTIVE);

  navigation_observer.Wait();

  EXPECT_EQ(tabs->count(), initial_tab_count + 1);
  EXPECT_EQ(tabs->GetActiveWebContents()->GetLastCommittedURL(), auth_url);

  // Check info bar is not created if not set via
  // `SetShouldShowInfoBar())`.
  base::WeakPtr<WebAuthFlowInfoBarDelegate> infobar_delegate =
      web_auth_flow()->GetInfoBarDelegateForTesting();
  EXPECT_FALSE(infobar_delegate);
}

class WebAuthFlowWithBrowserTabInPopupWindowBrowserTest
    : public WebAuthFlowBrowserTest {
 public:
  WebAuthFlowWithBrowserTabInPopupWindowBrowserTest() {
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        features::kWebAuthFlowInBrowserTab,
        {{"browser_tab_mode", "popup_window"}});
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(WebAuthFlowWithBrowserTabInPopupWindowBrowserTest,
                       PopupWindowOpened_ThenCloseWindow) {
  size_t initial_browser_count = chrome::GetTotalBrowserCount();

  const GURL auth_url = embedded_test_server()->GetURL("/title1.html");
  content::TestNavigationObserver navigation_observer(auth_url);
  navigation_observer.StartWatchingNewWebContents();

  EXPECT_CALL(mock(), OnAuthFlowURLChange(auth_url));
  StartWebAuthFlow(auth_url, WebAuthFlow::Partition::LAUNCH_WEB_AUTH_FLOW,
                   WebAuthFlow::Mode::INTERACTIVE);

  navigation_observer.Wait();

  // New popup window is a browser, browser count should increment by 1.
  EXPECT_EQ(chrome::GetTotalBrowserCount(), initial_browser_count + 1);

  // Retrieve the browser used in the WebAuthFlow, the popup window.
  Browser* popup_window_browser =
      chrome::FindBrowserWithWebContents(web_contents());
  EXPECT_NE(popup_window_browser, browser());

  TabStripModel* popup_tabs = popup_window_browser->tab_strip_model();
  EXPECT_EQ(popup_tabs->count(), 1);
  EXPECT_EQ(popup_tabs->GetActiveWebContents()->GetLastCommittedURL(),
            auth_url);

  //---------------------------------------------------------------------
  // Closing the browser popup window, simulating declining the consent.
  //---------------------------------------------------------------------
  EXPECT_CALL(mock(), OnAuthFlowFailure(WebAuthFlow::Failure::WINDOW_CLOSED));
  CloseBrowserSynchronously(popup_window_browser);
}

}  //  namespace extensions
