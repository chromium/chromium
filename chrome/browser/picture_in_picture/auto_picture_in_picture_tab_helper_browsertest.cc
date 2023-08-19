// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/files/file_util.h"
#include "base/path_service.h"
#include "chrome/browser/media/webrtc/webrtc_browsertest_base.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/media_session.h"
#include "content/public/browser/media_session_service.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/media_start_stop_observer.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "services/media_session/public/cpp/test/audio_focus_test_util.h"
#include "services/media_session/public/cpp/test/mock_media_session.h"
#include "third_party/blink/public/common/features.h"

namespace {

const base::FilePath::CharType kAutoDocumentPipPage[] =
    FILE_PATH_LITERAL("media/picture-in-picture/autopip-document.html");

const base::FilePath::CharType kAutoVideoPipPage[] =
    FILE_PATH_LITERAL("media/picture-in-picture/autopip-video.html");

const base::FilePath::CharType kBlankPage[] =
    FILE_PATH_LITERAL("media/picture-in-picture/blank.html");

const base::FilePath::CharType kCameraPage[] =
    FILE_PATH_LITERAL("media/picture-in-picture/autopip-camera.html");

const base::FilePath::CharType kNotRegisteredPage[] =
    FILE_PATH_LITERAL("media/picture-in-picture/autopip-no-register.html");

const base::FilePath::CharType kAutopipDelayPage[] =
    FILE_PATH_LITERAL("media/picture-in-picture/autopip-delay.html");

class AutoPictureInPictureTabHelperBrowserTest : public WebRtcTestBase {
 public:
  AutoPictureInPictureTabHelperBrowserTest() = default;

  AutoPictureInPictureTabHelperBrowserTest(
      const AutoPictureInPictureTabHelperBrowserTest&) = delete;
  AutoPictureInPictureTabHelperBrowserTest& operator=(
      const AutoPictureInPictureTabHelperBrowserTest&) = delete;

  void SetUpOnMainThread() override {
    ResetAudioFocusObserver();

    host_resolver()->AddRule("*", "127.0.0.1");
    embedded_test_server()->ServeFilesFromSourceDirectory("chrome/test/data");
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  void SetUp() override {
    scoped_feature_list_.InitWithFeatures(
        {blink::features::kDocumentPictureInPictureAPI,
         blink::features::kMediaSessionEnterPictureInPicture},
        {});
    InProcessBrowserTest::SetUp();
  }

  void LoadAutoVideoPipPage(Browser* browser) {
    GURL test_page_url = ui_test_utils::GetTestUrl(
        base::FilePath(base::FilePath::kCurrentDirectory),
        base::FilePath(kAutoVideoPipPage));
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser, test_page_url));
  }

