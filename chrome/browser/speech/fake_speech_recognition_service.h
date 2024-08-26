// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SPEECH_FAKE_SPEECH_RECOGNITION_SERVICE_H_
#define CHROME_BROWSER_SPEECH_FAKE_SPEECH_RECOGNITION_SERVICE_H_

#include <memory>
#include <string>

#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "chrome/browser/speech/chrome_speech_recognition_service.h"
#include "chrome/browser/speech/fake_speech_recognizer.h"
#include "media/base/audio_parameters.h"
#include "media/mojo/mojom/audio_data.mojom.h"
#include "media/mojo/mojom/speech_recognition.mojom.h"
#include "media/mojo/mojom/speech_recognition_service.mojom.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace speech {

// A fake SpeechRecognitionService. This service works by creating
// FakeSpeechRecognizers as self owned receivers.  If a test wants to assert
// against the state of a particular session then the fixture will need to
// implement the observer interface and add itself.  When the recognizer is
// bound the fixture can then grab a reference to the recognizer and assert
// against it. Fixtures may also want to confirm that they have the correct
// recognizer by checking the recognition type in the options struct.
class FakeSpeechRecognitionService
    : public SpeechRecognitionService,
      public media::mojom::SpeechRecognitionContext,
      public media::mojom::AudioSourceSpeechRecognitionContext {
 public:
  class Observer : public base::CheckedObserver {
   public:
    virtual void OnRecognizerBound(FakeSpeechRecognizer* bound_recognizer) = 0;
  };

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
  void BindWebSpeechRecognizer(
      mojo::PendingReceiver<media::mojom::SpeechRecognitionSession>
          session_receiver,
      mojo::PendingRemote<media::mojom::SpeechRecognitionSessionClient>
          session_client,
      mojo::PendingReceiver<media::mojom::SpeechRecognitionAudioForwarder>
          audio_forwarder,
      int channel_count,
      int sample_rate,
      media::mojom::SpeechRecognitionOptionsPtr options,
      bool continuous) override;

  // media::mojom::AudioSourceSpeechRecognitionContext:
  void BindAudioSourceFetcher(
      mojo::PendingReceiver<media::mojom::AudioSourceFetcher> fetcher_receiver,
      mojo::PendingRemote<media::mojom::SpeechRecognitionRecognizerClient>
          client,
      media::mojom::SpeechRecognitionOptionsPtr options,
      BindRecognizerCallback callback) override;

  void set_multichannel_supported(bool is_multichannel_supported) {
    is_multichannel_supported_ = is_multichannel_supported;
  }

  void AddObserver(Observer* obs) { observers_.AddObserver(obs); }

  void RemoveObsever(Observer* obs) { observers_.RemoveObserver(obs); }

 private:
  void OnRecognizerClientDisconnected();
  void OnSpeechRecognitionRecognitionEventCallback(bool success);

  std::unique_ptr<FakeSpeechRecognizer> GetNextRecognizerAndBindItsRemote(
      mojo::PendingRemote<media::mojom::SpeechRecognitionRecognizerClient>
          client,
      media::mojom::SpeechRecognitionOptionsPtr options);

  // Whether multichannel audio is supported.
  bool is_multichannel_supported_ = false;

  mojo::ReceiverSet<media::mojom::AudioSourceSpeechRecognitionContext>
      audio_source_speech_recognition_contexts_;
  mojo::ReceiverSet<media::mojom::SpeechRecognitionContext>
      speech_recognition_contexts_;

  base::ObserverList<Observer> observers_;
};

}  // namespace speech

#endif  // CHROME_BROWSER_SPEECH_FAKE_SPEECH_RECOGNITION_SERVICE_H_
