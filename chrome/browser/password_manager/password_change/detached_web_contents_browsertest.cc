// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/password_change/detached_web_contents.h"

#include "base/test/test_timeouts.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "url/gurl.h"

namespace {

class DetachedWebContentsBrowserTest : public InProcessBrowserTest {
 public:
  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    ASSERT_TRUE(embedded_test_server()->Start());
  }
};

// Regression test for crbug.com/438702587: a `WebContents` that isn't attached
// to a widget never produces frames, so `requestAnimationFrame()` callbacks
// never run and sites waiting on them stall. `DetachedWebContents` must keep
// frames flowing even though it is never shown to the user.
IN_PROC_BROWSER_TEST_F(DetachedWebContentsBrowserTest,
                       RequestAnimationFrameRuns) {
  DetachedWebContents detached_web_contents(
      GetProfile(), embedded_test_server()->GetURL("/title1.html"));
  content::WebContents* web_contents = detached_web_contents.GetWebContents();
  ASSERT_TRUE(content::WaitForLoadStop(web_contents));

  // Waits for several consecutive animation frames, so a single stray frame is
  // not enough to pass. Resolves with "timeout" if frames don't arrive in time,
  // so a regression fails cleanly instead of hanging until the test times out.
  static constexpr char kWaitForAnimationFramesScript[] = R"(
    new Promise(resolve => {
      let framesLeft = 3;
      const onFrame = () => {
        if (--framesLeft === 0) {
          resolve('raf');
          return;
        }
        requestAnimationFrame(onFrame);
      };
      requestAnimationFrame(onFrame);
      setTimeout(() => resolve('timeout'), $1);
    })
  )";

  const double timeout_ms = TestTimeouts::action_timeout().InMillisecondsF();
  EXPECT_EQ("raf",
            content::EvalJs(
                web_contents,
                content::JsReplace(kWaitForAnimationFramesScript, timeout_ms)));
}

}  // namespace
