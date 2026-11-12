// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TTC_CORE_HOTL_SODA_HOTL_SPEECH_RECOGNIZER_H_
#define CHROME_BROWSER_TTC_CORE_HOTL_SODA_HOTL_SPEECH_RECOGNIZER_H_

#include <stdint.h>

#include <string_view>

#include "base/containers/span.h"
#include "base/functional/callback.h"
#include "base/sequence_checker.h"
#include "chrome/browser/ttc/core/hotl/hotl_speech_recognizer.h"
#include "media/mojo/mojom/speech_recognition.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

class Profile;

namespace ttc {

// HotlSpeechRecognizer implementation backed by Chrome's on-device SODA
// SpeechRecognitionService.
class SodaHotlSpeechRecognizer
    : public HotlSpeechRecognizer,
      public media::mojom::SpeechRecognitionRecognizerClient {
 public:
  using SpeechRecognitionContextBinder = base::RepeatingCallback<void(
      mojo::PendingReceiver<media::mojom::SpeechRecognitionContext>)>;

  explicit SodaHotlSpeechRecognizer(Profile& profile);
  // Test-only constructor allowing injection of a fake speech recognition
  // context binder without spinning up KeyedServices or testing profiles.
  explicit SodaHotlSpeechRecognizer(
      SpeechRecognitionContextBinder context_binder);
  ~SodaHotlSpeechRecognizer() override;

  SodaHotlSpeechRecognizer(const SodaHotlSpeechRecognizer&) = delete;
  SodaHotlSpeechRecognizer& operator=(const SodaHotlSpeechRecognizer&) = delete;

  // HotlSpeechRecognizer:
  void Start(std::string_view locale,
             TranscriptCallback on_transcript,
             ErrorCallback on_error) override;
  void Stop() override;
  void PushAudioChunk(base::span<const int16_t> pcm_data,
                      const media::AudioParameters& params) override;

 private:
  // media::mojom::SpeechRecognitionRecognizerClient:
  void OnSpeechRecognitionRecognitionEvent(
      const media::SpeechRecognitionResult& result,
      OnSpeechRecognitionRecognitionEventCallback reply) override;
  void OnSpeechRecognitionStopped() override;
  void OnSpeechRecognitionError() override;
  void OnLanguageIdentificationEvent(
      media::mojom::LanguageIdentificationEventPtr event) override;

  void HandleError();

  SEQUENCE_CHECKER(sequence_checker_);

  SpeechRecognitionContextBinder context_binder_;
  TranscriptCallback on_transcript_;
  ErrorCallback on_error_;

  // All bound iff recognition is active.
  mojo::Remote<media::mojom::SpeechRecognitionContext> context_remote_;
  mojo::Remote<media::mojom::SpeechRecognitionRecognizer> recognizer_remote_;
  mojo::Receiver<media::mojom::SpeechRecognitionRecognizerClient>
      client_receiver_{this};
};

}  // namespace ttc

#endif  // CHROME_BROWSER_TTC_CORE_HOTL_SODA_HOTL_SPEECH_RECOGNIZER_H_