  void LoadAutoDocumentPipPage(Browser* browser) {
    GURL test_page_url = ui_test_utils::GetTestUrl(
        base::FilePath(base::FilePath::kCurrentDirectory),
        base::FilePath(kAutoDocumentPipPage));
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser, test_page_url));
  }

  void LoadCameraMicrophonePage(Browser* browser) {
    GURL test_page_url = ui_test_utils::GetTestUrl(
        base::FilePath(base::FilePath::kCurrentDirectory),
        base::FilePath(kCameraPage));
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser, test_page_url));
  }

  void LoadNotRegisteredPage(Browser* browser) {
    GURL test_page_url = ui_test_utils::GetTestUrl(
        base::FilePath(base::FilePath::kCurrentDirectory),
        base::FilePath(kNotRegisteredPage));
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser, test_page_url));
  }

  void LoadAutopipDelayPage(Browser* browser) {
    GURL test_page_url = ui_test_utils::GetTestUrl(
        base::FilePath(base::FilePath::kCurrentDirectory),
        base::FilePath(kAutopipDelayPage));
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser, test_page_url));
  }

  void OpenNewTab(Browser* browser) {
    GURL test_page_url = ui_test_utils::GetTestUrl(
        base::FilePath(base::FilePath::kCurrentDirectory),
        base::FilePath(kBlankPage));
    ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
        browser, test_page_url, WindowOpenDisposition::NEW_FOREGROUND_TAB,
        ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));
  }

  void OpenPopUp(Browser* browser) {
    GURL test_page_url = ui_test_utils::GetTestUrl(
        base::FilePath(base::FilePath::kCurrentDirectory),
        base::FilePath(kBlankPage));
    ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
        browser, test_page_url, WindowOpenDisposition::NEW_POPUP,
        ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));
  }

  void PlayVideo(content::WebContents* web_contents) {
    web_contents->GetPrimaryMainFrame()
        ->ExecuteJavaScriptWithUserGestureForTests(u"playVideo()",
                                                   base::NullCallback());
  }

  void PauseVideo(content::WebContents* web_contents) {
    web_contents->GetPrimaryMainFrame()->ExecuteJavaScriptForTests(
        u"pauseVideo()", base::NullCallback());
  }

  void OpenPipManually(content::WebContents* web_contents) {
    web_contents->GetPrimaryMainFrame()
        ->ExecuteJavaScriptWithUserGestureForTests(
            u"openPip({automatic: true})", base::NullCallback());
  }

  void WaitForMediaSessionPaused(content::WebContents* web_contents) {
    media_session::test::MockMediaSessionMojoObserver observer(
        *content::MediaSession::Get(web_contents));
    observer.WaitForPlaybackState(
        media_session::mojom::MediaPlaybackState::kPaused);
  }

  void WaitForAudioFocusGained() {
    audio_focus_observer_->WaitForGainedEvent();
  }

  void ResetAudioFocusObserver() {
    mojo::Remote<media_session::mojom::AudioFocusManager> audio_focus_remote;
    content::GetMediaSessionService().BindAudioFocusManager(
        audio_focus_remote.BindNewPipeAndPassReceiver());
    audio_focus_observer_ =
        std::make_unique<media_session::test::TestAudioFocusObserver>();
    audio_focus_remote->AddObserver(
        audio_focus_observer_->BindNewPipeAndPassRemote());
  }

  void SwitchToNewTabAndBackAndExpectAutopip(bool should_video_pip,
                                             bool should_document_pip) {
    auto* original_web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();

    // There should not currently be a picture-in-picture window.
    EXPECT_FALSE(original_web_contents->HasPictureInPictureVideo());
    EXPECT_FALSE(original_web_contents->HasPictureInPictureDocument());

    // Open and switch to a new tab.
    content::MediaStartStopObserver enter_pip_observer(
        original_web_contents,
        content::MediaStartStopObserver::Type::kEnterPictureInPicture);
    OpenNewTab(browser());
    enter_pip_observer.Wait();

    // A picture-in-picture window of the correct type should automatically
    // open.
    EXPECT_EQ(should_video_pip,
              original_web_contents->HasPictureInPictureVideo());
    EXPECT_EQ(should_document_pip,
              original_web_contents->HasPictureInPictureDocument());

    // Switch back to the original tab.
    content::MediaStartStopObserver exit_pip_observer(
        original_web_contents,
        content::MediaStartStopObserver::Type::kExitPictureInPicture);
    browser()->tab_strip_model()->ActivateTabAt(
        browser()->tab_strip_model()->GetIndexOfWebContents(
            original_web_contents));
    exit_pip_observer.Wait();

    // There should no longer be a picture-in-picture window.
    EXPECT_FALSE(original_web_contents->HasPictureInPictureVideo());
    EXPECT_FALSE(original_web_contents->HasPictureInPictureDocument());
  }

 private:
  std::unique_ptr<media_session::test::TestAudioFocusObserver>
      audio_focus_observer_;

  base::test::ScopedFeatureList scoped_feature_list_;
};

}  // namespace

IN_PROC_BROWSER_TEST_F(AutoPictureInPictureTabHelperBrowserTest,
                       OpensAndClosesVideoAutopip) {
  // Load a page that registers for autopip and start video playback.
  LoadAutoVideoPipPage(browser());
  PlayVideo(browser()->tab_strip_model()->GetActiveWebContents());
  WaitForAudioFocusGained();

  SwitchToNewTabAndBackAndExpectAutopip(/*should_video_pip=*/true,
                                        /*should_document_pip=*/false);
}

IN_PROC_BROWSER_TEST_F(AutoPictureInPictureTabHelperBrowserTest,
                       OpensAndClosesDocumentAutopip) {
  // Load a page that registers for autopip and start video playback.
  LoadAutoDocumentPipPage(browser());
  PlayVideo(browser()->tab_strip_model()->GetActiveWebContents());
  WaitForAudioFocusGained();

  SwitchToNewTabAndBackAndExpectAutopip(/*should_video_pip=*/false,
                                        /*should_document_pip=*/true);
}

// TODO(https://crbug.com/1457056): Enable when media session actions can be
// routed without requesting audio focus (see https://crrev.com/c/4659151).
IN_PROC_BROWSER_TEST_F(AutoPictureInPictureTabHelperBrowserTest,
                       DISABLED_CanAutopipWithCameraMicrophone) {
  // Load a page that registers for autopip and starts using camera/microphone.
  LoadCameraMicrophonePage(browser());
  GetUserMediaAndAccept(browser()->tab_strip_model()->GetActiveWebContents());

  SwitchToNewTabAndBackAndExpectAutopip(/*should_video_pip=*/false,
                                        /*should_document_pip=*/true);
}

