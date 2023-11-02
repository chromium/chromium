// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/media_session.h"
#include "content/public/test/browser_test.h"
#include "services/media_session/public/cpp/test/mock_media_session.h"

class MediaSessionLacrosBrowserTest : public InProcessBrowserTest {
 protected:
  MediaSessionLacrosBrowserTest() = default;

  MediaSessionLacrosBrowserTest(const MediaSessionLacrosBrowserTest&) = delete;
  MediaSessionLacrosBrowserTest& operator=(
      const MediaSessionLacrosBrowserTest&) = delete;

  ~MediaSessionLacrosBrowserTest() override = default;
};

// This test checks that a Media Session can become active which can only
// happen if the Media Session Service is working correctly.
IN_PROC_BROWSER_TEST_F(MediaSessionLacrosBrowserTest, CheckServiceWorks) {
  ASSERT_TRUE(embedded_test_server()->Start());

  // Navigate to a test page with some media on it.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(),
      embedded_test_server()->GetURL("/media/autoplay_iframe.html")));

  auto* web_contents = browser()->tab_strip_model()->GetActiveWebContents();

  media_session::test::MockMediaSessionMojoObserver observer(
      *content::MediaSession::Get(web_contents));

  // Start playback.
  ASSERT_EQ(
      nullptr,
      content::EvalJs(web_contents, "document.getElementById('video').play()"));

  // Wait for the session to become active.
  observer.WaitForState(
      media_session::mojom::MediaSessionInfo::SessionState::kActive);
}
