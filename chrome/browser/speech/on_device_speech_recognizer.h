// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SPEECH_ON_DEVICE_SPEECH_RECOGNIZER_H_
#define CHROME_BROWSER_SPEECH_ON_DEVICE_SPEECH_RECOGNIZER_H_

#include <memory>
#include <string>

#include "base/memory/weak_ptr.h"
#include "chrome/browser/speech/speech_recognizer.h"
#include "chrome/browser/speech/speech_recognizer_delegate.h"
#include "media/mojo/mojom/speech_recognition_service.mojom.h"

class Profile;
class SpeechRecognizerDelegate;

namespace media {
class AudioSystem;
}  // namespace media

// OnDeviceSpeechRecognizer is a wrapper around the on-device speech recognition
// engine that simplifies its use from the browser process.
class OnDeviceSpeechRecognizer
    : public SpeechRecognizer,
      public media::mojom::SpeechRecognitionRecognizerClient {
 public:
  // Returns true if on-device speech recognition is available and installed
  // on-device.
  static bool IsOnDeviceSpeechRecognizerAvailable(
      std::string language_or_locale);

  OnDeviceSpeechRecognizer(
      const base::WeakPtr<SpeechRecognizerDelegate>& delegate,
      Profile* profile,
      std::string language_or_locale);
  ~OnDeviceSpeechRecognizer() override;
  OnDeviceSpeechRecognizer(const OnDeviceSpeechRecognizer&) = delete;
  OnDeviceSpeechRecognizer& operator=(const OnDeviceSpeechRecognizer&) = delete;

  // SpeechRecognizer:
  // Start and Stop must be called on the UI thread.
  void Start() override;
  void Stop() override;

  // media::mojom::SpeechRecognitionRecognizerClient:
  void OnSpeechRecognitionRecognitionEvent(
      media::mojom::SpeechRecognitionResultPtr result) override;
  void OnSpeechRecognitionError() override;
  void OnLanguageIdentificationEvent(
      media::mojom::LanguageIdentificationEventPtr event) override;

 private:
  friend class OnDeviceSpeechRecognizerTest;

  void OnRecognizerBound(bool success);
  void OnRecognizerDisconnected();
  void StartFetchingOnInputDeviceInfo(
      const base::Optional<media::AudioParameters>& params);

  // Helper function to send the delegate updates to SpeechRecognizerStatus
  // only when the status has changed.
  void UpdateStatus(SpeechRecognizerStatus state);

  SpeechRecognizerStatus state_;
  bool is_multichannel_supported_;
  std::string language_or_locale_;

  // Whether we are waiting for the AudioParameters callback to return. Used
  // to ensure Start doesn't keep starting if Stop or Error were called
  // in between requesting the callback and it running.
  bool waiting_for_params_;

  // Tests may set audio_system_ after constructing an OnDeviceSpeechRecognizer
  // to override default behavior.
  std::unique_ptr<media::AudioSystem> audio_system_;

  mojo::Remote<media::mojom::SpeechRecognitionContext>
      speech_recognition_context_;
  mojo::Remote<media::mojom::AudioSourceFetcher> audio_source_fetcher_;
  mojo::Receiver<media::mojom::SpeechRecognitionRecognizerClient>
      speech_recognition_client_receiver_{this};

  base::WeakPtrFactory<OnDeviceSpeechRecognizer> weak_factory_{this};
};

#endif  // CHROME_BROWSER_SPEECH_ON_DEVICE_SPEECH_RECOGNIZER_H_
