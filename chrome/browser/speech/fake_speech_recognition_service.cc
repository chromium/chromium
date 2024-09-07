// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/speech/fake_speech_recognition_service.h"

#include <memory>
#include <utility>

#include "base/notimplemented.h"
#include "base/run_loop.h"
#include "media/mojo/mojom/audio_data.mojom.h"
#include "media/mojo/mojom/media_types.mojom.h"
#include "media/mojo/mojom/speech_recognition.mojom.h"
#include "media/mojo/mojom/speech_recognition_service.mojom.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace speech {

FakeSpeechRecognitionService::FakeSpeechRecognitionService() = default;

FakeSpeechRecognitionService::~FakeSpeechRecognitionService() = default;

void FakeSpeechRecognitionService::BindAudioSourceSpeechRecognitionContext(
    mojo::PendingReceiver<media::mojom::AudioSourceSpeechRecognitionContext>
        receiver) {
  audio_source_speech_recognition_contexts_.Add(this, std::move(receiver));
}

void FakeSpeechRecognitionService::BindSpeechRecognitionContext(
    mojo::PendingReceiver<media::mojom::SpeechRecognitionContext> receiver) {
  speech_recognition_contexts_.Add(this, std::move(receiver));
}

std::unique_ptr<FakeSpeechRecognizer>
FakeSpeechRecognitionService::GetNextRecognizerAndBindItsRemote(
    mojo::PendingRemote<media::mojom::SpeechRecognitionRecognizerClient> client,
    media::mojom::SpeechRecognitionOptionsPtr options) {
  // If the test has injected a fake recognizer for assertions then use it here
  // otherwise create a new one which can be ignored.
  std::unique_ptr<FakeSpeechRecognizer> next_fake_recognizer =
      std::make_unique<FakeSpeechRecognizer>();
  next_fake_recognizer->BindRecognizerClientRemoteAndPassRecognitionOptions(
      std::move(client), std::move(options));

  for (Observer& obs : observers_) {
    obs.OnRecognizerBound(next_fake_recognizer.get());
  }

  return next_fake_recognizer;
}

void FakeSpeechRecognitionService::BindRecognizer(
    mojo::PendingReceiver<media::mojom::SpeechRecognitionRecognizer> receiver,
    mojo::PendingRemote<media::mojom::SpeechRecognitionRecognizerClient> client,
    media::mojom::SpeechRecognitionOptionsPtr options,
    BindRecognizerCallback callback) {
  // Bind the remote for the fake recognizer, then make it a self owned
  // receiver.
  mojo::MakeSelfOwnedReceiver(
      GetNextRecognizerAndBindItsRemote(std::move(client), std::move(options)),
      std::move(receiver));
  std::move(callback).Run(is_multichannel_supported_);
}

void FakeSpeechRecognitionService::BindWebSpeechRecognizer(
    mojo::PendingReceiver<media::mojom::SpeechRecognitionSession>
        session_receiver,
    mojo::PendingRemote<media::mojom::SpeechRecognitionSessionClient>
        session_client,
    mojo::PendingReceiver<media::mojom::SpeechRecognitionAudioForwarder>
        audio_forwarder,
    int channel_count,
    int sample_rate,
    media::mojom::SpeechRecognitionOptionsPtr options,
    bool continuous) {
  NOTIMPLEMENTED();
}

void FakeSpeechRecognitionService::BindAudioSourceFetcher(
    mojo::PendingReceiver<media::mojom::AudioSourceFetcher> fetcher_receiver,
    mojo::PendingRemote<media::mojom::SpeechRecognitionRecognizerClient> client,
    media::mojom::SpeechRecognitionOptionsPtr options,
    BindRecognizerCallback callback) {
  // See above, BindRecognizer.
  mojo::MakeSelfOwnedReceiver(
      GetNextRecognizerAndBindItsRemote(std::move(client), std::move(options)),
      std::move(fetcher_receiver));
  std::move(callback).Run(is_multichannel_supported_);
}

}  // namespace speech
