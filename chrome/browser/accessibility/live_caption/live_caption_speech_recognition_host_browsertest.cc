// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/accessibility/live_caption/live_caption_speech_recognition_host.h"

#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/accessibility/live_caption/live_caption_controller_factory.h"
#include "chrome/browser/accessibility/live_caption/live_caption_test_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/live_caption/caption_bubble_controller.h"
#include "components/live_caption/live_caption_controller.h"
#include "components/live_caption/pref_names.h"
#include "components/sync_preferences/pref_service_syncable.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/test/browser_test.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/constants/ash_features.h"
#endif

namespace {
// A WebContentsObserver that allows waiting for some media to start or stop
// playing fullscreen.
class FullscreenEventsWaiter : public content::WebContentsObserver {
 public:
  explicit FullscreenEventsWaiter(content::WebContents* web_contents)
      : WebContentsObserver(web_contents) {}
  FullscreenEventsWaiter(const FullscreenEventsWaiter& rhs) = delete;
  FullscreenEventsWaiter& operator=(const FullscreenEventsWaiter& rhs) = delete;
  ~FullscreenEventsWaiter() override = default;

  void MediaEffectivelyFullscreenChanged(bool value) override {
    if (run_loop_)
      run_loop_->Quit();
  }

  // Wait for the current media playing fullscreen mode to be equal to
  // |expected_media_fullscreen_mode|.
  void Wait() {
    run_loop_ = std::make_unique<base::RunLoop>();
    run_loop_->Run();
  }

 private:
  std::unique_ptr<base::RunLoop> run_loop_;
};
}  // namespace

namespace captions {

class LiveCaptionSpeechRecognitionHostTest : public LiveCaptionBrowserTest {
 public:
  LiveCaptionSpeechRecognitionHostTest() = default;
  ~LiveCaptionSpeechRecognitionHostTest() override = default;
  LiveCaptionSpeechRecognitionHostTest(
      const LiveCaptionSpeechRecognitionHostTest&) = delete;
  LiveCaptionSpeechRecognitionHostTest& operator=(
      const LiveCaptionSpeechRecognitionHostTest&) = delete;

  // LiveCaptionBrowserTest:
  void SetUp() override {
    // This is required for the fullscreen video tests.
    embedded_test_server()->ServeFilesFromSourceDirectory(
        base::FilePath(FILE_PATH_LITERAL("content/test/data")));
    LiveCaptionBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    LiveCaptionBrowserTest::SetUpOnMainThread();
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  void CreateLiveCaptionSpeechRecognitionHost(
      content::RenderFrameHost* frame_host) {
    mojo::Remote<media::mojom::SpeechRecognitionRecognizerClient> remote;
    mojo::PendingReceiver<media::mojom::SpeechRecognitionRecognizerClient>
        receiver;
    remote.Bind(receiver.InitWithNewPipeAndPassRemote());
    LiveCaptionSpeechRecognitionHost::Create(frame_host, std::move(receiver));
    remotes_.emplace(frame_host, std::move(remote));
  }

  void OnSpeechRecognitionRecognitionEvent(content::RenderFrameHost* frame_host,
                                           std::string text,
                                           bool expected_success) {
    remotes_[frame_host]->OnSpeechRecognitionRecognitionEvent(
        media::SpeechRecognitionResult(text, /*is_final=*/false),
        base::BindOnce(&LiveCaptionSpeechRecognitionHostTest::
                           DispatchTranscriptionCallback,
                       base::Unretained(this), expected_success));
  }

  void OnLanguageIdentificationEvent(
      content::RenderFrameHost* frame_host,
      const std::string& language,
      const media::mojom::ConfidenceLevel confidence_level) {
    remotes_[frame_host]->OnLanguageIdentificationEvent(
        media::mojom::LanguageIdentificationEvent::New(language,
                                                       confidence_level));
  }

  void OnSpeechRecognitionError(content::RenderFrameHost* frame_host) {
    remotes_[frame_host]->OnSpeechRecognitionError();
  }

  bool HasBubbleController() {
    return LiveCaptionControllerFactory::GetForProfile(browser()->profile())
               ->caption_bubble_controller_for_testing() != nullptr;
  }

  void ExpectIsWidgetVisible(bool visible) {
#if defined(TOOLKIT_VIEWS)
    CaptionBubbleController* bubble_controller =
        LiveCaptionControllerFactory::GetForProfile(browser()->profile())
            ->caption_bubble_controller_for_testing();
    EXPECT_EQ(visible, bubble_controller->IsWidgetVisibleForTesting());
#endif
  }

 private:
  void DispatchTranscriptionCallback(bool expected_success, bool success) {
    EXPECT_EQ(expected_success, success);
  }

  std::map<content::RenderFrameHost*,
           mojo::Remote<media::mojom::SpeechRecognitionRecognizerClient>>
      remotes_;
};

// Disabled due to flaky crashes; https://crbug.com/1216304.
IN_PROC_BROWSER_TEST_F(LiveCaptionSpeechRecognitionHostTest,
                       DISABLED_DestroysWithoutCrashing) {
  content::RenderFrameHost* frame_host = browser()
                                             ->tab_strip_model()
                                             ->GetActiveWebContents()
                                             ->GetPrimaryMainFrame();
  CreateLiveCaptionSpeechRecognitionHost(frame_host);

  SetLiveCaptionEnabled(true);
  OnSpeechRecognitionRecognitionEvent(
      frame_host,
      "Pandas' coloring helps them camouflage in snowy environments.",
      /* expected_success= */ true);
  base::RunLoop().RunUntilIdle();
  ExpectIsWidgetVisible(true);

  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), GURL("http://www.google.com")));
  content::WaitForLoadStop(
      browser()->tab_strip_model()->GetActiveWebContents());
  content::RenderFrameHost* new_frame_host = browser()
                                                 ->tab_strip_model()
                                                 ->GetActiveWebContents()
                                                 ->GetPrimaryMainFrame();
  // After navigating to a new URL, the main frame should be different from the
  // former frame host.
  CreateLiveCaptionSpeechRecognitionHost(new_frame_host);
  ExpectIsWidgetVisible(false);
  // Test passes if the following line runs without crashing.
  OnSpeechRecognitionRecognitionEvent(new_frame_host,
                                      "Pandas have vertical slits for pupils.",
                                      /* expected_success= */ true);
  base::RunLoop().RunUntilIdle();
  ExpectIsWidgetVisible(true);
}

