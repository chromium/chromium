// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/accessibility/live_caption/live_caption_speech_recognition_host_browsertest.h"

#include <string>
#include <vector>

#include "chrome/browser/accessibility/live_caption/live_caption_controller_factory.h"
#include "chrome/browser/accessibility/live_caption/live_caption_speech_recognition_host.h"
#include "chrome/browser/accessibility/live_caption/live_caption_test_util.h"
#include "chrome/browser/accessibility/live_translate_controller_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/live_caption/caption_bubble_controller.h"
#include "components/live_caption/live_caption_controller.h"
#include "components/live_caption/live_translate_controller.h"
#include "components/live_caption/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/sync_preferences/pref_service_syncable.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "media/mojo/mojom/speech_recognition.mojom.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"

namespace {
FullscreenEventsWaiter::FullscreenEventsWaiter(
    content::WebContents* web_contents)
    : WebContentsObserver(web_contents) {}

FullscreenEventsWaiter::~FullscreenEventsWaiter() = default;

void FullscreenEventsWaiter::MediaEffectivelyFullscreenChanged(bool value) {
  if (run_loop_) {
    run_loop_->Quit();
  }
}

void FullscreenEventsWaiter::Wait() {
  run_loop_ = std::make_unique<base::RunLoop>();
  run_loop_->Run();
}
}  // namespace

namespace captions {
MockLiveTranslateController::MockLiveTranslateController(
    PrefService* profile_prefs,
    content::BrowserContext* browser_context)
    : LiveTranslateController(profile_prefs, browser_context) {}

MockLiveTranslateController::~MockLiveTranslateController() = default;

void MockLiveTranslateController::GetTranslation(
    const std::string& result,
    std::string source_language,
    std::string target_language,
    OnTranslateEventCallback callback) {
  translation_requests_.push_back(result);
  std::move(callback).Run(result);
}

std::vector<std::string> MockLiveTranslateController::GetTranslationRequests() {
  return translation_requests_;
}

LiveCaptionSpeechRecognitionHostTest::LiveCaptionSpeechRecognitionHostTest() =
    default;
LiveCaptionSpeechRecognitionHostTest::~LiveCaptionSpeechRecognitionHostTest() =
    default;

std::unique_ptr<KeyedService>
LiveCaptionSpeechRecognitionHostTest::SetLiveTranslateController(
    content::BrowserContext* context) {
  return std::make_unique<testing::NiceMock<MockLiveTranslateController>>(
      browser()->profile()->GetPrefs(), browser()->profile());
}

void LiveCaptionSpeechRecognitionHostTest::SetUp() {
  // This is required for the fullscreen video tests.
  embedded_test_server()->ServeFilesFromSourceDirectory(
      base::FilePath(FILE_PATH_LITERAL("content/test/data")));
  LiveCaptionBrowserTest::SetUp();
}

void LiveCaptionSpeechRecognitionHostTest::SetUpOnMainThread() {
  LiveTranslateControllerFactory::GetInstance()->SetTestingFactory(
      browser()->profile(),
      base::BindRepeating(
          &LiveCaptionSpeechRecognitionHostTest::SetLiveTranslateController,
          base::Unretained(this)));
  LiveCaptionBrowserTest::SetUpOnMainThread();
  ASSERT_TRUE(embedded_test_server()->Start());
}

void LiveCaptionSpeechRecognitionHostTest::
    CreateLiveCaptionSpeechRecognitionHost(
        content::RenderFrameHost* frame_host) {
  mojo::Remote<media::mojom::SpeechRecognitionRecognizerClient> remote;
  mojo::PendingReceiver<media::mojom::SpeechRecognitionRecognizerClient>
      receiver;
  remote.Bind(receiver.InitWithNewPipeAndPassRemote());
  LiveCaptionSpeechRecognitionHost::Create(frame_host, std::move(receiver));
  remotes_.emplace(frame_host, std::move(remote));
}

void LiveCaptionSpeechRecognitionHostTest::OnSpeechRecognitionRecognitionEvent(
    content::RenderFrameHost* frame_host,
    std::string text,
    bool expected_success,
    bool is_final) {
  remotes_[frame_host]->OnSpeechRecognitionRecognitionEvent(
      media::SpeechRecognitionResult(text, is_final),
      base::BindOnce(
          &LiveCaptionSpeechRecognitionHostTest::DispatchTranscriptionCallback,
          base::Unretained(this), expected_success));
}

void LiveCaptionSpeechRecognitionHostTest::OnLanguageIdentificationEvent(
    content::RenderFrameHost* frame_host,
    const std::string& language,
    const media::mojom::ConfidenceLevel confidence_level,
    const media::mojom::AsrSwitchResult asr_switch_result) {
  remotes_[frame_host]->OnLanguageIdentificationEvent(
      media::mojom::LanguageIdentificationEvent::New(language, confidence_level,
                                                     asr_switch_result));
}

void LiveCaptionSpeechRecognitionHostTest::OnSpeechRecognitionError(
    content::RenderFrameHost* frame_host) {
  remotes_[frame_host]->OnSpeechRecognitionError();
}

bool LiveCaptionSpeechRecognitionHostTest::HasBubbleController() {
  return LiveCaptionControllerFactory::GetForProfile(browser()->profile())
             ->caption_bubble_controller_for_testing() != nullptr;
}

void LiveCaptionSpeechRecognitionHostTest::ExpectIsWidgetVisible(bool visible) {
#if defined(TOOLKIT_VIEWS)
    CaptionBubbleController* bubble_controller =
        LiveCaptionControllerFactory::GetForProfile(browser()->profile())
            ->caption_bubble_controller_for_testing();
    EXPECT_EQ(visible, bubble_controller->IsWidgetVisibleForTesting());
#endif
}

std::vector<std::string>
LiveCaptionSpeechRecognitionHostTest::GetTranslationRequests() {
  return static_cast<MockLiveTranslateController*>(
             LiveTranslateControllerFactory::GetForProfile(
                 browser()->profile()))
      ->GetTranslationRequests();
}

void LiveCaptionSpeechRecognitionHostTest::DispatchTranscriptionCallback(
    bool expected_success,
    bool success) {
  EXPECT_EQ(expected_success, success);
}

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
      frame_host, "en-US", media::mojom::ConfidenceLevel::kHighlyConfident,
      media::mojom::AsrSwitchResult::kSwitchSucceeded);
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

  OnSpeechRecognitionRecognitionEvent(
      frame_host, "Elephants can live over 90 years in captivity.",
      /* expected_success= */ true);
  base::RunLoop().RunUntilIdle();
  ExpectIsWidgetVisible(true);
}

