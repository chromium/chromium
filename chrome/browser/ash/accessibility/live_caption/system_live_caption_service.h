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
                      const absl::optional<media::SpeechRecognitionResult>&
                          full_result) override;
  void OnSpeechSoundLevelChanged(int16_t level) override;
  void OnSpeechRecognitionStateChanged(
      SpeechRecognizerStatus new_state) override;
  void OnSpeechRecognitionStopped() override;

  // media::mojom::SpeechRecognitionBrowserObserver overrides:
  void SpeechRecognitionAvailabilityChanged(
      bool is_speech_recognition_available) override;
  void SpeechRecognitionLanguageChanged(const std::string& language) override;

  void set_audio_system_factory_for_testing(
      base::RepeatingCallback<std::unique_ptr<media::AudioSystem>()>
          create_audio_system_for_testing) {
    create_audio_system_for_testing_ =
        std::move(create_audio_system_for_testing);
  }

  // CrasAudioHandler::AudioObserver overrides
  void OnNonChromeOutputStarted() override;

  void OnNonChromeOutputStopped() override;

 private:
  SpeechRecognizerStatus current_recognizer_status_ =
      SpeechRecognizerStatus::SPEECH_RECOGNIZER_OFF;
  bool output_running_ = false;

  std::unique_ptr<base::OneShotTimer> stop_countdown_timer_;

  // Stops and destructs audio stream recognizing client.
  void StopRecognizing();

  void CreateClient();
  void StopTimeoutFinished();

  const base::raw_ptr<Profile> profile_;
  base::raw_ptr<::captions::LiveCaptionController> controller_;

  ash::captions::CaptionBubbleContextAsh context_;

  std::unique_ptr<SpeechRecognitionRecognizerClientImpl> client_;

  mojo::Receiver<media::mojom::SpeechRecognitionBrowserObserver>
      browser_observer_receiver_{this};

  // Used to inject a fake audio system into our client in tests.
  base::RepeatingCallback<std::unique_ptr<media::AudioSystem>()>
      create_audio_system_for_testing_;

  base::WeakPtrFactory<SystemLiveCaptionService> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_ACCESSIBILITY_LIVE_CAPTION_SYSTEM_LIVE_CAPTION_SERVICE_H_
