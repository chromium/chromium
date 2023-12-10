// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/speech/chrome_speech_recognition_manager_delegate.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/fake_speech_recognition_manager.h"
#include "net/test/embedded_test_server/embedded_test_server.h"

using content::FakeSpeechRecognitionManager;
using content::WebContents;

namespace speech {

class ChromeSpeechRecognitionTest : public InProcessBrowserTest {
 public:
  ChromeSpeechRecognitionTest() {}

  ChromeSpeechRecognitionTest(const ChromeSpeechRecognitionTest&) = delete;
  ChromeSpeechRecognitionTest& operator=(const ChromeSpeechRecognitionTest&) =
      delete;

  ~ChromeSpeechRecognitionTest() override {}

  void SetUp() override {
    // SpeechRecognition test specific SetUp.
    fake_speech_recognition_manager_.set_should_send_fake_response(true);
    // Inject the fake manager factory so that the test result is returned to
    // the web page.
    content::SpeechRecognitionManager::SetManagerForTesting(
        &fake_speech_recognition_manager_);

    InProcessBrowserTest::SetUp();
  }

  void TearDown() override {
    content::SpeechRecognitionManager::SetManagerForTesting(nullptr);
    fake_speech_recognition_manager_.SetDelegate(nullptr);
    InProcessBrowserTest::TearDown();
  }

 protected:
  ChromeSpeechRecognitionManagerDelegate delegate_;
  content::FakeSpeechRecognitionManager fake_speech_recognition_manager_;
};

class SpeechWebContentsObserver : public content::WebContentsObserver {
 public:
  explicit SpeechWebContentsObserver(WebContents* web_contents)
      : WebContentsObserver(web_contents),
        render_frame_host_changed_(false),
        web_contents_destroyed_(false) {}

  SpeechWebContentsObserver(const SpeechWebContentsObserver&) = delete;
  SpeechWebContentsObserver& operator=(const SpeechWebContentsObserver&) =
      delete;

  ~SpeechWebContentsObserver() override {}

  // content::WebContentsObserver overrides.
  void RenderFrameHostChanged(content::RenderFrameHost* old_host,
                              content::RenderFrameHost* new_host) override {
    render_frame_host_changed_ = true;
  }
  void WebContentsDestroyed() override { web_contents_destroyed_ = true; }

  bool web_contents_destroyed() { return web_contents_destroyed_; }
  bool render_frame_host_changed() { return render_frame_host_changed_; }

 private:
  bool render_frame_host_changed_;
  bool web_contents_destroyed_;
};

// Tests that ChromeSpeechRecognitionManagerDelegate works properly
// when a WebContents goes away (WCO::WebContentsDestroyed) or the RFH
// changes within a WebContents (WCO::RenderFrameHostChanged).
IN_PROC_BROWSER_TEST_F(ChromeSpeechRecognitionTest, BasicTearDown) {
  ASSERT_TRUE(embedded_test_server()->Start());
  net::EmbeddedTestServer https_server(net::EmbeddedTestServer::TYPE_HTTPS);
  https_server.ServeFilesFromSourceDirectory(GetChromeTestDataDir());
  ASSERT_TRUE(https_server.Start());

  GURL http_url =
      embedded_test_server()->GetURL("/speech/web_speech_test.html");
  GURL https_url(https_server.GetURL("/speech/web_speech_test.html"));

  fake_speech_recognition_manager_.SetDelegate(&delegate_);

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), http_url));
  WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_TRUE(web_contents);
  SpeechWebContentsObserver speech_contents_observer(web_contents);

  std::u16string success_title(u"PASS");
  std::u16string failure_title(u"FAIL");

  const char kRetriveTranscriptScript[] = "window.getFirstTranscript()";
  const char kExpectedTranscript[] = "Pictures of the moon";

  {
    content::TitleWatcher title_watcher(web_contents, success_title);
    title_watcher.AlsoWaitForTitle(failure_title);
    EXPECT_TRUE(content::ExecJs(web_contents, "testSpeechRecognition()"));
    EXPECT_EQ(success_title, title_watcher.WaitAndGetTitle());

    EXPECT_EQ(kExpectedTranscript,
              content::EvalJs(web_contents, kRetriveTranscriptScript));
  }

  // Navigating to an https page will force RFH change within
  // |web_contents|, results in WCO::RenderFrameHostChanged().
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), https_url));

  EXPECT_TRUE(speech_contents_observer.render_frame_host_changed());

  {
    content::TitleWatcher title_watcher(web_contents, success_title);
    title_watcher.AlsoWaitForTitle(failure_title);
    EXPECT_TRUE(content::ExecJs(web_contents, "testSpeechRecognition()"));
    EXPECT_EQ(success_title, title_watcher.WaitAndGetTitle());

    EXPECT_EQ(kExpectedTranscript,
              content::EvalJs(web_contents, kRetriveTranscriptScript));
  }

  // Close the tab to so that we see WCO::WebContentsDestroyed().
  chrome::CloseTab(browser());
  EXPECT_TRUE(speech_contents_observer.web_contents_destroyed());
}

}  // namespace speech
