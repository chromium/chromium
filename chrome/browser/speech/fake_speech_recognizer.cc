// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/speech/fake_speech_recognizer.h"

#include "base/run_loop.h"
#include "media/mojo/mojom/audio_data.mojom.h"
#include "media/mojo/mojom/audio_logging.mojom.h"
#include "media/mojo/mojom/audio_stream_factory.mojom.h"
#include "media/mojo/mojom/media_types.mojom.h"
#include "media/mojo/mojom/speech_recognition.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace speech {

FakeSpeechRecognizer::FakeSpeechRecognizer() = default;
FakeSpeechRecognizer::~FakeSpeechRecognizer() = default;

void FakeSpeechRecognizer::BindRecognizerClientRemoteAndPassRecognitionOptions(
    mojo::PendingRemote<media::mojom::SpeechRecognitionRecognizerClient> client,
    media::mojom::SpeechRecognitionOptionsPtr recognition_options) {
  BindSpeechRecognizerClientRemote(std::move(client));
  recognition_options_ = std::move(recognition_options);
}

void FakeSpeechRecognizer::BindSpeechRecognizerClientRemote(
    mojo::PendingRemote<media::mojom::SpeechRecognitionRecognizerClient>
        client) {
  recognizer_client_remote_.Bind(std::move(client));
}

void FakeSpeechRecognizer::Start(
    mojo::PendingRemote<media::mojom::AudioStreamFactory> stream_factory,
    const std::string& device_id,
    const ::media::AudioParameters& audio_parameters) {
  capturing_audio_ = true;
  device_id_ = device_id;
  audio_parameters_ = audio_parameters;
  if (recognition_started_closure_) {
    std::move(recognition_started_closure_).Run();
  }
}

void FakeSpeechRecognizer::Stop() {
  capturing_audio_ = false;
  device_id_ = "";
  audio_parameters_ = std::nullopt;
  MarkDone();
}

void FakeSpeechRecognizer::MarkDone() {
  ASSERT_TRUE(recognizer_client_remote_.is_bound());
  recognizer_client_remote_->OnSpeechRecognitionStopped();
}

void FakeSpeechRecognizer::SendSpeechRecognitionResult(
    const media::SpeechRecognitionResult& result) {
  ASSERT_TRUE(recognizer_client_remote_.is_bound());
  EXPECT_TRUE(capturing_audio_ || has_received_audio_);
  recognizer_client_remote_->OnSpeechRecognitionRecognitionEvent(
      result,
      base::BindOnce(&FakeSpeechRecognizer::OnSpeechRecognitionEventCallback,
                     base::Unretained(this)));
}

void FakeSpeechRecognizer::SendAudioToSpeechRecognitionService(
    media::mojom::AudioDataS16Ptr buffer) {
  has_received_audio_ = true;
}

void FakeSpeechRecognizer::SendSpeechRecognitionError() {
  ASSERT_TRUE(recognizer_client_remote_.is_bound());
  recognizer_client_remote_->OnSpeechRecognitionError();
}

void FakeSpeechRecognizer::OnSpeechRecognitionEventCallback(bool success) {
  capturing_audio_ = success;
}

void FakeSpeechRecognizer::WaitForRecognitionStarted() {
  // We're already capturing audio so recognition has already started!
  if (capturing_audio_) {
    return;
  }

  base::RunLoop runner;
  recognition_started_closure_ = runner.QuitClosure();
  runner.Run();
}

}  // namespace speech
