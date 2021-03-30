// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SPEECH_FAKE_SPEECH_RECOGNITION_SERVICE_H_
#define CHROME_BROWSER_SPEECH_FAKE_SPEECH_RECOGNITION_SERVICE_H_

#include "chrome/browser/speech/chrome_speech_recognition_service.h"
#include "media/mojo/mojom/speech_recognition_service.mojom.h"
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
      public media::mojom::AudioSourceFetcher {
 public:
  FakeSpeechRecognitionService();
  FakeSpeechRecognitionService(const FakeSpeechRecognitionService&) = delete;
  FakeSpeechRecognitionService& operator=(const SpeechRecognitionService&) =
      delete;
  ~FakeSpeechRecognitionService() override;
  void Create(mojo::PendingReceiver<media::mojom::SpeechRecognitionContext>
                  receiver) override;

  // media::mojom::SpeechRecognitionContext:
  void BindRecognizer(
      mojo::PendingReceiver<media::mojom::SpeechRecognitionRecognizer> receiver,
      mojo::PendingRemote<media::mojom::SpeechRecognitionRecognizerClient>
          client,
      BindRecognizerCallback callback) override;
  void BindAudioSourceFetcher(
      mojo::PendingReceiver<media::mojom::AudioSourceFetcher> fetcher_receiver,
      mojo::PendingRemote<media::mojom::SpeechRecognitionRecognizerClient>
          client,
      mojo::PendingRemote<media::mojom::AudioStreamFactory> stream_factory,
      BindRecognizerCallback callback) override;

  // media::mojom::AudioSourceFetcher:
  void Start() override;
  void Stop() override;

  // media::mojom::SpeechRecognitionRecognizer:
  void SendAudioToSpeechRecognitionService(
      media::mojom::AudioDataS16Ptr buffer) override;
  void OnCaptionBubbleClosed() override {}
  void AudioReceivedAfterBubbleClosed(base::TimeDelta duration) override {}

  // Methods for testing plumbing to SpeechRecognitionRecognizerClient.
  void SendSpeechRecognitionResult(
      media::mojom::SpeechRecognitionResultPtr result);
  void SendSpeechRecognitionError();

  // Whether AudioSourceFetcher is capturing audio.
  bool is_capturing_audio() { return capturing_audio_; }

  // Whether SendAudioToSpeechRecognitionService has been called.
  bool has_received_audio() { return has_received_audio_; }

 private:
  // Whether the AudioSourceFetcher has been started.
  bool capturing_audio_ = false;
  // Whether any audio has been sent to the SpeechRecognitionRecognizer.
  bool has_received_audio_ = false;
  mojo::Remote<media::mojom::SpeechRecognitionRecognizerClient>
      recognizer_client_remote_;

  mojo::Receiver<media::mojom::SpeechRecognitionContext>
      speech_recognition_context_{this};
  mojo::Receiver<media::mojom::SpeechRecognitionRecognizer>
      recognizer_receiver_{this};
  mojo::Receiver<media::mojom::AudioSourceFetcher> fetcher_receiver_{this};
};

}  // namespace speech

#endif  // CHROME_BROWSER_SPEECH_FAKE_SPEECH_RECOGNITION_SERVICE_H_
