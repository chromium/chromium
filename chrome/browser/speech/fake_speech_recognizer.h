// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SPEECH_FAKE_SPEECH_RECOGNIZER_H_
#define CHROME_BROWSER_SPEECH_FAKE_SPEECH_RECOGNIZER_H_

#include "base/memory/weak_ptr.h"
#include "media/base/audio_parameters.h"
#include "media/mojo/mojom/audio_data.mojom.h"
#include "media/mojo/mojom/speech_recognition.mojom.h"
#include "media/mojo/mojom/speech_recognition_service.mojom.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace speech {

// A fake SpeechRecognizer which will be a self owned receiver.  Allows for
// asserting the state of a speech recognition session.
class FakeSpeechRecognizer : public media::mojom::AudioSourceFetcher,
                             public media::mojom::SpeechRecognitionRecognizer {
 public:
  FakeSpeechRecognizer();
  FakeSpeechRecognizer(const FakeSpeechRecognizer&) = delete;
  FakeSpeechRecognizer& operator=(const FakeSpeechRecognizer&) = delete;
  ~FakeSpeechRecognizer() override;

  // Binds the RecognizerClient remote to this recognizer.
  void BindRecognizerClientRemoteAndPassRecognitionOptions(
      mojo::PendingRemote<media::mojom::SpeechRecognitionRecognizerClient>
          client,
      media::mojom::SpeechRecognitionOptionsPtr recognition_options);

  // Methods for testing plumbing to SpeechRecognitionRecognizerClient.
  void SendSpeechRecognitionResult(
      const media::SpeechRecognitionResult& result);
  void SendSpeechRecognitionError();

  // media::mojom::SpeechRecognitionRecognizer:
  void SendAudioToSpeechRecognitionService(
      media::mojom::AudioDataS16Ptr buffer) override;
  void OnLanguageChanged(const std::string& language) override {}
  void OnMaskOffensiveWordsChanged(bool mask_offensive_words) override {}
  void MarkDone() override;

  // media::mojom::AudioSourceFetcher:
  void Start(
      mojo::PendingRemote<media::mojom::AudioStreamFactory> stream_factory,
      const std::string& device_id,
      const ::media::AudioParameters& audio_parameters) override;
  void Stop() override;

  // Halts test execution until after start has been called.
  void WaitForRecognitionStarted();

  // Whether AudioSourceFetcher is capturing audio.
  bool is_capturing_audio() { return capturing_audio_; }

  // Device ID used by the AudioSource Fetcher.
  std::string device_id() { return device_id_; }

  // Speech recognition options passed to this Recognizer.
  const media::mojom::SpeechRecognitionOptions* recognition_options() const {
    return recognition_options_.get();
  }

  // Audio parameters passed to this recognizer.
  const std::optional<::media::AudioParameters>& audio_parameters() const {
    return audio_parameters_;
  }

  base::WeakPtr<FakeSpeechRecognizer> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  // Bind a client to this audio fetcher.
  void BindSpeechRecognizerClientRemote(
      mojo::PendingRemote<media::mojom::SpeechRecognitionRecognizerClient>
          client);

  void OnSpeechRecognitionEventCallback(bool success);

  base::OnceClosure recognition_started_closure_;

  // Speech recognition options passed to this recognizer.
  media::mojom::SpeechRecognitionOptionsPtr recognition_options_;
  // Audio parameters associated with this recognizer.
  std::optional<::media::AudioParameters> audio_parameters_;
  // Whether the recognizer has been started.
  bool capturing_audio_ = false;
  // Whether or not the recognizer has received audio.
  bool has_received_audio_ = false;
  // The device ID used to capture audio.
  std::string device_id_;
  // Client tied to this fetcher.
  mojo::Remote<media::mojom::SpeechRecognitionRecognizerClient>
      recognizer_client_remote_;

  base::WeakPtrFactory<FakeSpeechRecognizer> weak_ptr_factory_{this};
};

}  // namespace speech

#endif  // CHROME_BROWSER_SPEECH_FAKE_SPEECH_RECOGNIZER_H_
