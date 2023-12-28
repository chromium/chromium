// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/accessibility/live_caption/live_caption_speech_recognition_host_browsertest.h"

#include <set>

#include "base/containers/contains.h"
#include "base/functional/callback_forward.h"
#include "base/run_loop.h"
#include "base/scoped_observation.h"
#include "components/soda/constants.h"
#include "components/soda/soda_installer.h"
#include "media/mojo/mojom/speech_recognition.mojom.h"

namespace captions {

class LiveCaptionAutomaticLanguageDownloadTest
    : public LiveCaptionSpeechRecognitionHostTest,
      public speech::SodaInstaller::Observer {
 public:
  LiveCaptionAutomaticLanguageDownloadTest() = default;
  ~LiveCaptionAutomaticLanguageDownloadTest() override = default;
  LiveCaptionAutomaticLanguageDownloadTest(
      const LiveCaptionAutomaticLanguageDownloadTest&) = delete;
  LiveCaptionAutomaticLanguageDownloadTest& operator=(
      const LiveCaptionAutomaticLanguageDownloadTest&) = delete;

  // SodaInstaller::Observer:
  void OnSodaInstalled(speech::LanguageCode language_code) override {}
  void OnSodaInstallError(
      speech::LanguageCode language_code,
      speech::SodaInstaller::ErrorCode error_code) override {
    if (language_code != speech::LanguageCode::kNone) {
      installed_languages_.insert(language_code);
    }

    if (installed_languages_.size() == expected_language_pack_count_) {
      std::move(quit_waiting_callback_).Run();
    }
  }
  void OnSodaProgress(speech::LanguageCode language_code,
                      int progress) override {}

  std::set<speech::LanguageCode> GetInstalledLanguages() {
    return installed_languages_;
  }

  void WaitForLanguagePackInstallation(size_t expected_language_pack_count) {
    base::RunLoop run_loop;
    quit_waiting_callback_ = run_loop.QuitClosure();
    expected_language_pack_count_ = expected_language_pack_count;
    run_loop.Run();
  }

  base::OnceClosure quit_waiting_callback_;
  size_t expected_language_pack_count_ = 0u;
  std::set<speech::LanguageCode> installed_languages_;
};

// Verify that 3 consecutive highly confident language identification events
// trigger an automatic download of the language pack.
IN_PROC_BROWSER_TEST_F(LiveCaptionAutomaticLanguageDownloadTest,
                       AutomaticLanguageDownload) {
  base::ScopedObservation<speech::SodaInstaller,
                          speech::SodaInstaller::Observer>
      soda_installer_observer{this};
  soda_installer_observer.Observe(speech::SodaInstaller::GetInstance());

  content::RenderFrameHost* frame_host = browser()
                                             ->tab_strip_model()
                                             ->GetActiveWebContents()
                                             ->GetPrimaryMainFrame();
  CreateLiveCaptionSpeechRecognitionHost(frame_host);

  SetLiveCaptionEnabled(true);
  OnLanguageIdentificationEvent(
      frame_host, "de-de", media::mojom::ConfidenceLevel::kConfident,
      media::mojom::AsrSwitchResult::kDefaultNoSwitch);
  OnLanguageIdentificationEvent(
      frame_host, "de-de", media::mojom::ConfidenceLevel::kConfident,
      media::mojom::AsrSwitchResult::kDefaultNoSwitch);
  OnLanguageIdentificationEvent(
      frame_host, "de-de", media::mojom::ConfidenceLevel::kConfident,
      media::mojom::AsrSwitchResult::kDefaultNoSwitch);

  OnLanguageIdentificationEvent(
      frame_host, "it-it", media::mojom::ConfidenceLevel::kHighlyConfident,
      media::mojom::AsrSwitchResult::kDefaultNoSwitch);
  OnLanguageIdentificationEvent(
      frame_host, "de-de", media::mojom::ConfidenceLevel::kHighlyConfident,
      media::mojom::AsrSwitchResult::kDefaultNoSwitch);
  OnLanguageIdentificationEvent(
      frame_host, "it-it", media::mojom::ConfidenceLevel::kHighlyConfident,
      media::mojom::AsrSwitchResult::kDefaultNoSwitch);
  OnLanguageIdentificationEvent(
      frame_host, "it-it", media::mojom::ConfidenceLevel::kHighlyConfident,
      media::mojom::AsrSwitchResult::kDefaultNoSwitch);

  OnLanguageIdentificationEvent(
      frame_host, "fr-ca", media::mojom::ConfidenceLevel::kHighlyConfident,
      media::mojom::AsrSwitchResult::kDefaultNoSwitch);
  OnLanguageIdentificationEvent(
      frame_host, "fr-ca", media::mojom::ConfidenceLevel::kHighlyConfident,
      media::mojom::AsrSwitchResult::kDefaultNoSwitch);
  OnLanguageIdentificationEvent(
      frame_host, "fr-ca", media::mojom::ConfidenceLevel::kHighlyConfident,
      media::mojom::AsrSwitchResult::kDefaultNoSwitch);

  size_t expected_language_pack_count = 2u;
  WaitForLanguagePackInstallation(expected_language_pack_count);
  std::set<speech::LanguageCode> installed_languages = GetInstalledLanguages();
  ASSERT_EQ(expected_language_pack_count, installed_languages.size());

  // The en-US language pack is downloaded by default. Only the fr-FR language
  // pack should be automatically downloaded.
  ASSERT_TRUE(base::Contains(installed_languages, speech::LanguageCode::kEnUs));
  ASSERT_TRUE(base::Contains(installed_languages, speech::LanguageCode::kFrFr));
}

}  // namespace captions
