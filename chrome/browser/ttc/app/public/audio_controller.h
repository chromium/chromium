// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TTC_APP_PUBLIC_AUDIO_CONTROLLER_H_
#define CHROME_BROWSER_TTC_APP_PUBLIC_AUDIO_CONTROLLER_H_

#include <stdint.h>

#include <string_view>

#include "base/callback_list.h"
#include "base/containers/span.h"
#include "base/functional/callback.h"
#include "components/ttc/app/public/error_codes.h"
#include "media/base/audio_parameters.h"

namespace ttc {

// Abstract interface for Audio I/O in TTC.
// Manages microphone capture and speaker playback.
//
// TODO(crbug.com/563130426): Move to components/ttc/app/public/ alongside
// Conversation to allow e.g. iOS to provide their own AudioController.
class AudioController {
 public:
  // `pcm_data` holds signed 16-bit interleaved PCM samples.
  using AudioCaptureCallback =
      base::RepeatingCallback<void(base::span<const int16_t> pcm_data,
                                   const media::AudioParameters& params)>;
  using AudioEnergyCallback = base::RepeatingCallback<void(float energy)>;
  using PlaybackCompletionCallback =
      base::RepeatingCallback<void(int64_t sequence_number)>;
  using ErrorCallback = base::RepeatingCallback<void(ErrorCode error_code)>;

  virtual ~AudioController() = default;

  // --- Audio Capture (Microphone Input) ---

  // Starts capturing microphone audio from `device_id` (or default if empty).
  // The device's parameters are queried asynchronously, so the capture stream
  // is only opened once they have been received.
  virtual void StartCapture(std::string_view device_id) = 0;
  virtual void StopCapture() = 0;
  virtual bool is_capturing() const = 0;

  // Subscribes to incoming captured audio frames. Audio is always delivered in
  // AudioControllerImpl::GetBackendInputAudioParameters() format (16kHz mono
  // PCM16).
  virtual base::CallbackListSubscription AddAudioCaptureListener(
      AudioCaptureCallback callback) = 0;

  // Subscribes to calculated RMS audio energy (0.0 to 1.0) of captured
  // microphone audio for visualizer UI.
  virtual base::CallbackListSubscription AddAudioEnergyListener(
      AudioEnergyCallback callback) = 0;

  // Subscribes to notifications when an audio chunk sequence finishes playback.
  virtual base::CallbackListSubscription AddPlaybackCompletionListener(
      PlaybackCompletionCallback callback) = 0;

  // Subscribes to notifications when an audio capture error occurs.
  virtual base::CallbackListSubscription AddErrorListener(
      ErrorCallback callback) = 0;

  // --- Audio Playback (Speaker Output) ---

  // Enqueues signed PCM16 audio data to be rendered natively with specific
  // parameters.
  virtual void PlayAudio(base::span<const int16_t> pcm_data,
                         const media::AudioParameters& params,
                         int64_t sequence_number) = 0;

  // Enqueues audio data using default 24kHz mono PCM16 parameters.
  virtual void PlayAudio(base::span<const int16_t> pcm_data,
                         int64_t sequence_number) = 0;

  // Instantly flushes all queued playback audio (used on barge-in /
  // interruption).
  virtual void ClearPlaybackQueue() = 0;
  virtual void StopPlayback() = 0;
  virtual bool is_playing() const = 0;
};

}  // namespace ttc

#endif  // CHROME_BROWSER_TTC_APP_PUBLIC_AUDIO_CONTROLLER_H_
