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
  is_capturing_ = false;
}

bool EvalAudioController::is_capturing() const {
  return is_capturing_;
}

base::CallbackListSubscription EvalAudioController::AddAudioCaptureListener(
    AudioCaptureCallback callback) {
  return capture_callbacks_.Add(std::move(callback));
}

base::CallbackListSubscription EvalAudioController::AddAudioEnergyListener(
    AudioEnergyCallback callback) {
  return energy_callbacks_.Add(std::move(callback));
}

base::CallbackListSubscription
EvalAudioController::AddPlaybackCompletionListener(
    PlaybackCompletionCallback callback) {
  return completion_callbacks_.Add(std::move(callback));
}

base::CallbackListSubscription EvalAudioController::AddErrorListener(
    ErrorCallback callback) {
  return error_callbacks_.Add(std::move(callback));
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
  completion_callbacks_.Notify(sequence_number);
}

void EvalAudioController::ClearPlaybackQueue() {
  recorded_audio_.clear();
  is_playing_ = false;
}

void EvalAudioController::StopPlayback() {
  is_playing_ = false;
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
  energy_callbacks_.Notify(std::min(1.0f, rms * 3.0f));

  media::AudioParameters params(media::AudioParameters::AUDIO_PCM_LOW_LATENCY,
                                media::ChannelLayoutConfig::Mono(), 16000,
                                static_cast<int>(pcm_data.size()));
  capture_callbacks_.Notify(pcm_data, params);
}

std::vector<int16_t> EvalAudioController::TakeRecordedAudio() {
  return std::move(recorded_audio_);
}

}  // namespace ttc
