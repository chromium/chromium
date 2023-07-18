// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SPEECH_FAKE_SPEECH_RECOGNITION_SERVICE_H_
#define CHROME_BROWSER_SPEECH_FAKE_SPEECH_RECOGNITION_SERVICE_H_

#include <string>

#include "chrome/browser/speech/chrome_speech_recognition_service.h"
#include "media/base/audio_parameters.h"
#include "media/mojo/mojom/audio_data.mojom.h"
#include "media/mojo/mojom/speech_recognition.mojom.h"
#include "media/mojo/mojom/speech_recognition_service.mojom.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace speech {

// A fake SpeechRecognitionService. This can only be used by one client at a
// time. This class also acts as the AudioSourceFetcher receiver and
// SpeechRecognitionRecognizer receiver to allow tests to inspect whether
// methods have been called properly.
class FakeSpeechRecognitionService
    : public SpeechRecognitionService,
      public media::mojom::SpeechRecognitionContext,
      public media::mojom::SpeechRecognitionRecognizer,
      public media::mojom::AudioSourceSpeechRecognitionContext,
      public media::mojom::AudioSourceFetcher {
 public:
  FakeSpeechRecognitionService();
  FakeSpeechRecognitionService(const FakeSpeechRecognitionService&) = delete;
  FakeSpeechRecognitionService& operator=(const SpeechRecognitionService&) =
      delete;
  ~FakeSpeechRecognitionService() override;

  // SpeechRecognitionService:
  void BindSpeechRecognitionContext(
      mojo::PendingReceiver<media::mojom::SpeechRecognitionContext> receiver)
      override;
  void BindAudioSourceSpeechRecognitionContext(
      mojo::PendingReceiver<media::mojom::AudioSourceSpeechRecognitionContext>
          receiver) override;

  // media::mojom::SpeechRecognitionContext:
  void BindRecognizer(
      mojo::PendingReceiver<media::mojom::SpeechRecognitionRecognizer> receiver,
      mojo::PendingRemote<media::mojom::SpeechRecognitionRecognizerClient>
          client,
      media::mojom::SpeechRecognitionOptionsPtr options,
      BindRecognizerCallback callback) override;

  // media::mojom::AudioSourceSpeechRecognitionContext:
  void BindAudioSourceFetcher(
      mojo::PendingReceiver<media::mojom::AudioSourceFetcher> fetcher_receiver,
      mojo::PendingRemote<media::mojom::SpeechRecognitionRecognizerClient>
          client,
      media::mojom::SpeechRecognitionOptionsPtr options,
      BindRecognizerCallback callback) override;

  // media::mojom::AudioSourceFetcher:
  void Start(
      mojo::PendingRemote<media::mojom::AudioStreamFactory> stream_factory,
      const std::string& device_id,
      const ::media::AudioParameters& audio_parameters) override;
  void Stop() override;

  // media::mojom::SpeechRecognitionRecognizer:
  void SendAudioToSpeechRecognitionService(
      media::mojom::AudioDataS16Ptr buffer) override;
  void OnLanguageChanged(const std::string& language) override {}
  void OnMaskOffensiveWordsChanged(bool mask_offensive_words) override {}
  void MarkDone() override;

  // Methods for testing plumbing to SpeechRecognitionRecognizerClient.
  void SendSpeechRecognitionResult(
      const media::SpeechRecognitionResult& result);
  void SendSpeechRecognitionError();

  void WaitForRecognitionStarted();

  // Whether AudioSourceFetcher is capturing audio.
  bool is_capturing_audio() { return capturing_audio_; }

  // Whether SendAudioToSpeechRecognitionService has been called.
  bool has_received_audio() { return has_received_audio_; }

  std::string device_id() { return device_id_; }

  const absl::optional<::media::AudioParameters>& audio_parameters() {
    return audio_parameters_;
  }

  void set_multichannel_supported(bool is_multichannel_supported) {
    is_multichannel_supported_ = is_multichannel_supported;
  }

 private:
  void OnRecognizerClientDisconnected();

  void OnSpeechRecognitionRecognitionEventCallback(bool success);

  // Whether multichannel audio is supported.
  bool is_multichannel_supported_ = false;
  // Whether the AudioSourceFetcher has been started.
  bool capturing_audio_ = false;
  // Whether any audio has been sent to the SpeechRecognitionRecognizer.
  bool has_received_audio_ = false;
  // The device ID used to capture audio.
  std::string device_id_;
  // The audio parameters used to capture audio.
  absl::optional<::media::AudioParameters> audio_parameters_;

  base::OnceClosure recognition_started_closure_;

  mojo::Remote<media::mojom::SpeechRecognitionRecognizerClient>
      recognizer_client_remote_;

  mojo::ReceiverSet<media::mojom::AudioSourceSpeechRecognitionContext>
      audio_source_speech_recognition_contexts_;
  mojo::ReceiverSet<media::mojom::SpeechRecognitionContext>
      speech_recognition_contexts_;
  mojo::Receiver<media::mojom::SpeechRecognitionRecognizer>
      recognizer_receiver_{this};
  mojo::Receiver<media::mojom::AudioSourceFetcher> fetcher_receiver_{this};
};

}  // namespace speech

#endif  // CHROME_BROWSER_SPEECH_FAKE_SPEECH_RECOGNITION_SERVICE_H_