IN_PROC_BROWSER_TEST_F(LiveCaptionSpeechRecognitionHostTest, TranslationCache) {
  content::RenderFrameHost* frame_host = browser()
                                             ->tab_strip_model()
                                             ->GetActiveWebContents()
                                             ->GetPrimaryMainFrame();
  CreateLiveCaptionSpeechRecognitionHost(frame_host);

  SetLiveCaptionEnabled(true);
  SetLiveTranslateEnabled(true);

  OnSpeechRecognitionRecognitionEvent(
      frame_host,
      "Pomeranians come in 23 different color combinations. Some Pomeranians",
      /* expected_success= */ true, /* is_final= */ false);
  base::RunLoop().RunUntilIdle();
  ExpectIsWidgetVisible(true);

  ASSERT_EQ(1u, GetTranslationRequests().size());
  ASSERT_EQ(
      "Pomeranians come in 23 different color combinations. Some Pomeranians",
      GetTranslationRequests().back());

  OnSpeechRecognitionRecognitionEvent(
      frame_host,
      "Pomeranians come in 23 different color combinations. Some Pomeranians",
      /* expected_success= */ true, /* is_final= */ false);
  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(2u, GetTranslationRequests().size());
  ASSERT_EQ("Some Pomeranians", GetTranslationRequests().back());

  OnSpeechRecognitionRecognitionEvent(
      frame_host,
      "Pomeranians come in 23 different color combinations. Some Pomeranians "
      "are even tricolored! Are",
      /* expected_success= */ true, /* is_final= */ false);
  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(3u, GetTranslationRequests().size());
  ASSERT_EQ("Some Pomeranians are even tricolored! Are",
            GetTranslationRequests().back());

  OnSpeechRecognitionRecognitionEvent(
      frame_host,
      "Pomeranians come in 23 different color combinations. Some Pomeranians "
      "are even tricolored! Are they the cutest dog breed in the",
      /* expected_success= */ true, /* is_final= */ false);
  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(4u, GetTranslationRequests().size());
  ASSERT_EQ("Are they the cutest dog breed in the",
            GetTranslationRequests().back());

  OnSpeechRecognitionRecognitionEvent(
      frame_host,
      "Pomeranians come in 23 different color combinations. Some Pomeranians "
      "are even tricolored! Are they the cutest dog breed in the world? "
      "Absolutely.",
      /* expected_success= */ true, /* is_final= */ true);
  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(5u, GetTranslationRequests().size());
  ASSERT_EQ("Are they the cutest dog breed in the world? Absolutely.",
            GetTranslationRequests().back());

  // The previous final event clears the translation cache.
  OnSpeechRecognitionRecognitionEvent(
      frame_host, "Pomeranians come in 23 different color combinations.",
      /* expected_success= */ true, /* is_final= */ false);
  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(6u, GetTranslationRequests().size());
  ASSERT_EQ("Pomeranians come in 23 different color combinations.",
            GetTranslationRequests().back());

  // Ensure that cached strings aren't retrieved out of order.
  OnSpeechRecognitionRecognitionEvent(frame_host, "First sentence. Second",
                                      /* expected_success= */ true,
                                      /* is_final= */ false);
  OnSpeechRecognitionRecognitionEvent(frame_host, "Third sentence. Fourth",
                                      /* expected_success= */ true,
                                      /* is_final= */ false);
  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(8u, GetTranslationRequests().size());
  OnSpeechRecognitionRecognitionEvent(
      frame_host, "First sentence. Second sentence. Third sentence.",
      /* expected_success= */ true, /* is_final= */ false);
  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(9u, GetTranslationRequests().size());
  ASSERT_EQ("Second sentence. Third sentence.",
            GetTranslationRequests().back());

  // Ensure that partial sentences aren't cached.
  OnSpeechRecognitionRecognitionEvent(frame_host, "Fourth sentence",
                                      /* expected_success= */ true,
                                      /* is_final= */ false);
  OnSpeechRecognitionRecognitionEvent(frame_host, "Fourth sentence",
                                      /* expected_success= */ true,
                                      /* is_final= */ false);
  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(11u, GetTranslationRequests().size());

  // Ensure that punctuation marks aren't cached.
  OnSpeechRecognitionRecognitionEvent(frame_host, "Possums can play dead! Wow",
                                      /* expected_success= */ true,
                                      /* is_final= */ false);
  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(12u, GetTranslationRequests().size());
  ASSERT_EQ("Possums can play dead! Wow", GetTranslationRequests().back());

  OnSpeechRecognitionRecognitionEvent(frame_host, "Possums can play dead? Wow",
                                      /* expected_success= */ true,
                                      /* is_final= */ false);
  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(13u, GetTranslationRequests().size());
  ASSERT_EQ("Wow", GetTranslationRequests().back());

  // Ensure that phrases are cached without capitalization.
  OnSpeechRecognitionRecognitionEvent(
      frame_host, "Possums can play DEAD? Amazing",
      /* expected_success= */ true, /* is_final= */ false);
  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(14u, GetTranslationRequests().size());
  ASSERT_EQ("Amazing", GetTranslationRequests().back());

  // Ensure that strings are cached without whitespace.
  OnSpeechRecognitionRecognitionEvent(
      frame_host, "Flying squirrels can glide up to 300 feet. Wow",
      /* expected_success= */ true, /* is_final= */ false);
  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(15u, GetTranslationRequests().size());
  ASSERT_EQ("Flying squirrels can glide up to 300 feet. Wow",
            GetTranslationRequests().back());
  OnSpeechRecognitionRecognitionEvent(
      frame_host, "Flying squirrels can glide up to 300 feet.\nThat's so far",
      /* expected_success= */ true, /* is_final= */ false);
  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(16u, GetTranslationRequests().size());
  ASSERT_EQ("That's so far", GetTranslationRequests().back());
}

