// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ttc/core/eval_audio_controller.h"

#include <algorithm>
#include <cmath>
#include <utility>

#include "base/check_op.h"
#include "base/time/time.h"

namespace ttc {

namespace {

constexpr int kDefaultPlaybackSampleRate = 24000;

}  // namespace

EvalAudioController::EvalAudioController() = default;
EvalAudioController::~EvalAudioController() = default;

void EvalAudioController::StartCapture(std::string_view device_id) {
  if (is_capturing_) {
    return;
  }
  is_capturing_ = true;
  observers_.Notify(&Observer::OnCaptureStarted);
}

void EvalAudioController::StopCapture() {
  if (!is_capturing_) {
    return;
  }
  is_capturing_ = false;
  observers_.Notify(&Observer::OnCaptureStopped);
}

bool EvalAudioController::is_capturing() const {
  return is_capturing_;
}

void EvalAudioController::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void EvalAudioController::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void EvalAudioController::PlayAudio(base::span<const int16_t> pcm_data,
                                    const media::AudioParameters& params) {
  recorded_audio_.insert(recorded_audio_.end(), pcm_data.begin(),
                         pcm_data.end());
  if (pcm_data.empty()) {
    return;
  }
  const AudioEnvelope envelope = audio_envelope_extractor_.Extract(pcm_data);
  observers_.Notify(&Observer::OnModelAudioEnvelopeReady, envelope);

  CHECK_GT(params.sample_rate(), 0);
  const base::TimeDelta played_duration =
      FramesToCeilingTimeDelta(pcm_data.size(), params.sample_rate());
  observers_.Notify(&Observer::OnPlaybackProgress, played_duration,
                    /*did_fully_drain_queue=*/true);
}

void EvalAudioController::PlayAudio(base::span<const int16_t> pcm_data) {
  media::AudioParameters params(
      media::AudioParameters::AUDIO_PCM_LOW_LATENCY,
      media::ChannelLayoutConfig::Mono(), kDefaultPlaybackSampleRate,
      std::max<int>(1, static_cast<int>(pcm_data.size())));
  PlayAudio(pcm_data, params);
}

void EvalAudioController::ClearPlaybackQueue() {
  recorded_audio_.clear();
  audio_envelope_extractor_.Reset();
}

void EvalAudioController::StopPlayback() {
  audio_envelope_extractor_.Reset();
}

bool EvalAudioController::is_playing() const {
  // PlayAudio() consumes each chunk and notifies OnPlaybackProgress()
  // synchronously rather than queuing audio for asynchronous output;
  // therefore, the audio is effectively played instantly so is_playing() will
  // always return false.
  return false;
}

void EvalAudioController::InjectCapturedAudio(
    base::span<const int16_t> pcm_data) {
  if (pcm_data.empty()) {
    return;
  }

  float sum_squares = 0.0f;
  for (int16_t sample : pcm_data) {
    float normalized = static_cast<float>(sample) / 32768.0f;
    sum_squares += normalized * normalized;
  }
  float rms = std::sqrt(sum_squares / pcm_data.size());
  float energy = std::min(1.0f, rms * 3.0f);

  media::AudioParameters params(media::AudioParameters::AUDIO_PCM_LOW_LATENCY,
                                media::ChannelLayoutConfig::Mono(), 16000,
                                static_cast<int>(pcm_data.size()));
  observers_.Notify(&Observer::OnCapturedAudio, pcm_data, params, energy);
}

std::vector<int16_t> EvalAudioController::TakeRecordedAudio() {
  return std::move(recorded_audio_);
}

}  // namespace ttc
