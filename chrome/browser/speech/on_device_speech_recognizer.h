// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SPEECH_ON_DEVICE_SPEECH_RECOGNIZER_H_
#define CHROME_BROWSER_SPEECH_ON_DEVICE_SPEECH_RECOGNIZER_H_

#include <memory>

#include "base/memory/weak_ptr.h"
#include "chrome/browser/speech/speech_recognizer.h"
#include "chrome/browser/speech/speech_recognizer_delegate.h"
#include "media/mojo/mojom/speech_recognition_service.mojom.h"

class Profile;
class SpeechRecognizerDelegate;

// OnDeviceSpeechRecognizer is a wrapper around the on-device speech recognition
// engine that simplifies its use from the browser process.
class OnDeviceSpeechRecognizer
    : public SpeechRecognizer,
      public media::mojom::SpeechRecognitionRecognizerClient {
 public:
  // Returns true if on-device speech recognition is available and installed
  // on-device.
  // TODO(crbug.com/1173135): Language pack availability is based on the current
  // profile language settings, and currently based on kLiveCaptionLanguageCode
  // which is hard-coded to en-us. IsOnDeviceSpeechRecognizerAvailable should
  // take a language code to check.
  static bool IsOnDeviceSpeechRecognizerAvailable();

  OnDeviceSpeechRecognizer(
      const base::WeakPtr<SpeechRecognizerDelegate>& delegate,
      Profile* profile);
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

 private:
  void OnRecognizerBound(bool success);
  void OnRecognizerDisconnected();

  // Helper function to send the delegate updates to SpeechRecognizerStatus
  // only when the status has changed.
  void UpdateStatus(SpeechRecognizerStatus state);

  SpeechRecognizerStatus state_;

  mojo::Remote<media::mojom::SpeechRecognitionContext>
      speech_recognition_context_;
  mojo::Remote<media::mojom::AudioSourceFetcher> audio_source_fetcher_;
  mojo::Receiver<media::mojom::SpeechRecognitionRecognizerClient>
      speech_recognition_client_receiver_{this};

  base::WeakPtrFactory<OnDeviceSpeechRecognizer> weak_factory_{this};
};

#endif  // CHROME_BROWSER_SPEECH_ON_DEVICE_SPEECH_RECOGNIZER_H_
