// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TTC_APP_PUBLIC_AUDIO_CONTROLLER_H_
#define CHROME_BROWSER_TTC_APP_PUBLIC_AUDIO_CONTROLLER_H_

#include <stdint.h>

#include <string_view>

#include "base/containers/span.h"
#include "base/observer_list_types.h"
#include "chrome/browser/ttc/app/public/audio_envelope.h"
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
  class Observer : public base::CheckedObserver {
   public:
    // Called with each chunk of captured microphone audio. Audio is always
    // delivered in AudioControllerImpl::GetBackendInputAudioParameters()
    // format (16kHz mono PCM16), and energy is its RMS energy (0.0 to 1.0)
    // for visualizer UI.
    virtual void OnCapturedAudio(base::span<const int16_t> pcm_data,
                                 const media::AudioParameters& params,
                                 float energy) {}

    // Called when capture stops (see StopCapture()). No more captured audio is
    // delivered after this, so the microphone's level is silence.
    virtual void OnCaptureStopped() {}

    // Called when acoustic envelope and lookahead segments are extracted for a
    // queued playback chunk.
    virtual void OnModelAudioEnvelopeReady(const AudioEnvelope& envelope) {}

    // Called when an audio chunk sequence finishes playback.
    virtual void OnPlaybackCompleted(int64_t sequence_number) {}

    // Called when an audio capture error occurs.
    virtual void OnAudioError(ErrorCode error) {}
  };

  virtual ~AudioController() = default;

  virtual void AddObserver(Observer* observer) = 0;
  virtual void RemoveObserver(Observer* observer) = 0;

  // --- Audio Capture (Microphone Input) ---

  // Starts capturing microphone audio from `device_id` (or default if empty).
  // The device's parameters are queried asynchronously, so the capture stream
  // is only opened once they have been received.
  virtual void StartCapture(std::string_view device_id) = 0;
  // Stops capturing. Audio captured before this but not yet delivered to
  // observers is dropped.
  virtual void StopCapture() = 0;
  // Whether capture has been started and not stopped since.
  virtual bool is_capturing() const = 0;

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
