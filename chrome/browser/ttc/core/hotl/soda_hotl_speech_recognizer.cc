// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ttc/core/hotl/soda_hotl_speech_recognizer.h"

#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "base/check.h"
#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/speech/speech_recognition_service.h"
#include "chrome/services/speech/buildflags/buildflags.h"
#include "media/base/audio_parameters.h"
#include "media/mojo/mojom/audio_data.mojom.h"
#include "media/mojo/mojom/speech_recognition_result.h"

#if BUILDFLAG(ENABLE_BROWSER_SPEECH_SERVICE)
#include "chrome/browser/speech/speech_recognition_service_factory.h"
#elif BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/speech/cros_speech_recognition_service_factory.h"
#endif

namespace ttc {

namespace {

speech::SpeechRecognitionService* GetSpeechRecognitionService(
    Profile* profile) {
  if (!profile) {
    return nullptr;
  }
#if BUILDFLAG(ENABLE_BROWSER_SPEECH_SERVICE)
  return SpeechRecognitionServiceFactory::GetForProfile(profile);
#elif BUILDFLAG(IS_CHROMEOS)
  return CrosSpeechRecognitionServiceFactory::GetForProfile(profile);
#else
  return nullptr;
#endif
}

SodaHotlSpeechRecognizer::SpeechRecognitionContextBinder
CreateProfileContextBinder(Profile& profile) {
  speech::SpeechRecognitionService* service =
      GetSpeechRecognitionService(&profile);
  CHECK(service);
  return base::BindRepeating(
      [](base::WeakPtr<Profile> weak_profile,
         mojo::PendingReceiver<media::mojom::SpeechRecognitionContext>
             receiver) {
        if (!weak_profile) {
          return;
        }
        speech::SpeechRecognitionService* service =
            GetSpeechRecognitionService(weak_profile.get());
        if (service) {
          service->BindSpeechRecognitionContext(std::move(receiver));
        }
      },
      profile.GetWeakPtr());
}

}  // namespace

SodaHotlSpeechRecognizer::SodaHotlSpeechRecognizer(Profile& profile)
    : SodaHotlSpeechRecognizer(CreateProfileContextBinder(profile)) {}

SodaHotlSpeechRecognizer::SodaHotlSpeechRecognizer(
    SpeechRecognitionContextBinder context_binder)
    : context_binder_(std::move(context_binder)) {
  CHECK(context_binder_);
}

SodaHotlSpeechRecognizer::~SodaHotlSpeechRecognizer() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  Stop();
}

void SodaHotlSpeechRecognizer::Start(std::string_view locale,
                                     TranscriptCallback on_transcript,
                                     ErrorCallback on_error) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(!context_remote_.is_bound());
  on_transcript_ = std::move(on_transcript);
  on_error_ = std::move(on_error);

  context_binder_.Run(context_remote_.BindNewPipeAndPassReceiver());
  context_remote_.set_disconnect_handler(base::BindOnce(
      &SodaHotlSpeechRecognizer::HandleError, base::Unretained(this)));

  auto options = media::mojom::SpeechRecognitionOptions::New();
  options->recognition_mode = media::mojom::SpeechRecognitionMode::kCaption;
  options->enable_formatting = false;
  if (!locale.empty()) {
    options->language = std::string(locale);
  }
  options->is_server_based = false;
  options->allow_multi_language = false;
  // TODO(crbug.com/560229926): Introduce RecognizerClientType::kTtc in
  // media/mojo/mojom/ with speech OWNERS approval. SpeechRecognitionServiceImpl
  // on desktop currently requires kLiveCaption and calls
  // mojo::ReportBadMessage() for other client types.
  options->recognizer_client_type =
      media::mojom::RecognizerClientType::kLiveCaption;

  // TODO(crbug.com/560229926): Pre-download SODA and language pack via
  // SodaInstaller upon user's first TTC session start so models are pre-cached.
  // TODO(crbug.com/560229926): Validate SODA platform support across rollout
  // hardware tiers; fallback to UI button click on unsupported platforms.
  context_remote_->BindRecognizer(
      recognizer_remote_.BindNewPipeAndPassReceiver(),
      client_receiver_.BindNewPipeAndPassRemote(), std::move(options),
      base::DoNothing());
  recognizer_remote_.set_disconnect_handler(base::BindOnce(
      &SodaHotlSpeechRecognizer::HandleError, base::Unretained(this)));
  client_receiver_.set_disconnect_handler(base::BindOnce(
      &SodaHotlSpeechRecognizer::HandleError, base::Unretained(this)));
}

void SodaHotlSpeechRecognizer::Stop() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  on_transcript_.Reset();
  on_error_.Reset();
  client_receiver_.reset();
  recognizer_remote_.reset();
  context_remote_.reset();
}

void SodaHotlSpeechRecognizer::PushAudioChunk(
    base::span<const int16_t> pcm_data,
    const media::AudioParameters& params) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!recognizer_remote_.is_bound() || pcm_data.empty()) {
    return;
  }
  CHECK(params.IsValid());
  CHECK_EQ(pcm_data.size() % params.channels(), 0u);

  auto audio_data = media::mojom::AudioDataS16::New();
  audio_data->channel_count = params.channels();
  audio_data->sample_rate = params.sample_rate();
  audio_data->frame_count =
      base::checked_cast<int>(pcm_data.size() / params.channels());
  audio_data->data.assign(pcm_data.begin(), pcm_data.end());
  recognizer_remote_->SendAudioToSpeechRecognitionService(std::move(audio_data),
                                                          std::nullopt);
}

void SodaHotlSpeechRecognizer::OnSpeechRecognitionRecognitionEvent(
    const media::SpeechRecognitionResult& result,
    OnSpeechRecognitionRecognitionEventCallback reply) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::move(reply).Run(true);
  if (!result.is_final || result.transcription.empty()) {
    return;
  }

  // Copy the callback in case the handler synchronously calls Stop() or
  // destroys `this`.
  TranscriptCallback callback = on_transcript_;
  callback.Run(base::UTF8ToUTF16(result.transcription));
}

void SodaHotlSpeechRecognizer::OnSpeechRecognitionStopped() {
  HandleError();
}

void SodaHotlSpeechRecognizer::OnSpeechRecognitionError() {
  HandleError();
}

void SodaHotlSpeechRecognizer::OnLanguageIdentificationEvent(
    media::mojom::LanguageIdentificationEventPtr event) {}

void SodaHotlSpeechRecognizer::HandleError() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  ErrorCallback error_cb = std::move(on_error_);
  Stop();
  if (error_cb) {
    std::move(error_cb).Run();
  }
}

}  // namespace ttc
