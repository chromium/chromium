// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/webrtc/same_origin_observer.h"

#include "base/test/mock_callback.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/fenced_frame_test_util.h"
#include "net/dns/mock_host_resolver.h"

class SameOriginObserverTest : public InProcessBrowserTest {
 public:
  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    InProcessBrowserTest::SetUpOnMainThread();
  }

  void NavigateAndCreateObserver(const std::string& hostname,
                                 const std::string& relative_url) {
    ASSERT_TRUE(ui_test_utils::NavigateToURL(
        browser(), embedded_test_server()->GetURL(hostname, relative_url)));

    same_origin_observer_ = std::make_unique<SameOriginObserver>(
        web_contents(), embedded_test_server()->GetOrigin(hostname),
        origin_state_callback_.Get());
  }

  content::WebContents* web_contents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

 protected:
  using OriginCallback = base::RepeatingCallback<void(content::WebContents*)>;
  base::MockCallback<OriginCallback> origin_state_callback_;
  std::unique_ptr<SameOriginObserver> same_origin_observer_;
};

IN_PROC_BROWSER_TEST_F(SameOriginObserverTest, CallCallbackWhenOriginChanged) {
  ASSERT_TRUE(embedded_test_server()->Start());

  // Navigate to the initial page. The observer refers to its origin.
  NavigateAndCreateObserver("a.com", "/title1.html");

  // Navigate to the second page with the same origin. The observer won't call
  // the callback because the origin was not changed.
  EXPECT_CALL(origin_state_callback_, Run).Times(0);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("a.com", "/title2.html")));
  testing::Mock::VerifyAndClearExpectations(&origin_state_callback_);

  // Navigate to the third page with the different origin. The observer has to
  // call the callback.
  EXPECT_CALL(origin_state_callback_, Run);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("b.com", "/title1.html")));
  testing::Mock::VerifyAndClearExpectations(&origin_state_callback_);
}

class SameOriginObserverFencedFrameTest : public SameOriginObserverTest {
 public:
  content::test::FencedFrameTestHelper& fenced_frame_test_helper() {
    return fenced_frame_helper_;
  }

 private:
  content::test::FencedFrameTestHelper fenced_frame_helper_;
};

IN_PROC_BROWSER_TEST_F(SameOriginObserverFencedFrameTest,
                       FFDoesNotAffectSameOriginState) {
  ASSERT_TRUE(embedded_test_server()->Start());

  // Navigate to the initial page. The observer refers to its origin.
  NavigateAndCreateObserver("a.com", "/title1.html");

  // The same-origin state callback is not called by fenced frames.
  EXPECT_CALL(origin_state_callback_, Run).Times(0);
  ASSERT_TRUE(fenced_frame_test_helper().CreateFencedFrame(
      web_contents()->GetPrimaryMainFrame(),
      embedded_test_server()->GetURL("b.com", "/fenced_frames/title1.html")));
  testing::Mock::VerifyAndClearExpectations(&origin_state_callback_);
}
