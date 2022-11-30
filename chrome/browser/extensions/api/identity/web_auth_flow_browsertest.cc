// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/identity/web_auth_flow.h"

#include "base/strings/strcat.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/storage_partition.h"
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
  }

  void TearDownOnMainThread() override {
    // Delete the web auth flow (uses DeleteSoon).
    web_auth_flow_.release()->DetachDelegateAndDelete();
    base::RunLoop().RunUntilIdle();
    InProcessBrowserTest::TearDownOnMainThread();
  }

  void StartWebAuthFlow(
      const GURL& url,
      WebAuthFlow::Partition partition = WebAuthFlow::LAUNCH_WEB_AUTH_FLOW) {
    web_auth_flow_ = std::make_unique<WebAuthFlow>(
        &mock_web_auth_flow_delegate_, browser()->profile(), url,
        WebAuthFlow::INTERACTIVE, partition);
    web_auth_flow_->Start();
  }

  WebAuthFlow* web_auth_flow() { return web_auth_flow_.get(); }

  content::WebContents* web_contents() {
    DCHECK(web_auth_flow_);
    return web_auth_flow_->web_contents();
  }

  MockWebAuthFlowDelegate& mock() { return mock_web_auth_flow_delegate_; }

 private:
  std::unique_ptr<WebAuthFlow> web_auth_flow_;
  MockWebAuthFlowDelegate mock_web_auth_flow_delegate_;
};

IN_PROC_BROWSER_TEST_F(WebAuthFlowBrowserTest, OnAuthFlowURLChangeCalled) {
  const GURL auth_url = embedded_test_server()->GetURL("/title1.html");

  // Observer for waiting until a navigation to a url has finished.
  content::TestNavigationObserver navigation_observer(auth_url);
  navigation_observer.StartWatchingNewWebContents();

  StartWebAuthFlow(auth_url);
  // The delegate method OnAuthFlowURLChange should be called
  // by DidStartNavigation.
  EXPECT_CALL(mock(), OnAuthFlowURLChange(auth_url));

  navigation_observer.WaitForNavigationFinished();
}

IN_PROC_BROWSER_TEST_F(WebAuthFlowBrowserTest, OnAuthFlowFailureChangeCalled) {
  // Navigate to a url that doesn't exist.
  const GURL error_url = embedded_test_server()->GetURL("/error");

  content::TestNavigationObserver navigation_observer(error_url);
  navigation_observer.StartWatchingNewWebContents();

  StartWebAuthFlow(error_url);
  // The delegate method OnAuthFlowFailure should be called
  // by DidFinishNavigation.
  EXPECT_CALL(mock(), OnAuthFlowFailure(WebAuthFlow::LOAD_FAILED));

  navigation_observer.WaitForNavigationFinished();
}

class WebAuthFlowGuestPartitionParamTest
    : public WebAuthFlowBrowserTest,
      public testing::WithParamInterface<
          std::tuple<bool, WebAuthFlow::Partition>> {
 public:
  WebAuthFlowGuestPartitionParamTest() {
    if (feature_enabled()) {
      scoped_feature_list_.InitAndEnableFeature(
          kPersistentStorageForWebAuthFlow);
    } else {
      scoped_feature_list_.InitAndDisableFeature(
          kPersistentStorageForWebAuthFlow);
    }
  }

  bool feature_enabled() { return std::get<0>(GetParam()); }

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
  if (feature_enabled() && partition() == WebAuthFlow::LAUNCH_WEB_AUTH_FLOW) {
    ASSERT_EQ(1u, cookies.size());
    EXPECT_EQ("testCookie", cookies[0].Name());
    EXPECT_EQ("1", cookies[0].Value());
  } else {
    EXPECT_EQ(0u, cookies.size());
  }
}

INSTANTIATE_TEST_CASE_P(
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
class WebAuthFlowFencedFrameTest : public WebAuthFlowBrowserTest {
 public:
  content::test::FencedFrameTestHelper& fenced_frame_test_helper() {
    return fenced_frame_helper_;
  }

 private:
  content::test::FencedFrameTestHelper fenced_frame_helper_;
};

IN_PROC_BROWSER_TEST_F(WebAuthFlowFencedFrameTest,
                       FencedFrameNavigationSuccess) {
  const GURL auth_url = embedded_test_server()->GetURL("/title1.html");

  // Observer for waiting until loading stops. A fenced frame will be created
  // after load has finished.
  content::TestNavigationObserver navigation_observer(auth_url);
  navigation_observer.set_wait_event(
      content::TestNavigationObserver::WaitEvent::kLoadStopped);
  navigation_observer.StartWatchingNewWebContents();

  StartWebAuthFlow(auth_url);

  EXPECT_CALL(mock(), OnAuthFlowURLChange(auth_url));
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

IN_PROC_BROWSER_TEST_F(WebAuthFlowFencedFrameTest,
                       FencedFrameNavigationFailure) {
  const GURL auth_url = embedded_test_server()->GetURL("/title1.html");

  // Observer for waiting until loading stops. A fenced frame will be created
  // after load has finished.
  content::TestNavigationObserver navigation_observer(auth_url);
  navigation_observer.set_wait_event(
      content::TestNavigationObserver::WaitEvent::kLoadStopped);
  navigation_observer.StartWatchingNewWebContents();

  StartWebAuthFlow(auth_url);

  EXPECT_CALL(mock(), OnAuthFlowURLChange(auth_url));
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

}  //  namespace extensions
