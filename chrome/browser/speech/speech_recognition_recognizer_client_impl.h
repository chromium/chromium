// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SPEECH_SPEECH_RECOGNITION_RECOGNIZER_CLIENT_IMPL_H_
#define CHROME_BROWSER_SPEECH_SPEECH_RECOGNITION_RECOGNIZER_CLIENT_IMPL_H_

#include <memory>
#include <string>

#include "ash/public/cpp/projector/speech_recognition_availability.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/speech/speech_recognizer.h"
#include "chrome/browser/speech/speech_recognizer_delegate.h"
#include "media/audio/audio_system.h"
#include "media/mojo/mojom/speech_recognition_service.mojom.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

class Profile;
class SpeechRecognizerDelegate;

namespace media {
class AudioSystem;
}  // namespace media

// SpeechRecognitionRecognizerClientImpl is a wrapper around the on-device
// speech recognition engine that simplifies its use from the browser process.
// This client class is only used on ash chrome browser.
class SpeechRecognitionRecognizerClientImpl
    : public SpeechRecognizer,
      public media::mojom::SpeechRecognitionRecognizerClient {
 public:
  // Returns the availability of on-device speech recognition for the given
  // language (BCP-47 format, e.g. "en-US").
  static ash::OnDeviceRecognitionAvailability
  GetOnDeviceSpeechRecognitionAvailability(const std::string& language);

  // Returns the availability of server-based speech recognition for the given
  // language.
  static ash::ServerBasedRecognitionAvailability
  GetServerBasedRecognitionAvailability(const std::string& language);

  SpeechRecognitionRecognizerClientImpl(
      const base::WeakPtr<SpeechRecognizerDelegate>& delegate,
      Profile* profile,
      const std::string& device_id,
      media::mojom::SpeechRecognitionOptionsPtr options);
  ~SpeechRecognitionRecognizerClientImpl() override;
  SpeechRecognitionRecognizerClientImpl(
      const SpeechRecognitionRecognizerClientImpl&) = delete;
  SpeechRecognitionRecognizerClientImpl& operator=(
      const SpeechRecognitionRecognizerClientImpl&) = delete;

  // SpeechRecognizer:
  // Start and Stop must be called on the UI thread.
  void Start() override;
  void Stop() override;

  // media::mojom::SpeechRecognitionRecognizerClient:
  void OnSpeechRecognitionRecognitionEvent(
      const media::SpeechRecognitionResult& result,
      OnSpeechRecognitionRecognitionEventCallback reply) override;
  void OnSpeechRecognitionError() override;
  void OnLanguageIdentificationEvent(
      media::mojom::LanguageIdentificationEventPtr event) override;
  void OnSpeechRecognitionStopped() override;

  void set_audio_system_for_testing(
      std::unique_ptr<media::AudioSystem> audio_system) {
    audio_system_ = std::move(audio_system);
  }

 private:
  void OnRecognizerBound(bool success);
  void OnRecognizerDisconnected();
  void StartFetchingOnInputDeviceInfo(
      const std::optional<media::AudioParameters>& params);

  // Helper function to send the delegate updates to SpeechRecognizerStatus
  // only when the status has changed.
  void UpdateStatus(SpeechRecognizerStatus state);

  SpeechRecognizerStatus state_{SpeechRecognizerStatus::SPEECH_RECOGNIZER_OFF};
  std::string device_id_;
  bool is_multichannel_supported_{false};
  std::string language_;

  // Whether we are waiting for the AudioParameters callback to return. Used
  // to ensure Start doesn't keep starting if Stop or Error were called
  // in between requesting the callback and it running.
  bool waiting_for_params_{false};

  // Tests may use the audio system setter above after constructing an
  // SpeechRecognitionRecognizerClientImpl to override default behavior.
  std::unique_ptr<media::AudioSystem> audio_system_;

  mojo::Remote<media::mojom::AudioSourceSpeechRecognitionContext>
      audio_source_speech_recognition_context_;
  mojo::Remote<media::mojom::AudioSourceFetcher> audio_source_fetcher_;
  mojo::Receiver<media::mojom::SpeechRecognitionRecognizerClient>
      speech_recognition_client_receiver_{this};

  base::WeakPtrFactory<SpeechRecognitionRecognizerClientImpl> weak_factory_{
      this};
};

#endif  // CHROME_BROWSER_SPEECH_SPEECH_RECOGNITION_RECOGNIZER_CLIENT_IMPL_H_
