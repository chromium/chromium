// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACCESSIBILITY_LIVE_CAPTION_LIVE_CAPTION_SPEECH_RECOGNITION_HOST_BROWSERTEST_H_
#define CHROME_BROWSER_ACCESSIBILITY_LIVE_CAPTION_LIVE_CAPTION_SPEECH_RECOGNITION_HOST_BROWSERTEST_H_

#include "chrome/browser/accessibility/live_caption/live_caption_speech_recognition_host.h"

#include <string>
#include <vector>

#include "chrome/browser/accessibility/live_caption/live_caption_controller_factory.h"
#include "chrome/browser/accessibility/live_caption/live_caption_test_util.h"
#include "chrome/browser/accessibility/live_translate_controller_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/live_caption/caption_bubble_controller.h"
#include "components/live_caption/live_caption_controller.h"
#include "components/live_caption/live_translate_controller.h"
#include "components/live_caption/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/sync_preferences/pref_service_syncable.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/test/browser_test.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"

namespace {
// A WebContentsObserver that allows waiting for some media to start or stop
// playing fullscreen.
class FullscreenEventsWaiter : public content::WebContentsObserver {
 public:
  explicit FullscreenEventsWaiter(content::WebContents* web_contents);
  FullscreenEventsWaiter(const FullscreenEventsWaiter& rhs) = delete;
  FullscreenEventsWaiter& operator=(const FullscreenEventsWaiter& rhs) = delete;
  ~FullscreenEventsWaiter() override;

  void MediaEffectivelyFullscreenChanged(bool value) override;

  // Wait for the current media playing fullscreen mode to be equal to
  // |expected_media_fullscreen_mode|.
  void Wait();

 private:
  std::unique_ptr<base::RunLoop> run_loop_;
};
}  // namespace

namespace captions {

class MockLiveTranslateController : public LiveTranslateController {
 public:
  MockLiveTranslateController(PrefService* profile_prefs,
                              content::BrowserContext* browser_context);
  ~MockLiveTranslateController() override;

  void GetTranslation(const std::string& result,
                      std::string source_language,
                      std::string target_language,
                      OnTranslateEventCallback callback) override;

  // Returns a collection of strings passed into `GetTranslation()`.
  std::vector<std::string> GetTranslationRequests();

 private:
  std::vector<std::string> translation_requests_;
};

class LiveCaptionSpeechRecognitionHostTest : public LiveCaptionBrowserTest {
 public:
  LiveCaptionSpeechRecognitionHostTest();
  ~LiveCaptionSpeechRecognitionHostTest() override;
  LiveCaptionSpeechRecognitionHostTest(
      const LiveCaptionSpeechRecognitionHostTest&) = delete;
  LiveCaptionSpeechRecognitionHostTest& operator=(
      const LiveCaptionSpeechRecognitionHostTest&) = delete;

  std::unique_ptr<KeyedService> SetLiveTranslateController(
      content::BrowserContext* context);

  // LiveCaptionBrowserTest:
  void SetUp() override;
  void SetUpOnMainThread() override;

  void CreateLiveCaptionSpeechRecognitionHost(
      content::RenderFrameHost* frame_host);
  void OnSpeechRecognitionRecognitionEvent(content::RenderFrameHost* frame_host,
                                           std::string text,
                                           bool expected_success,
                                           bool is_final = false);
  void OnLanguageIdentificationEvent(
      content::RenderFrameHost* frame_host,
      const std::string& language,
      const media::mojom::ConfidenceLevel confidence_level,
      const media::mojom::AsrSwitchResult asr_switch_result);
  void OnSpeechRecognitionError(content::RenderFrameHost* frame_host);
  bool HasBubbleController();
  void ExpectIsWidgetVisible(bool visible);
  std::vector<std::string> GetTranslationRequests();

 private:
  void DispatchTranscriptionCallback(bool expected_success, bool success);
  std::map<content::RenderFrameHost*,
           mojo::Remote<media::mojom::SpeechRecognitionRecognizerClient>>
      remotes_;
};

}  // namespace captions

#endif  // CHROME_BROWSER_ACCESSIBILITY_LIVE_CAPTION_LIVE_CAPTION_SPEECH_RECOGNITION_HOST_BROWSERTEST_H_
