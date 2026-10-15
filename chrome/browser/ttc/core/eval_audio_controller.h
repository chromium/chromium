// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TTC_CORE_EVAL_AUDIO_CONTROLLER_H_
#define CHROME_BROWSER_TTC_CORE_EVAL_AUDIO_CONTROLLER_H_

#include <stdint.h>

#include <string_view>
#include <vector>

#include "base/callback_list.h"
#include "base/containers/span.h"
#include "chrome/browser/ttc/app/public/audio_controller.h"
#include "media/base/audio_parameters.h"

namespace ttc {

// An in-memory AudioController for testing and evaluation harnesses.
// Injects canned audio input into capture listeners and records downstream
// playback audio into an in-memory buffer.
class EvalAudioController : public AudioController {
 public:
  EvalAudioController();
  ~EvalAudioController() override;

  // AudioController implementation:
  void StartCapture(std::string_view device_id) override;
  void StopCapture() override;
  bool is_capturing() const override;

  base::CallbackListSubscription AddAudioCaptureListener(
      AudioCaptureCallback callback) override;
  base::CallbackListSubscription AddAudioEnergyListener(
      AudioEnergyCallback callback) override;
  base::CallbackListSubscription AddPlaybackCompletionListener(
      PlaybackCompletionCallback callback) override;
  base::CallbackListSubscription AddErrorListener(
      ErrorCallback callback) override;

  void PlayAudio(base::span<const int16_t> pcm_data,
                 const media::AudioParameters& params,
                 int64_t sequence_number) override;
  void PlayAudio(base::span<const int16_t> pcm_data,
                 int64_t sequence_number) override;

  void ClearPlaybackQueue() override;
  void StopPlayback() override;
  bool is_playing() const override;

  // Injects PCM16 mono 16 kHz audio samples into capture listeners.
  void InjectCapturedAudio(base::span<const int16_t> pcm_data);

  const std::vector<int16_t>& recorded_audio() const { return recorded_audio_; }
  std::vector<int16_t> TakeRecordedAudio();

 private:
  bool is_capturing_ = false;
  bool is_playing_ = false;

  std::vector<int16_t> recorded_audio_;

  base::RepeatingCallbackList<void(base::span<const int16_t>,
                                   const media::AudioParameters&)>
      capture_callbacks_;
  base::RepeatingCallbackList<void(float)> energy_callbacks_;
  base::RepeatingCallbackList<void(int64_t)> completion_callbacks_;
  base::RepeatingCallbackList<void(ErrorCode)> error_callbacks_;
};

}  // namespace ttc

#endif  // CHROME_BROWSER_TTC_CORE_EVAL_AUDIO_CONTROLLER_H_
