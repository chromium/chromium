// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/macros.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/speech/chrome_speech_recognition_manager_delegate.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/fake_speech_recognition_manager.h"
#include "net/test/embedded_test_server/embedded_test_server.h"

using content::FakeSpeechRecognitionManager;
using content::WebContents;

namespace speech {

class ChromeSpeechRecognitionTest : public InProcessBrowserTest {
 public:
  ChromeSpeechRecognitionTest() {}
  ~ChromeSpeechRecognitionTest() override {}

  void SetUp() override {
    // SpeechRecognition test specific SetUp.
    fake_speech_recognition_manager_.reset(
        new content::FakeSpeechRecognitionManager());
    fake_speech_recognition_manager_->set_should_send_fake_response(true);
    // Inject the fake manager factory so that the test result is returned to
    // the web page.
    content::SpeechRecognitionManager::SetManagerForTesting(
        fake_speech_recognition_manager_.get());

    InProcessBrowserTest::SetUp();
  }

  void TearDown() override {
    content::SpeechRecognitionManager::SetManagerForTesting(NULL);
    fake_speech_recognition_manager_->SetDelegate(NULL);
    InProcessBrowserTest::TearDown();
  }

 protected:
  std::unique_ptr<content::FakeSpeechRecognitionManager>
      fake_speech_recognition_manager_;

 private:
  DISALLOW_COPY_AND_ASSIGN(ChromeSpeechRecognitionTest);
};

class SpeechWebContentsObserver : public content::WebContentsObserver {
 public:
  explicit SpeechWebContentsObserver(WebContents* web_contents)
      : WebContentsObserver(web_contents),
        render_view_host_changed_(false),
        web_contents_destroyed_(false) {}
  ~SpeechWebContentsObserver() override {}

  // content::WebContentsObserver overrides.
  void RenderViewHostChanged(content::RenderViewHost* old_host,
                             content::RenderViewHost* new_host) override {
    render_view_host_changed_ = true;
  }
  void WebContentsDestroyed() override { web_contents_destroyed_ = true; }

  bool web_contents_destroyed() { return web_contents_destroyed_; }
  bool render_view_host_changed() { return render_view_host_changed_; }

 private:
  bool render_view_host_changed_;
  bool web_contents_destroyed_;

  DISALLOW_COPY_AND_ASSIGN(SpeechWebContentsObserver);
};

// Tests that ChromeSpeechRecognitionManagerDelegate works properly
// when a WebContents goes away (WCO::WebContentsDestroyed) or the RVH
// changes within a WebContents (WCO::RenderViewHostChanged).
IN_PROC_BROWSER_TEST_F(ChromeSpeechRecognitionTest, BasicTearDown) {
  ASSERT_TRUE(embedded_test_server()->Start());
  net::EmbeddedTestServer https_server(net::EmbeddedTestServer::TYPE_HTTPS);
  https_server.ServeFilesFromSourceDirectory(GetChromeTestDataDir());
  ASSERT_TRUE(https_server.Start());

  GURL http_url =
      embedded_test_server()->GetURL("/speech/web_speech_test.html");
  GURL https_url(https_server.GetURL("/speech/web_speech_test.html"));

  std::unique_ptr<ChromeSpeechRecognitionManagerDelegate> delegate(
      new ChromeSpeechRecognitionManagerDelegate());
  static_cast<content::FakeSpeechRecognitionManager*>(
      fake_speech_recognition_manager_.get())->SetDelegate(delegate.get());

  ui_test_utils::NavigateToURL(browser(), http_url);
  WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_TRUE(web_contents);
  SpeechWebContentsObserver speech_contents_observer(web_contents);

  base::string16 success_title(base::ASCIIToUTF16("PASS"));
  base::string16 failure_title(base::ASCIIToUTF16("FAIL"));

  const char kRetriveTranscriptScript[] =
      "window.domAutomationController.send(window.getFirstTranscript())";
  const char kExpectedTranscript[] = "Pictures of the moon";

  {
    content::TitleWatcher title_watcher(web_contents, success_title);
    title_watcher.AlsoWaitForTitle(failure_title);
    EXPECT_TRUE(
        content::ExecuteScript(web_contents, "testSpeechRecognition()"));
    EXPECT_EQ(success_title, title_watcher.WaitAndGetTitle());

    std::string output;
    EXPECT_TRUE(content::ExecuteScriptAndExtractString(
        web_contents, kRetriveTranscriptScript, &output));
    EXPECT_EQ(kExpectedTranscript, output);
  }

  // Navigating to an https page will force RVH change within
  // |web_contents|, results in WCO::RenderViewHostChanged().
  ui_test_utils::NavigateToURL(browser(), https_url);

  EXPECT_TRUE(speech_contents_observer.render_view_host_changed());

  {
    content::TitleWatcher title_watcher(web_contents, success_title);
    title_watcher.AlsoWaitForTitle(failure_title);
    EXPECT_TRUE(
        content::ExecuteScript(web_contents, "testSpeechRecognition()"));
    EXPECT_EQ(success_title, title_watcher.WaitAndGetTitle());

    std::string output;
    EXPECT_TRUE(content::ExecuteScriptAndExtractString(
        web_contents, kRetriveTranscriptScript, &output));
    EXPECT_EQ(kExpectedTranscript, output);
  }

  // Close the tab to so that we see WCO::WebContentsDestroyed().
  chrome::CloseTab(browser());
  EXPECT_TRUE(speech_contents_observer.web_contents_destroyed());
}

}  // namespace speech
