// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/speech/on_device_live_caption_recognizer.h"

#include <algorithm>
#include <utility>

#include "base/check.h"
#include "base/containers/to_vector.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/numerics/checked_math.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/string_util.h"
#include "media/base/audio_sample_types.h"
#include "media/base/audio_timestamp_helper.h"
#include "media/base/channel_layout.h"
#include "media/base/limits.h"
#include "mojo/public/cpp/bindings/message.h"

namespace speech {

namespace {

constexpr int kTargetOutputSampleRate = 16000;
constexpr int kTargetOutputFramesPerBuffer = 1600;
constexpr char kInvalidAudioDataError[] =
    "Invalid audio data received from renderer in Live Caption recognizer.";

}  // namespace

OnDeviceLiveCaptionRecognizer::OnDeviceLiveCaptionRecognizer(
    mojo::PendingRemote<media::mojom::SpeechRecognitionRecognizerClient> client,
    media::mojom::SpeechRecognitionOptionsPtr options,
    mojo::Remote<on_device_model::mojom::Session> session,
    mojo::PendingRemote<on_device_model::mojom::AsrStreamInput>
        asr_stream_input,
    mojo::PendingReceiver<on_device_model::mojom::AsrStreamResponder>
        asr_stream_responder,
    bool mask_offensive_words)
    : client_remote_(std::move(client)),
      options_(std::move(options)),
      session_(std::move(session)),
      mask_offensive_words_(mask_offensive_words),
      language_(options_ && options_->language && !options_->language->empty()
                    ? *options_->language
                    : "en-US") {
  if (asr_stream_input.is_valid()) {
    asr_stream_input_.Bind(std::move(asr_stream_input));
    asr_stream_input_.set_disconnect_handler(
        base::BindOnce(&OnDeviceLiveCaptionRecognizer::OnAsrStreamDisconnected,
                       weak_factory_.GetWeakPtr()));
  }
  if (asr_stream_responder.is_valid()) {
    asr_stream_responder_.Bind(std::move(asr_stream_responder));
    asr_stream_responder_.set_disconnect_handler(
        base::BindOnce(&OnDeviceLiveCaptionRecognizer::OnAsrStreamDisconnected,
                       weak_factory_.GetWeakPtr()));
  }
  if (session_.is_bound()) {
    session_.set_disconnect_handler(
        base::BindOnce(&OnDeviceLiveCaptionRecognizer::OnAsrStreamDisconnected,
                       weak_factory_.GetWeakPtr()));
  }
  if (client_remote_.is_bound()) {
    client_remote_.set_disconnect_handler(
        base::BindOnce(&OnDeviceLiveCaptionRecognizer::OnClientDisconnected,
                       weak_factory_.GetWeakPtr()));
  }
}

OnDeviceLiveCaptionRecognizer::~OnDeviceLiveCaptionRecognizer() {
  if (!is_stopped_ && client_remote_.is_bound()) {
    is_stopped_ = true;
    client_remote_->OnSpeechRecognitionStopped();
  }
}

void OnDeviceLiveCaptionRecognizer::SendAudioToSpeechRecognitionService(
    media::mojom::AudioDataS16Ptr buffer,
    std::optional<base::TimeDelta> /*media_start_pts*/) {
  if (!buffer) {
    mojo::ReportBadMessage(kInvalidAudioDataError);
    return;
  }

  if (is_marked_done_ || is_stopped_ ||
      !is_client_requesting_speech_recognition_) {
    return;
  }

  int channel_count = buffer->channel_count;
  int frame_count = buffer->frame_count;
  int sample_rate = buffer->sample_rate;
  size_t num_samples = 0;

  // Verify the channel count.
  if (channel_count <= 0 || channel_count > media::limits::kMaxChannels) {
    mojo::ReportBadMessage(kInvalidAudioDataError);
    return;
  }

  // Verify and calculate the number of samples.
  if (sample_rate < media::limits::kMinSampleRate ||
      sample_rate > media::limits::kMaxSampleRate || frame_count <= 0 ||
      frame_count > media::limits::kMaxSamplesPerPacket ||
      !base::CheckMul(frame_count, channel_count).AssignIfValid(&num_samples) ||
      num_samples != buffer->data.size()) {
    mojo::ReportBadMessage(kInvalidAudioDataError);
    return;
  }

  // Verify the buffer multiplication validity.
  if (!base::CheckMul(buffer->data.size(), sizeof(buffer->data[0])).IsValid()) {
    mojo::ReportBadMessage(kInvalidAudioDataError);
    return;
  }

  if (options_ && options_->skip_continuously_empty_audio) {
    constexpr int16_t kSilenceThresholdAmplitude = 32;
    const bool buffer_is_zero = std::all_of(
        buffer->data.begin(), buffer->data.end(),
        [](int16_t x) { return std::abs(x) <= kSilenceThresholdAmplitude; });
    const base::TimeTicks now = base::TimeTicks::Now();
    if (!buffer_is_zero) {
      last_non_empty_audio_time_ = now;
    }

    constexpr base::TimeDelta kSilenceThreshold = base::Seconds(10);
    if (now - last_non_empty_audio_time_ > kSilenceThreshold) {
      return;
    }
  }

  if (sample_rate_ != sample_rate || channel_count_ != channel_count) {
    if (audio_fifo_) {
      audio_fifo_->Flush();
      DrainAudioFifo();
    }
    audio_fifo_.reset();
  }

  channel_count_ = channel_count;
  sample_rate_ = sample_rate;

  if (!audio_fifo_) {
    media::ChannelLayoutConfig input_layout_config =
        media::ChannelLayoutConfig::Guess(channel_count_);
    if (input_layout_config.channel_layout() ==
        media::CHANNEL_LAYOUT_UNSUPPORTED) {
      input_layout_config = media::ChannelLayoutConfig(
          media::CHANNEL_LAYOUT_DISCRETE, channel_count_);
    }
    media::AudioParameters input_params(
        media::AudioParameters::AUDIO_PCM_LOW_LATENCY, input_layout_config,
        sample_rate_, frame_count);
    if (!input_params.IsValid()) {
      mojo::ReportBadMessage(kInvalidAudioDataError);
      return;
    }
    media::AudioParameters output_params(
        media::AudioParameters::AUDIO_PCM_LOW_LATENCY,
        media::ChannelLayoutConfig::Mono(), kTargetOutputSampleRate,
        kTargetOutputFramesPerBuffer);

    audio_fifo_ = std::make_unique<media::ConvertingAudioFifo>(input_params,
                                                               output_params);
  }

  auto input_bus = media::AudioBus::Create(channel_count_, frame_count);
  input_bus->FromInterleaved<media::SignedInt16SampleTypeTraits>(
      base::span(buffer->data));

  audio_fifo_->Push(std::move(input_bus));
  DrainAudioFifo();
}

void OnDeviceLiveCaptionRecognizer::DrainAudioFifo() {
  if (!audio_fifo_) {
    return;
  }
  if (!asr_stream_input_.is_bound()) {
    VLOG(1)
        << "Audio received but ASR stream input is not bound; dropping chunks.";
    while (audio_fifo_->HasOutput()) {
      audio_fifo_->PopOutput();
    }
    return;
  }
  while (audio_fifo_->HasOutput()) {
    const media::AudioBus* output_bus = audio_fifo_->PeekOutput();
    std::vector<float> resampled_samples =
        base::ToVector(output_bus->channel(0));
    audio_fifo_->PopOutput();

    auto audio_data = on_device_model::mojom::AudioData::New();
    audio_data->channel_count = 1;
    audio_data->sample_rate = kTargetOutputSampleRate;
    audio_data->frame_count =
        base::checked_cast<int32_t>(resampled_samples.size());
    audio_data->data = std::move(resampled_samples);

    asr_stream_input_->AddAudioChunk(std::move(audio_data));
  }
}

void OnDeviceLiveCaptionRecognizer::OnResponse(
    std::vector<on_device_model::mojom::SpeechRecognitionResultPtr> results) {
  if (!client_remote_.is_bound() || is_stopped_) {
    return;
  }
  for (const auto& res : results) {
    if (!res) {
      continue;
    }
    client_remote_->OnSpeechRecognitionRecognitionEvent(
        media::SpeechRecognitionResult(res->transcript, res->is_final),
        base::BindOnce(&OnDeviceLiveCaptionRecognizer::
                           OnSpeechRecognitionRecognitionEventCallback,
                       weak_factory_.GetWeakPtr()));
  }
}

void OnDeviceLiveCaptionRecognizer::OnSpeechRecognitionRecognitionEventCallback(
    bool success) {
  is_client_requesting_speech_recognition_ = success;
}

void OnDeviceLiveCaptionRecognizer::OnLanguageChanged(
    const std::string& language) {
  if (language_ == language) {
    return;
  }
  language_ = language;
  if (!session_.is_bound()) {
    return;
  }
  if (audio_fifo_) {
    audio_fifo_->Flush();
    DrainAudioFifo();
  }
  asr_stream_input_.reset();
  asr_stream_responder_.reset();

  auto asr_options = on_device_model::mojom::AsrStreamOptions::New();
  asr_options->sample_rate_hz = kTargetOutputSampleRate;
  asr_options->language = language_;

  session_->AsrStream(std::move(asr_options),
                      asr_stream_input_.BindNewPipeAndPassReceiver(),
                      asr_stream_responder_.BindNewPipeAndPassRemote());

  asr_stream_input_.set_disconnect_handler(
      base::BindOnce(&OnDeviceLiveCaptionRecognizer::OnAsrStreamDisconnected,
                     weak_factory_.GetWeakPtr()));
  asr_stream_responder_.set_disconnect_handler(
      base::BindOnce(&OnDeviceLiveCaptionRecognizer::OnAsrStreamDisconnected,
                     weak_factory_.GetWeakPtr()));
}

void OnDeviceLiveCaptionRecognizer::OnMaskOffensiveWordsChanged(
    bool mask_offensive_words) {
  mask_offensive_words_ = mask_offensive_words;
}

void OnDeviceLiveCaptionRecognizer::MarkDone() {
  is_marked_done_ = true;
  if (audio_fifo_) {
    audio_fifo_->Flush();
    DrainAudioFifo();
    audio_fifo_.reset();
  }
  if (asr_stream_input_.is_bound()) {
    asr_stream_input_.reset();
  }
}

void OnDeviceLiveCaptionRecognizer::UpdateRecognitionContext(
    const media::SpeechRecognitionRecognitionContext& /*recognition_context*/) {
}

void OnDeviceLiveCaptionRecognizer::OnAsrStreamDisconnected() {
  asr_stream_input_.reset();
  if (asr_stream_responder_.is_bound()) {
    asr_stream_responder_.reset();
  }
  if (session_.is_bound()) {
    session_.reset();
  }
  audio_fifo_.reset();
  if (client_remote_.is_bound()) {
    if (!is_marked_done_) {
      client_remote_->OnSpeechRecognitionError();
    }
    if (!is_stopped_) {
      is_stopped_ = true;
      client_remote_->OnSpeechRecognitionStopped();
    }
  }
}

void OnDeviceLiveCaptionRecognizer::OnClientDisconnected() {
  MarkDone();
  if (session_.is_bound()) {
    session_.reset();
  }
  if (asr_stream_responder_.is_bound()) {
    asr_stream_responder_.reset();
  }
  client_remote_.reset();
  is_stopped_ = true;
}

}  // namespace speech