IN_PROC_BROWSER_TEST_F(LiveCaptionSpeechRecognitionHostTest,
                       OnSpeechRecognitionRecognitionEvent) {
  content::RenderFrameHost* frame_host = browser()
                                             ->tab_strip_model()
                                             ->GetActiveWebContents()
                                             ->GetPrimaryMainFrame();
  CreateLiveCaptionSpeechRecognitionHost(frame_host);

  SetLiveCaptionEnabled(true);
  OnSpeechRecognitionRecognitionEvent(frame_host,
                                      "Pandas learn to climb at 5 months old.",
                                      /* expected_success= */ true);
  base::RunLoop().RunUntilIdle();
  ExpectIsWidgetVisible(true);

  SetLiveCaptionEnabled(false);
  OnSpeechRecognitionRecognitionEvent(
      frame_host,
      "Pandas have an extended wrist bone which they use like a thumb.",
      /* expected_success= */ false);
  base::RunLoop().RunUntilIdle();
}

IN_PROC_BROWSER_TEST_F(LiveCaptionSpeechRecognitionHostTest,
                       OnLanguageIdentificationEvent) {
  content::RenderFrameHost* frame_host = browser()
                                             ->tab_strip_model()
                                             ->GetActiveWebContents()
                                             ->GetPrimaryMainFrame();
  CreateLiveCaptionSpeechRecognitionHost(frame_host);

  SetLiveCaptionEnabled(true);
  OnLanguageIdentificationEvent(
      frame_host, "en-US", media::mojom::ConfidenceLevel::kHighlyConfident);
}

IN_PROC_BROWSER_TEST_F(LiveCaptionSpeechRecognitionHostTest,
                       OnSpeechRecognitionError) {
  content::RenderFrameHost* frame_host = browser()
                                             ->tab_strip_model()
                                             ->GetActiveWebContents()
                                             ->GetPrimaryMainFrame();
  CreateLiveCaptionSpeechRecognitionHost(frame_host);

  SetLiveCaptionEnabled(true);
  OnSpeechRecognitionError(frame_host);
}

#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_CHROMEOS)
IN_PROC_BROWSER_TEST_F(LiveCaptionSpeechRecognitionHostTest,
                       MediaEffectivelyFullscreenChanged) {
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::RenderFrameHost* frame_host = web_contents->GetPrimaryMainFrame();
  CreateLiveCaptionSpeechRecognitionHost(frame_host);
  EXPECT_TRUE(content::NavigateToURL(
      web_contents, embedded_test_server()->GetURL("/media/fullscreen.html")));

  SetLiveCaptionEnabled(true);
  EXPECT_TRUE(HasBubbleController());

  FullscreenEventsWaiter waiter(web_contents);
  EXPECT_TRUE(content::ExecJs(web_contents, "makeFullscreen('small_video')"));
  waiter.Wait();
  EXPECT_TRUE(HasBubbleController());

  EXPECT_TRUE(content::ExecJs(web_contents, "exitFullscreen()"));
  waiter.Wait();
  EXPECT_TRUE(HasBubbleController());
}
#endif

IN_PROC_BROWSER_TEST_F(LiveCaptionSpeechRecognitionHostTest, LiveTranslate) {
  content::RenderFrameHost* frame_host = browser()
                                             ->tab_strip_model()
                                             ->GetActiveWebContents()
                                             ->GetPrimaryMainFrame();
  CreateLiveCaptionSpeechRecognitionHost(frame_host);

  SetLiveCaptionEnabled(true);
  SetLiveTranslateEnabled(true);

  // Dispatching the speech recognition should be successful, but the
  // widget should remain hidden because the test does not fetch real
  // translations.
  OnSpeechRecognitionRecognitionEvent(
      frame_host, "Elephants can live over 90 years in captivity.",
      /* expected_success= */ true);
  base::RunLoop().RunUntilIdle();
  ExpectIsWidgetVisible(false);
}

}  // namespace captions