IN_PROC_BROWSER_TEST_F(LiveCaptionSpeechRecognitionHostTest,
                       IdeographicTranslationCache) {
  content::RenderFrameHost* frame_host = browser()
                                             ->tab_strip_model()
                                             ->GetActiveWebContents()
                                             ->GetPrimaryMainFrame();
  SetLiveCaptionEnabled(true);
  SetLiveTranslateEnabled(true);

  // Ensure that ideographic to non-ideographic translations are not cached.
  browser()->profile()->GetPrefs()->SetString(prefs::kLiveCaptionLanguageCode,
                                              "ja-JP");
  CreateLiveCaptionSpeechRecognitionHost(frame_host);

  OnSpeechRecognitionRecognitionEvent(frame_host,
                                      "Tanuki are canids, similar to dogs but "
                                      "with larger ears and tails. So cool",
                                      /* expected_success= */ true,
                                      /* is_final= */ false);
  OnSpeechRecognitionRecognitionEvent(frame_host,
                                      "Tanuki are canids, similar to dogs but "
                                      "with larger ears and tails. So cool",
                                      /* expected_success= */ true,
                                      /* is_final= */ false);
  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(2u, GetTranslationRequests().size());
  ASSERT_EQ(
      "Tanuki are canids, similar to dogs but with larger ears and tails. So "
      "cool",
      GetTranslationRequests().back());
}

}  // namespace captions
