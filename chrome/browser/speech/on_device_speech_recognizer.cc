// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/speech/on_device_speech_recognizer.h"

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/accessibility/soda_installer.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/speech/cros_speech_recognition_service.h"
#include "chrome/browser/speech/cros_speech_recognition_service_factory.h"
#include "chrome/browser/speech/speech_recognizer_delegate.h"
#include "content/public/browser/audio_service.h"
#include "content/public/browser/browser_thread.h"
#include "media/base/bind_to_current_loop.h"
#include "media/base/media_switches.h"

bool OnDeviceSpeechRecognizer::IsOnDeviceSpeechRecognizerAvailable() {
  // IsSodaInstalled will DCHECK if kUseSodaForLiveCaption is disabled.
  // kUseSodaForLiveCaption is used to track SODA availability on-device.
  return base::FeatureList::IsEnabled(media::kUseSodaForLiveCaption) &&
         speech::SodaInstaller::GetInstance()->IsSodaInstalled();
}

OnDeviceSpeechRecognizer::OnDeviceSpeechRecognizer(
    const base::WeakPtr<SpeechRecognizerDelegate>& delegate,
    Profile* profile)
    : SpeechRecognizer(delegate),
      state_(SpeechRecognizerStatus::SPEECH_RECOGNIZER_OFF) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // Connect the SpeechRecognitionContext.
  mojo::PendingReceiver<media::mojom::SpeechRecognitionContext>
      speech_recognition_context_receiver =
          speech_recognition_context_.BindNewPipeAndPassReceiver();

  // Bind to an AudioSourceFetcher in the Speech Recognition service,
  // passing the stream factory so it can listen to mic audio.
  // TODO(crbug.com/1173135): Get input stream parameters from
  // content::CreateAudioSystemForAudioService() if possible, and pass this
  // and device_id to the AudioSourceFetcher in BindAudioSourceFetcher().
  mojo::PendingRemote<media::mojom::AudioStreamFactory> stream_factory;
  content::GetAudioServiceStreamFactoryBinder().Run(
      stream_factory.InitWithNewPipeAndPassReceiver());
  speech_recognition_context_->BindAudioSourceFetcher(
      audio_source_fetcher_.BindNewPipeAndPassReceiver(),
      speech_recognition_client_receiver_.BindNewPipeAndPassRemote(),
      std::move(stream_factory),
      media::BindToCurrentLoop(
          base::BindOnce(&OnDeviceSpeechRecognizer::OnRecognizerBound,
                         weak_factory_.GetWeakPtr())));

  CrosSpeechRecognitionServiceFactory::GetForProfile(profile)->Create(
      std::move(speech_recognition_context_receiver));

  speech_recognition_context_.set_disconnect_handler(media::BindToCurrentLoop(
      base::BindOnce(&OnDeviceSpeechRecognizer::OnRecognizerDisconnected,
                     weak_factory_.GetWeakPtr())));
}

OnDeviceSpeechRecognizer::~OnDeviceSpeechRecognizer() {
  audio_source_fetcher_->Stop();
  audio_source_fetcher_.reset();
  speech_recognition_client_receiver_.reset();
  speech_recognition_context_.reset();
}

void OnDeviceSpeechRecognizer::Start() {
  audio_source_fetcher_->Start();
  UpdateStatus(SpeechRecognizerStatus::SPEECH_RECOGNIZER_RECOGNIZING);
}

void OnDeviceSpeechRecognizer::Stop() {
  audio_source_fetcher_->Stop();
  UpdateStatus(SpeechRecognizerStatus::SPEECH_RECOGNIZER_READY);
}

void OnDeviceSpeechRecognizer::OnSpeechRecognitionRecognitionEvent(
    media::mojom::SpeechRecognitionResultPtr result) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!result->transcription.size())
    return;
  UpdateStatus(SpeechRecognizerStatus::SPEECH_RECOGNIZER_IN_SPEECH);
  delegate()->OnSpeechResult(base::UTF8ToUTF16(result->transcription),
                             result->is_final, base::nullopt);
}

void OnDeviceSpeechRecognizer::OnSpeechRecognitionError() {
  UpdateStatus(SpeechRecognizerStatus::SPEECH_RECOGNIZER_ERROR);
}

void OnDeviceSpeechRecognizer::OnRecognizerBound(bool success) {
  if (success)
    UpdateStatus(SpeechRecognizerStatus::SPEECH_RECOGNIZER_READY);
}

void OnDeviceSpeechRecognizer::OnRecognizerDisconnected() {
  UpdateStatus(SpeechRecognizerStatus::SPEECH_RECOGNIZER_ERROR);
}

void OnDeviceSpeechRecognizer::UpdateStatus(SpeechRecognizerStatus state) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (state_ == state)
    return;
  delegate()->OnSpeechRecognitionStateChanged(state);
  state_ = state;
}
