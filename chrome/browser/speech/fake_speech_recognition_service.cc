// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/speech/fake_speech_recognition_service.h"

#include "media/mojo/mojom/speech_recognition_service.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace speech {

FakeSpeechRecognitionService::FakeSpeechRecognitionService() = default;

FakeSpeechRecognitionService::~FakeSpeechRecognitionService() = default;

void FakeSpeechRecognitionService::Create(
    mojo::PendingReceiver<media::mojom::SpeechRecognitionContext> receiver) {
  speech_recognition_contexts_.Add(this, std::move(receiver));
}

void FakeSpeechRecognitionService::BindRecognizer(
    mojo::PendingReceiver<media::mojom::SpeechRecognitionRecognizer> receiver,
    mojo::PendingRemote<media::mojom::SpeechRecognitionRecognizerClient> client,
    BindRecognizerCallback callback) {
  recognizer_receiver_.Bind(std::move(receiver));
  recognizer_client_remote_.Bind(std::move(client));
  recognizer_client_remote_.set_disconnect_handler(base::BindOnce(
      &FakeSpeechRecognitionService::OnRecognizerClientDisconnected,
      base::Unretained(this)));
  std::move(callback).Run(true /* multichannel supported */);
}

void FakeSpeechRecognitionService::BindAudioSourceFetcher(
    mojo::PendingReceiver<media::mojom::AudioSourceFetcher> fetcher_receiver,
    mojo::PendingRemote<media::mojom::SpeechRecognitionRecognizerClient> client,
    mojo::PendingRemote<media::mojom::AudioStreamFactory> stream_factory,
    BindRecognizerCallback callback) {
  fetcher_receiver_.Bind(std::move(fetcher_receiver));
  recognizer_client_remote_.Bind(std::move(client));
  recognizer_client_remote_.set_disconnect_handler(base::BindOnce(
      &FakeSpeechRecognitionService::OnRecognizerClientDisconnected,
      base::Unretained(this)));
  std::move(callback).Run(true);
}

void FakeSpeechRecognitionService::Start() {
  capturing_audio_ = true;
}
void FakeSpeechRecognitionService::Stop() {
  capturing_audio_ = false;
}

void FakeSpeechRecognitionService::SendAudioToSpeechRecognitionService(
    media::mojom::AudioDataS16Ptr buffer) {
  has_received_audio_ = true;
}

void FakeSpeechRecognitionService::SendSpeechRecognitionResult(
    media::mojom::SpeechRecognitionResultPtr result) {
  ASSERT_TRUE(recognizer_client_remote_.is_bound());
  EXPECT_TRUE(capturing_audio_ || has_received_audio_);
  recognizer_client_remote_->OnSpeechRecognitionRecognitionEvent(
      std::move(result));
}

void FakeSpeechRecognitionService::SendSpeechRecognitionError() {
  ASSERT_TRUE(recognizer_client_remote_.is_bound());
  recognizer_client_remote_->OnSpeechRecognitionError();
}

void FakeSpeechRecognitionService::OnRecognizerClientDisconnected() {
  // Reset everything in case it will be re-used.
  recognizer_client_remote_.reset();
  fetcher_receiver_.reset();
  recognizer_receiver_.reset();
}

}  // namespace speech
