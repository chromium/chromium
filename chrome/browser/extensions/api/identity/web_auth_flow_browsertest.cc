// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/identity/web_auth_flow.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/fenced_frame_test_util.h"
#include "content/public/test/test_navigation_observer.h"
#include "testing/gmock/include/gmock/gmock.h"

class MockWebAuthFlowDelegate : public extensions::WebAuthFlow::Delegate {
 public:
  MOCK_METHOD(void, OnAuthFlowURLChange, (const GURL&), (override));
  MOCK_METHOD(void, OnAuthFlowTitleChange, (const std::string&), (override));
  MOCK_METHOD(void,
              OnAuthFlowFailure,
              (extensions::WebAuthFlow::Failure),
              (override));
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

  void StartWebAuthFlow(const GURL& url) {
    web_auth_flow_ = std::make_unique<extensions::WebAuthFlow>(
        &mock_web_auth_flow_delegate_, browser()->profile(), url,
        extensions::WebAuthFlow::INTERACTIVE,
        extensions::WebAuthFlow::LAUNCH_WEB_AUTH_FLOW);
    web_auth_flow_->Start();
  }

  content::WebContents* web_contents() {
    DCHECK(web_auth_flow_);
    return web_auth_flow_->web_contents();
  }

  MockWebAuthFlowDelegate& mock() { return mock_web_auth_flow_delegate_; }

 private:
  std::unique_ptr<extensions::WebAuthFlow> web_auth_flow_;
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
  EXPECT_CALL(mock(), OnAuthFlowFailure(extensions::WebAuthFlow::LOAD_FAILED));

  navigation_observer.WaitForNavigationFinished();
}

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
      web_contents()->GetMainFrame(),
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
      web_contents()->GetMainFrame(), embedded_test_server()->GetURL("/error"),
      net::Error::ERR_FAILED));
}
