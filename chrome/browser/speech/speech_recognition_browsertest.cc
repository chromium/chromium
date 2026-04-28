// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/strings/utf_string_conversions.h"
#include "base/task/bind_post_task.h"
#include "base/test/test_future.h"
#include "chrome/browser/speech/chrome_speech_recognition_manager_delegate.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/fake_speech_recognition_manager.h"
#include "net/test/embedded_test_server/embedded_test_server.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/profiles/profile.h"
#include "extensions/browser/process_map.h"
#endif

using content::FakeSpeechRecognitionManager;
using content::WebContents;

namespace speech {

class ChromeSpeechRecognitionTest : public InProcessBrowserTest {
 public:
  ChromeSpeechRecognitionTest() = default;

  ChromeSpeechRecognitionTest(const ChromeSpeechRecognitionTest&) = delete;
  ChromeSpeechRecognitionTest& operator=(const ChromeSpeechRecognitionTest&) =
      delete;

  ~ChromeSpeechRecognitionTest() override = default;

  static void CheckRenderFrameType(
      base::OnceCallback<void(bool ask_user, bool is_allowed)> callback,
      int render_process_id,
      int render_frame_id) {
    ChromeSpeechRecognitionManagerDelegate::CheckRenderFrameType(
        std::move(callback), render_process_id, render_frame_id);
  }

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

  ~SpeechWebContentsObserver() override = default;

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

// Tests the TOCTOU race condition where an iframe is detached between
// StartRequestOnUI and CheckRenderFrameType, causing the RFH to be null.
// The safe fallback logic should securely deny permission.
IN_PROC_BROWSER_TEST_F(ChromeSpeechRecognitionTest, TOCTOUPermissionBypass) {
  base::test::TestFuture<bool /* ask_user */, bool /* is_allowed */> future;

  int process_id = browser()
                       ->tab_strip_model()
                       ->GetActiveWebContents()
                       ->GetPrimaryMainFrame()
                       ->GetProcess()
                       ->GetID()
                       .GetUnsafeValue();

  // Call CheckRenderFrameType directly on the UI thread with an invalid RFH ID
  // but a valid renderer process ID to simulate a detached iframe.
  CheckRenderFrameType(
      base::BindPostTask(content::GetUIThreadTaskRunner({}),
                         future.GetCallback()),
      process_id, -1);

  // Wait for the callback and validate the safe logic.
  // Get<0>() is ask_user, Get<1>() is is_allowed.
  EXPECT_FALSE(future.Get<0>());
  EXPECT_FALSE(future.Get<1>());
}

#if BUILDFLAG(ENABLE_EXTENSIONS)
// Verifies that extension background pages/service workers are successfully
// granted permission despite having a null RenderFrameHost.
IN_PROC_BROWSER_TEST_F(ChromeSpeechRecognitionTest,
                       ExtensionBackgroundPageAllowed) {
  base::test::TestFuture<bool /* ask_user */, bool /* is_allowed */> future;

  int process_id = browser()
                       ->tab_strip_model()
                       ->GetActiveWebContents()
                       ->GetPrimaryMainFrame()
                       ->GetProcess()
                       ->GetID()
                       .GetUnsafeValue();

  extensions::ProcessMap::Get(browser()->profile())
      ->Insert("fake_extension_id", process_id);

  // Call CheckRenderFrameType with a missing frame, which is typical for
  // extension background pages or service workers.
  CheckRenderFrameType(base::BindPostTask(content::GetUIThreadTaskRunner({}),
                                          future.GetCallback()),
                       process_id, -1);

  // For extensions, ask_user should be false (manifest checks apply instead)
  // and is_allowed should be true.
  EXPECT_FALSE(future.Get<0>());
  EXPECT_TRUE(future.Get<1>());
}
#endif

}  // namespace speech
