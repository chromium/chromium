// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/accessibility/live_caption_speech_recognition_host.h"

#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/live_caption/pref_names.h"
#include "components/soda/soda_installer.h"
#include "components/sync_preferences/pref_service_syncable.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/test/browser_test.h"
#include "media/base/media_switches.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/constants/ash_features.h"
#endif

namespace {
// Chrome OS requires an additional feature flag to enable Live Caption.
std::vector<base::Feature> RequiredFeatureFlags() {
  std::vector<base::Feature> features = {media::kLiveCaption,
                                         media::kUseSodaForLiveCaption};
#if BUILDFLAG(IS_CHROMEOS_ASH)
  features.push_back(ash::features::kOnDeviceSpeechRecognition);
#endif
  return features;
}
}  // namespace

namespace captions {

class LiveCaptionSpeechRecognitionHostTest : public InProcessBrowserTest {
 public:
  LiveCaptionSpeechRecognitionHostTest() = default;
  ~LiveCaptionSpeechRecognitionHostTest() override = default;
  LiveCaptionSpeechRecognitionHostTest(
      const LiveCaptionSpeechRecognitionHostTest&) = delete;
  LiveCaptionSpeechRecognitionHostTest& operator=(
      const LiveCaptionSpeechRecognitionHostTest&) = delete;

  // InProcessBrowserTest overrides:
  void SetUp() override {
    scoped_feature_list_.InitWithFeatures(RequiredFeatureFlags(), {});
    InProcessBrowserTest::SetUp();
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
        media::mojom::SpeechRecognitionResult::New(text, /*final=*/true),
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

  void SetLiveCaptionEnabled(bool enabled) {
    browser()->profile()->GetPrefs()->SetBoolean(prefs::kLiveCaptionEnabled,
                                                 enabled);
    if (enabled)
      speech::SodaInstaller::GetInstance()->NotifySodaInstalledForTesting();
  }

 private:
  void DispatchTranscriptionCallback(bool expected_success, bool success) {
    EXPECT_EQ(expected_success, success);
  }

  base::test::ScopedFeatureList scoped_feature_list_;
  std::map<content::RenderFrameHost*,
           mojo::Remote<media::mojom::SpeechRecognitionRecognizerClient>>
      remotes_;
};

IN_PROC_BROWSER_TEST_F(LiveCaptionSpeechRecognitionHostTest,
                       DestroysWithoutCrashing) {
  content::RenderFrameHost* frame_host =
      browser()->tab_strip_model()->GetActiveWebContents()->GetMainFrame();
  CreateLiveCaptionSpeechRecognitionHost(frame_host);

  SetLiveCaptionEnabled(true);
  OnSpeechRecognitionRecognitionEvent(
      frame_host,
      "Pandas' coloring helps them camouflage in snowy environments.",
      /* expected_success= */ true);
  base::RunLoop().RunUntilIdle();

  chrome::Reload(browser(), WindowOpenDisposition::CURRENT_TAB);
  content::WaitForLoadStop(
      browser()->tab_strip_model()->GetActiveWebContents());
  // Test passes if the following line runs without crashing.
  OnSpeechRecognitionRecognitionEvent(frame_host,
                                      "Pandas have vertical slits for pupils.",
                                      /* expected_success= */ true);
  base::RunLoop().RunUntilIdle();
}

IN_PROC_BROWSER_TEST_F(LiveCaptionSpeechRecognitionHostTest,
                       OnSpeechRecognitionRecognitionEvent) {
  content::RenderFrameHost* frame_host =
      browser()->tab_strip_model()->GetActiveWebContents()->GetMainFrame();
  CreateLiveCaptionSpeechRecognitionHost(frame_host);

  SetLiveCaptionEnabled(true);
  OnSpeechRecognitionRecognitionEvent(frame_host,
                                      "Pandas learn to climb at 5 months old.",
                                      /* expected_success= */ true);
  base::RunLoop().RunUntilIdle();

  SetLiveCaptionEnabled(false);
  OnSpeechRecognitionRecognitionEvent(
      frame_host,
      "Pandas have an extended wrist bone which they use like a thumb.",
      /* expected_success= */ false);
  base::RunLoop().RunUntilIdle();
}

IN_PROC_BROWSER_TEST_F(LiveCaptionSpeechRecognitionHostTest,
                       OnLanguageIdentificationEvent) {
  content::RenderFrameHost* frame_host =
      browser()->tab_strip_model()->GetActiveWebContents()->GetMainFrame();
  CreateLiveCaptionSpeechRecognitionHost(frame_host);

  SetLiveCaptionEnabled(true);
  OnLanguageIdentificationEvent(
      frame_host, "en-US", media::mojom::ConfidenceLevel::kHighlyConfident);
}

IN_PROC_BROWSER_TEST_F(LiveCaptionSpeechRecognitionHostTest,
                       OnSpeechRecognitionError) {
  content::RenderFrameHost* frame_host =
      browser()->tab_strip_model()->GetActiveWebContents()->GetMainFrame();
  CreateLiveCaptionSpeechRecognitionHost(frame_host);

  SetLiveCaptionEnabled(true);
  OnSpeechRecognitionError(frame_host);
}

}  // namespace captions
