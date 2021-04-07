// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/speech/on_device_speech_recognizer.h"

#include <algorithm>

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/accessibility/soda_installer.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/speech/cros_speech_recognition_service.h"
#include "chrome/browser/speech/cros_speech_recognition_service_factory.h"
#include "chrome/browser/speech/speech_recognizer_delegate.h"
#include "content/public/browser/audio_service.h"
#include "content/public/browser/browser_thread.h"
#include "media/audio/audio_system.h"
#include "media/base/audio_parameters.h"
#include "media/base/bind_to_current_loop.h"
#include "media/base/media_switches.h"

namespace {

// Sample rate used by content::SpeechRecognizerImpl, which is used
// by NetworkSpeechRecognizer.
static constexpr int kAudioSampleRate = 16000;

// This is about how many times we want the audio callback to happen per second.
// Web speech recognition happens about 10 time per second, so we take that
// convervative number here. We can increase if it seems laggy.
static constexpr int kPollingTimesPerSecond = 10;

media::AudioParameters GetAudioParameters(
    const base::Optional<media::AudioParameters>& params,
    bool is_multichannel_supported) {
  if (params) {
    media::AudioParameters result = params.value();
    int sample_rate = params->sample_rate();
    int frames_per_buffer = std::max(params->frames_per_buffer(),
                                     sample_rate / kPollingTimesPerSecond);
    media::ChannelLayout channel_layout = is_multichannel_supported
                                              ? params->channel_layout()
                                              : media::CHANNEL_LAYOUT_MONO;
    result.Reset(params->format(), channel_layout, sample_rate,
                 frames_per_buffer);
    return result;
  }

  static_assert(kAudioSampleRate % 100 == 0,
                "Audio sample rate is not divisible by 100");
  return media::AudioParameters(
      media::AudioParameters::AUDIO_PCM_LOW_LATENCY,
      is_multichannel_supported ? media::CHANNEL_LAYOUT_STEREO
                                : media::CHANNEL_LAYOUT_MONO,
      kAudioSampleRate, kAudioSampleRate / kPollingTimesPerSecond);
}

}  // namespace

bool OnDeviceSpeechRecognizer::IsOnDeviceSpeechRecognizerAvailable(
    std::string language_or_locale) {
  // IsSodaInstalled will DCHECK if kUseSodaForLiveCaption is disabled.
  // kUseSodaForLiveCaption is used to track SODA availability on-device.
  if (!base::FeatureList::IsEnabled(media::kUseSodaForLiveCaption))
    return false;
  speech::SodaInstaller* soda_installer = speech::SodaInstaller::GetInstance();
  return soda_installer->IsSodaInstalled() &&
         soda_installer->IsLanguageInstalled(language_or_locale);
}

OnDeviceSpeechRecognizer::OnDeviceSpeechRecognizer(
    const base::WeakPtr<SpeechRecognizerDelegate>& delegate,
    Profile* profile,
    std::string language_or_locale)
    : SpeechRecognizer(delegate),
      state_(SpeechRecognizerStatus::SPEECH_RECOGNIZER_OFF),
      is_multichannel_supported_(false),
      language_or_locale_(language_or_locale),
      waiting_for_params_(false) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // Connect the SpeechRecognitionContext.
  // TODO(crbug.com/1195916): Use language_or_locale_ when starting recognition.
  mojo::PendingReceiver<media::mojom::SpeechRecognitionContext>
      speech_recognition_context_receiver =
          speech_recognition_context_.BindNewPipeAndPassReceiver();
  speech_recognition_context_->BindAudioSourceFetcher(
      audio_source_fetcher_.BindNewPipeAndPassReceiver(),
      speech_recognition_client_receiver_.BindNewPipeAndPassRemote(),
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
  // Get audio parameters from the AudioSystem, and use these to start
  // recognition from the callback.
  if (!audio_system_)
    audio_system_ = content::CreateAudioSystemForAudioService();
  waiting_for_params_ = true;
  audio_system_->GetInputStreamParameters(
      media::AudioDeviceDescription::kDefaultDeviceId,
      base::BindOnce(&OnDeviceSpeechRecognizer::StartFetchingOnInputDeviceInfo,
                     weak_factory_.GetWeakPtr()));
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

void OnDeviceSpeechRecognizer::OnLanguageIdentificationEvent(
    media::mojom::LanguageIdentificationEventPtr event) {
  // Do nothing.
}

void OnDeviceSpeechRecognizer::OnRecognizerBound(
    bool is_multichannel_supported) {
  is_multichannel_supported_ = is_multichannel_supported;
  UpdateStatus(SpeechRecognizerStatus::SPEECH_RECOGNIZER_READY);
}

void OnDeviceSpeechRecognizer::OnRecognizerDisconnected() {
  UpdateStatus(SpeechRecognizerStatus::SPEECH_RECOGNIZER_ERROR);
}

void OnDeviceSpeechRecognizer::StartFetchingOnInputDeviceInfo(
    const base::Optional<media::AudioParameters>& params) {
  // waiting_for_params_ was set before requesting audio params from the
  // AudioSystem, which returns here asynchronously. If this has changed, then
  // we shouldn't start up any more.
  if (!waiting_for_params_)
    return;
  waiting_for_params_ = false;

  // Bind to an AudioSourceFetcher in the Speech Recognition service,
  // passing the stream factory so it can listen to mic audio.
  mojo::PendingRemote<media::mojom::AudioStreamFactory> stream_factory;
  content::GetAudioServiceStreamFactoryBinder().Run(
      stream_factory.InitWithNewPipeAndPassReceiver());
  audio_source_fetcher_->Start(
      std::move(stream_factory),
      media::AudioDeviceDescription::kDefaultDeviceId,
      GetAudioParameters(params, is_multichannel_supported_));
  UpdateStatus(SpeechRecognizerStatus::SPEECH_RECOGNIZER_RECOGNIZING);
}

void OnDeviceSpeechRecognizer::UpdateStatus(SpeechRecognizerStatus state) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  waiting_for_params_ = false;
  if (state_ == state)
    return;
  delegate()->OnSpeechRecognitionStateChanged(state);
  state_ = state;
}
