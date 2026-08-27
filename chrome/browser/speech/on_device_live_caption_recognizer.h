// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SPEECH_ON_DEVICE_LIVE_CAPTION_RECOGNIZER_H_
#define CHROME_BROWSER_SPEECH_ON_DEVICE_LIVE_CAPTION_RECOGNIZER_H_

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "media/base/audio_bus.h"
#include "media/base/audio_parameters.h"
#include "media/base/converting_audio_fifo.h"
#include "media/mojo/mojom/audio_data.mojom.h"
#include "media/mojo/mojom/speech_recognition.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/on_device_model/public/mojom/on_device_model.mojom.h"

namespace speech {

// Browser-side recognizer that bridges Live Caption audio from renderer
// processes directly to OnDeviceModelService (TinyGemma Audio / Gemini Nano),
// eliminating the need for an intermediate speech recognition utility process.
class OnDeviceLiveCaptionRecognizer
    : public media::mojom::SpeechRecognitionRecognizer,
      public on_device_model::mojom::AsrStreamResponder {
 public:
  OnDeviceLiveCaptionRecognizer(
      mojo::PendingRemote<media::mojom::SpeechRecognitionRecognizerClient>
          client,
      media::mojom::SpeechRecognitionOptionsPtr options,
      mojo::Remote<on_device_model::mojom::Session> session,
      mojo::PendingRemote<on_device_model::mojom::AsrStreamInput>
          asr_stream_input,
      mojo::PendingReceiver<on_device_model::mojom::AsrStreamResponder>
          asr_stream_responder,
      bool mask_offensive_words = true);
  ~OnDeviceLiveCaptionRecognizer() override;

  OnDeviceLiveCaptionRecognizer(const OnDeviceLiveCaptionRecognizer&) = delete;
  OnDeviceLiveCaptionRecognizer& operator=(
      const OnDeviceLiveCaptionRecognizer&) = delete;

  // media::mojom::SpeechRecognitionRecognizer:
  void SendAudioToSpeechRecognitionService(
      media::mojom::AudioDataS16Ptr buffer,
      std::optional<base::TimeDelta> media_start_pts) override;
  void OnLanguageChanged(const std::string& language) override;
  void OnMaskOffensiveWordsChanged(bool mask_offensive_words) override;
  void MarkDone() override;
  void UpdateRecognitionContext(
      const media::SpeechRecognitionRecognitionContext& recognition_context)
      override;

  // on_device_model::mojom::AsrStreamResponder:
  void OnResponse(
      std::vector<on_device_model::mojom::SpeechRecognitionResultPtr> results)
      override;

 private:
  void OnSpeechRecognitionRecognitionEventCallback(bool success);
  void OnAsrStreamDisconnected();
  void OnClientDisconnected();
  void DrainAudioFifo();

  mojo::Remote<media::mojom::SpeechRecognitionRecognizerClient> client_remote_;
  media::mojom::SpeechRecognitionOptionsPtr options_;
  mojo::Remote<on_device_model::mojom::Session> session_;
  mojo::Remote<on_device_model::mojom::AsrStreamInput> asr_stream_input_;
  mojo::Receiver<on_device_model::mojom::AsrStreamResponder>
      asr_stream_responder_{this};

  std::unique_ptr<media::ConvertingAudioFifo> audio_fifo_;
  int sample_rate_ = 0;
  int channel_count_ = 0;

  // Controls whether audio buffers from renderer are sent to the speech
  // recognition service. Set via the boolean response of
  // OnSpeechRecognitionRecognitionEventCallback from the client (e.g.
  // indicating whether the Live Caption bubble/UI is open/requesting
  // recognition).
  bool is_client_requesting_speech_recognition_ = true;

  // Whether offensive words should be masked in transcription results.
  // Note: For TinyGemma/Gemini Nano on-device models, offensive word filtering
  // is handled upstream/internally by the model service, but this state is
  // maintained for consistency with SpeechRecognitionRecognizer interface
  // updates.
  bool mask_offensive_words_ = true;
  std::string language_;

  base::TimeTicks last_non_empty_audio_time_ = base::TimeTicks::Now();

  // Tracks whether OnSpeechRecognitionStopped has already been dispatched to
  // client_remote_ to avoid duplicate calls.
  bool is_stopped_ = false;

  // Tracks whether MarkDone() has been called for normal termination.
  bool is_marked_done_ = false;

  base::WeakPtrFactory<OnDeviceLiveCaptionRecognizer> weak_factory_{this};
};

}  // namespace speech

#endif  // CHROME_BROWSER_SPEECH_ON_DEVICE_LIVE_CAPTION_RECOGNIZER_H_