IN_PROC_BROWSER_TEST_F(AutoPictureInPictureTabHelperBrowserTest,
                       DoesNotAutopipWithoutPlayback) {
  // Load a page that registers for autopip but doesn't start playback.
  LoadAutoVideoPipPage(browser());
  auto* original_web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  // There should not currently be a picture-in-picture window.
  EXPECT_FALSE(original_web_contents->HasPictureInPictureVideo());
  EXPECT_FALSE(original_web_contents->HasPictureInPictureDocument());

  // Open and switch to a new tab.
  OpenNewTab(browser());

  // There should not be a picture-in-picture window.
  EXPECT_FALSE(original_web_contents->HasPictureInPictureVideo());
  EXPECT_FALSE(original_web_contents->HasPictureInPictureDocument());
}

IN_PROC_BROWSER_TEST_F(AutoPictureInPictureTabHelperBrowserTest,
                       DoesNotAutopipWhenPaused) {
  // Load a page that registers for autopip and start video playback.
  LoadAutoVideoPipPage(browser());
  auto* original_web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  PlayVideo(original_web_contents);
  WaitForAudioFocusGained();

  // Pause the video.
  PauseVideo(original_web_contents);
  WaitForMediaSessionPaused(original_web_contents);

  // There should not currently be a picture-in-picture window.
  EXPECT_FALSE(original_web_contents->HasPictureInPictureVideo());
  EXPECT_FALSE(original_web_contents->HasPictureInPictureDocument());

  // Open and switch to a new tab.
  OpenNewTab(browser());

  // There should not be a picture-in-picture window.
  EXPECT_FALSE(original_web_contents->HasPictureInPictureVideo());
  EXPECT_FALSE(original_web_contents->HasPictureInPictureDocument());
}

IN_PROC_BROWSER_TEST_F(AutoPictureInPictureTabHelperBrowserTest,
                       DoesNotCloseManuallyOpenedPip) {
  // Load a page that registers for autopip and start video playback.
  LoadAutoVideoPipPage(browser());
  auto* original_web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  PlayVideo(original_web_contents);
  WaitForAudioFocusGained();

  // Open a picture-in-picture window manually.
  content::MediaStartStopObserver enter_pip_observer(
      original_web_contents,
      content::MediaStartStopObserver::Type::kEnterPictureInPicture);
  OpenPipManually(original_web_contents);
  enter_pip_observer.Wait();

  // A pip window should have opened.
  EXPECT_TRUE(original_web_contents->HasPictureInPictureVideo());

  // Open and switch to a new tab.
  OpenNewTab(browser());

  // The pip window should still be open.
  EXPECT_TRUE(original_web_contents->HasPictureInPictureVideo());

  // Switch back to the original tab.
  browser()->tab_strip_model()->ActivateTabAt(
      browser()->tab_strip_model()->GetIndexOfWebContents(
          original_web_contents));

  // The pip window should still be open.
  EXPECT_TRUE(original_web_contents->HasPictureInPictureVideo());
}

IN_PROC_BROWSER_TEST_F(AutoPictureInPictureTabHelperBrowserTest,
                       ShowsMostRecentlyHiddenTab) {
  // Load a page that registers for autopip and start video playback.
  LoadAutoVideoPipPage(browser());
  auto* original_web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  PlayVideo(original_web_contents);
  WaitForAudioFocusGained();

  // There should not currently be a picture-in-picture window.
  EXPECT_FALSE(original_web_contents->HasPictureInPictureVideo());

  // Open and switch to a new tab.
  {
    content::MediaStartStopObserver enter_pip_observer(
        original_web_contents,
        content::MediaStartStopObserver::Type::kEnterPictureInPicture);
    OpenNewTab(browser());
    enter_pip_observer.Wait();
  }

  // A video picture-in-picture window should automatically open.
  EXPECT_TRUE(original_web_contents->HasPictureInPictureVideo());

  // In the new tab, load a page that registers for autopip and start video
  // playback. Resetting the audio focus observer prevents us from continuing
  // based on the audio focus gained in the original tab.
  LoadAutoVideoPipPage(browser());
  auto* second_web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ResetAudioFocusObserver();
  PlayVideo(second_web_contents);
  WaitForAudioFocusGained();

  // The original tab should still be in picture-in-picture.
  EXPECT_TRUE(original_web_contents->HasPictureInPictureVideo());
  EXPECT_FALSE(second_web_contents->HasPictureInPictureVideo());

  // Switch back to the original tab.
  {
    content::MediaStartStopObserver exit_pip_observer(
        original_web_contents,
        content::MediaStartStopObserver::Type::kExitPictureInPicture);
    content::MediaStartStopObserver enter_pip_observer(
        second_web_contents,
        content::MediaStartStopObserver::Type::kEnterPictureInPicture);
    browser()->tab_strip_model()->ActivateTabAt(
        browser()->tab_strip_model()->GetIndexOfWebContents(
            original_web_contents));
    exit_pip_observer.Wait();
    enter_pip_observer.Wait();
  }

  // The second tab should now be in picture-in-picture.
  EXPECT_FALSE(original_web_contents->HasPictureInPictureVideo());
  EXPECT_TRUE(second_web_contents->HasPictureInPictureVideo());

  // Open a third tab.
  {
    content::MediaStartStopObserver exit_pip_observer(
        second_web_contents,
        content::MediaStartStopObserver::Type::kExitPictureInPicture);
    content::MediaStartStopObserver enter_pip_observer(
        original_web_contents,
        content::MediaStartStopObserver::Type::kEnterPictureInPicture);
    OpenNewTab(browser());
    exit_pip_observer.Wait();
    enter_pip_observer.Wait();
  }

  // The original tab should now be in picture-in-picture.
  EXPECT_TRUE(original_web_contents->HasPictureInPictureVideo());
  EXPECT_FALSE(second_web_contents->HasPictureInPictureVideo());

  // Switch back to the original tab.
  {
    content::MediaStartStopObserver exit_pip_observer(
        original_web_contents,
        content::MediaStartStopObserver::Type::kExitPictureInPicture);
    browser()->tab_strip_model()->ActivateTabAt(
        browser()->tab_strip_model()->GetIndexOfWebContents(
            original_web_contents));
    exit_pip_observer.Wait();
  }

  // Nothing should be in picture-in-picture.
  EXPECT_FALSE(original_web_contents->HasPictureInPictureVideo());
  EXPECT_FALSE(second_web_contents->HasPictureInPictureVideo());
}

