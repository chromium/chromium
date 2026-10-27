// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ttc/core/eval_audio_controller.h"

#include <algorithm>
#include <cmath>
#include <utility>

namespace ttc {

EvalAudioController::EvalAudioController() = default;
EvalAudioController::~EvalAudioController() = default;

void EvalAudioController::StartCapture(std::string_view device_id) {
  is_capturing_ = true;
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
                                    const media::AudioParameters& params,
                                    int64_t sequence_number) {
  PlayAudio(pcm_data, sequence_number);
}

void EvalAudioController::PlayAudio(base::span<const int16_t> pcm_data,
                                    int64_t sequence_number) {
  is_playing_ = true;
  recorded_audio_.insert(recorded_audio_.end(), pcm_data.begin(),
                         pcm_data.end());
  if (!pcm_data.empty()) {
    const AudioEnvelope envelope = audio_envelope_extractor_.Extract(pcm_data);
    observers_.Notify(&Observer::OnModelAudioEnvelopeReady, envelope);
  }
  observers_.Notify(&Observer::OnPlaybackCompleted, sequence_number);
}

void EvalAudioController::ClearPlaybackQueue() {
  recorded_audio_.clear();
  is_playing_ = false;
  audio_envelope_extractor_.Reset();
}

void EvalAudioController::StopPlayback() {
  is_playing_ = false;
  audio_envelope_extractor_.Reset();
}

bool EvalAudioController::is_playing() const {
  return is_playing_;
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
