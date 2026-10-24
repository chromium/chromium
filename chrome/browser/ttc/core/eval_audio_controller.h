// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TTC_CORE_EVAL_AUDIO_CONTROLLER_H_
#define CHROME_BROWSER_TTC_CORE_EVAL_AUDIO_CONTROLLER_H_

#include <stdint.h>

#include <string_view>
#include <vector>

#include "base/containers/span.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "chrome/browser/ttc/app/public/audio_controller.h"
#include "media/base/audio_parameters.h"

namespace ttc {

// An in-memory AudioController for testing and evaluation harnesses.
// Injects canned audio input into observers and records downstream
// playback audio into an in-memory buffer.
class EvalAudioController : public AudioController {
 public:
  EvalAudioController();
  ~EvalAudioController() override;

  // AudioController implementation:
  void StartCapture(std::string_view device_id) override;
  void StopCapture() override;
  bool is_capturing() const override;

  void AddObserver(Observer* observer) override;
  void RemoveObserver(Observer* observer) override;

  void PlayAudio(base::span<const int16_t> pcm_data,
                 const media::AudioParameters& params,
                 int64_t sequence_number) override;
  void PlayAudio(base::span<const int16_t> pcm_data,
                 int64_t sequence_number) override;

  void ClearPlaybackQueue() override;
  void StopPlayback() override;
  bool is_playing() const override;

  // Injects PCM16 mono 16 kHz audio samples into observers.
  void InjectCapturedAudio(base::span<const int16_t> pcm_data);

  const std::vector<int16_t>& recorded_audio() const { return recorded_audio_; }
  std::vector<int16_t> TakeRecordedAudio();

  base::WeakPtr<EvalAudioController> GetWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

 private:
  bool is_capturing_ = false;
  bool is_playing_ = false;

  std::vector<int16_t> recorded_audio_;
  AudioEnvelopeExtractor audio_envelope_extractor_;

  base::ObserverList<Observer> observers_;

  base::WeakPtrFactory<EvalAudioController> weak_factory_{this};
};

}  // namespace ttc

#endif  // CHROME_BROWSER_TTC_CORE_EVAL_AUDIO_CONTROLLER_H_
