// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ACCESSIBILITY_LIVE_CAPTION_SYSTEM_LIVE_CAPTION_SERVICE_H_
#define CHROME_BROWSER_ASH_ACCESSIBILITY_LIVE_CAPTION_SYSTEM_LIVE_CAPTION_SERVICE_H_

#include <memory>

#include "ash/accessibility/caption_bubble_context_ash.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/timer/timer.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"
#include "chrome/browser/speech/speech_recognition_recognizer_client_impl.h"
#include "chrome/browser/speech/speech_recognizer_delegate.h"
#include "chromeos/ash/components/audio/cras_audio_handler.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/live_caption/live_translate_controller.h"
#include "components/live_caption/translation_util.h"
#include "components/soda/constants.h"
#include "components/soda/soda_installer.h"
#include "media/mojo/mojom/speech_recognition.mojom.h"
#include "mojo/public/cpp/bindings/receiver.h"

class Profile;
class SpeechRecognitionRecognizerClientImpl;

namespace captions {
class LiveCaptionController;
}  // namespace captions

namespace media {
class AudioSystem;
}  // namespace media

namespace ash {

// Responsible for running the live captioning model on audio from non-web (e.g.
// Android, linux) sources. Internally uses the "audio stream" speech
// recognition API on (what will eventually be) a "loopback" audio stream.
//
// This class doesn't track preferences, package installation or audio status at
// all; it is told to start/stop by the classes that actually do so.
//
// For the moment, this is prototype logic only: it processes the input device
// stream (c.f. a not-yet-existing "non-web only" loopback) and processes the
// stream even when no audio is being produced.
//
// TODO(b/253114860): Until these issues are addressed, this class can't be used
//                    in production.
class SystemLiveCaptionService
    : public KeyedService,
      public SpeechRecognizerDelegate,
      public media::mojom::SpeechRecognitionBrowserObserver,
      public CrasAudioHandler::AudioObserver {
 public:
  explicit SystemLiveCaptionService(Profile* profile);
  ~SystemLiveCaptionService() override;

  SystemLiveCaptionService(const SystemLiveCaptionService&) = delete;
  SystemLiveCaptionService& operator=(const SystemLiveCaptionService&) = delete;

  // KeyedService overrides:
  void Shutdown() override;

  // SpeechRecognizerDelegate overrides:
  void OnSpeechResult(const std::u16string& text,
                      bool is_final,
                      const std::optional<media::SpeechRecognitionResult>&
                          full_result) override;
  void OnSpeechSoundLevelChanged(int16_t level) override;
  void OnSpeechRecognitionStateChanged(
      SpeechRecognizerStatus new_state) override;
  void OnSpeechRecognitionStopped() override;
  void OnLanguageIdentificationEvent(
      media::mojom::LanguageIdentificationEventPtr event) override;

  // media::mojom::SpeechRecognitionBrowserObserver overrides:
  void SpeechRecognitionAvailabilityChanged(
      bool is_speech_recognition_available) override;
  void SpeechRecognitionLanguageChanged(const std::string& language) override;
  void SpeechRecognitionMaskOffensiveWordsChanged(
      bool mask_offensive_words) override;

  void set_audio_system_factory_for_testing(
      base::RepeatingCallback<std::unique_ptr<media::AudioSystem>()>
          create_audio_system_for_testing) {
    create_audio_system_for_testing_ =
        std::move(create_audio_system_for_testing);
  }

  void set_num_non_chrome_output_streams_for_testing(
      uint32_t num_output_streams) {
    num_output_streams_for_testing_ = num_output_streams;
  }

  // CrasAudioHandler::AudioObserver overrides
  void OnNonChromeOutputStarted() override;

  void OnNonChromeOutputStopped() override;

 private:
  void OnTranslationCallback(const std::string& cached_translation,
                             const std::string& original_transcription,
                             const std::string& source_language,
                             const std::string& target_language,
                             bool is_final,
                             const std::string& result);
  // The source language code of the audio stream.
  std::string source_language_;
  SpeechRecognizerStatus current_recognizer_status_ =
      SpeechRecognizerStatus::SPEECH_RECOGNIZER_OFF;
  bool output_running_ = false;

  std::unique_ptr<base::OneShotTimer> stop_countdown_timer_;

  // Stops and destructs audio stream recognizing client.
  void StopRecognizing();

  void CreateClient();
  void StopTimeoutFinished();

  void OpenCaptionSettings();

  // wrapper around CrasAudioHandler's NumberOfNonChromeOutputStreams.  If
  // we inject a value for the number of non chrome output streams this method
  // will instead return that value.
  uint32_t GetNumberOfNonChromeOutputStreams();

  ::captions::TranslationCache translation_cache_;

  const raw_ptr<Profile> profile_;
  raw_ptr<::captions::LiveCaptionController> controller_;
  ash::captions::CaptionBubbleContextAsh context_;

  std::unique_ptr<SpeechRecognitionRecognizerClientImpl> client_;

  // The number of characters sent to the translation service.
  int characters_translated_ = 0;

  // The number of characters omitted from the translation by the text
  // stabilization policy. Used by metrics only.
  int translation_characters_erased_ = 0;

  // If set during a test this number will be used to determine the
  // number of non chrome output streams.
  std::optional<uint32_t> num_output_streams_for_testing_;

  mojo::Receiver<media::mojom::SpeechRecognitionBrowserObserver>
      browser_observer_receiver_{this};

  // Used to inject a fake audio system into our client in tests.
  base::RepeatingCallback<std::unique_ptr<media::AudioSystem>()>
      create_audio_system_for_testing_;

  base::WeakPtrFactory<SystemLiveCaptionService> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_ACCESSIBILITY_LIVE_CAPTION_SYSTEM_LIVE_CAPTION_SERVICE_H_