IN_PROC_BROWSER_TEST_F(AutoPictureInPictureTabHelperBrowserTest,
                       DoesNotAutopipWhenSwitchingToADifferentWindow) {
  // Load a page that registers for autopip and start video playback.
  LoadAutoVideoPipPage(browser());
  auto* original_web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  PlayVideo(original_web_contents);
  WaitForAudioFocusGained();

  // There should not currently be a picture-in-picture window.
  EXPECT_FALSE(original_web_contents->HasPictureInPictureVideo());
  EXPECT_FALSE(original_web_contents->HasPictureInPictureDocument());

  // Open and switch to a new popup window.
  OpenPopUp(browser());

  // There should not be a picture-in-picture window.
  EXPECT_FALSE(original_web_contents->HasPictureInPictureVideo());
  EXPECT_FALSE(original_web_contents->HasPictureInPictureDocument());
}

IN_PROC_BROWSER_TEST_F(AutoPictureInPictureTabHelperBrowserTest,
                       DoesNotAutopipIfNotRegistered) {
  // Load a page that does not register for autopip and start video playback.
  LoadNotRegisteredPage(browser());
  auto* original_web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  PlayVideo(original_web_contents);
  WaitForAudioFocusGained();

  // There should not currently be a picture-in-picture window.
  EXPECT_FALSE(original_web_contents->HasPictureInPictureVideo());
  EXPECT_FALSE(original_web_contents->HasPictureInPictureDocument());

  // Open and switch to a new tab.
  OpenNewTab(browser());

  // The page should not autopip since it is not registered.
  EXPECT_FALSE(original_web_contents->HasPictureInPictureVideo());
  EXPECT_FALSE(original_web_contents->HasPictureInPictureDocument());
}

IN_PROC_BROWSER_TEST_F(AutoPictureInPictureTabHelperBrowserTest,
                       ImmediatelyClosesAutopipIfTabIsAlreadyFocused) {
  // Load a page that is registered for autopip (delayed) and start video
  // playback.
  LoadAutopipDelayPage(browser());
  auto* original_web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  PlayVideo(original_web_contents);
  WaitForAudioFocusGained();

  // There should not currently be a picture-in-picture window.
  EXPECT_FALSE(original_web_contents->HasPictureInPictureVideo());
  EXPECT_FALSE(original_web_contents->HasPictureInPictureDocument());

  content::MediaStartStopObserver enter_pip_observer(
      original_web_contents,
      content::MediaStartStopObserver::Type::kEnterPictureInPicture);
  content::MediaStartStopObserver exit_pip_observer(
      original_web_contents,
      content::MediaStartStopObserver::Type::kExitPictureInPicture);

  // Open and switch to a new tab.
  OpenNewTab(browser());

  // Immediately switch back to the original tab.
  browser()->tab_strip_model()->ActivateTabAt(
      browser()->tab_strip_model()->GetIndexOfWebContents(
          original_web_contents));

  // When the page enters autopip after its delay it should immediately be
  // exited.
  enter_pip_observer.Wait();
  exit_pip_observer.Wait();

  // The page should no longer be in picture-in-picture.
  EXPECT_FALSE(original_web_contents->HasPictureInPictureVideo());
  EXPECT_FALSE(original_web_contents->HasPictureInPictureDocument());
}
