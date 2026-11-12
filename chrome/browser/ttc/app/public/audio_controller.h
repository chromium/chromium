// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TTC_APP_PUBLIC_AUDIO_CONTROLLER_H_
#define CHROME_BROWSER_TTC_APP_PUBLIC_AUDIO_CONTROLLER_H_

#include <stdint.h>

#include <string_view>

#include "base/containers/span.h"
#include "base/observer_list_types.h"
#include "base/time/time.h"
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
    // for visualizer UI. Only called between `OnCaptureStarted()` and
    // `OnCaptureStopped()`, including while the device is muted, when the
    // audio is silence.
    virtual void OnCapturedAudio(base::span<const int16_t> pcm_data,
                                 const media::AudioParameters& params,
                                 float energy) {}

    // Called once `StartCapture()` has requested the capture stream from the
    // audio service. The stream itself opens asynchronously after this.
    virtual void OnCaptureStarted() {}

    // Called when capture stops (see StopCapture()). No more captured audio is
    // delivered after this, so the microphone's level is silence.
    virtual void OnCaptureStopped() {}

    // Called when the system mutes or unmutes the capture device, e.g. with a
    // hardware switch or an OS setting. A muted device keeps delivering audio
    // through `OnCapturedAudio()`, as silence.
    // `at_stream_start` is true when the device was already muted as the
    // stream opened, rather than muted while capturing.
    virtual void OnCaptureMuted(bool muted, bool at_stream_start) {}

    // Called when acoustic envelope and lookahead segments are extracted for a
    // queued playback chunk.
    virtual void OnModelAudioEnvelopeReady(const AudioEnvelope& envelope) {}

    // Called as queued playback audio is rendered, reporting the duration of
    // audio just played and whether the playback queue has been fully drained.
    virtual void OnPlaybackProgress(base::TimeDelta played_duration,
                                    bool did_fully_drain_queue) {}

    // Called when an audio capture error occurs.
    virtual void OnAudioError(ErrorCode error) {}

    // Called when an audio playback/render error occurs.
    virtual void OnPlaybackError() {}
  };

  virtual ~AudioController() = default;

  // Computes the duration represented by `frames` rendered at `sample_rate`,
  // rounded up so played duration never under-reports rendered audio.
  static base::TimeDelta FramesToCeilingTimeDelta(int64_t frames,
                                                  int sample_rate);

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
                         const media::AudioParameters& params) = 0;

  // Enqueues audio data using default 24kHz mono PCM16 parameters.
  virtual void PlayAudio(base::span<const int16_t> pcm_data) = 0;

  // Instantly flushes all queued playback audio (used on barge-in /
  // interruption).
  virtual void ClearPlaybackQueue() = 0;
  virtual void StopPlayback() = 0;
  virtual bool is_playing() const = 0;
};

}  // namespace ttc

#endif  // CHROME_BROWSER_TTC_APP_PUBLIC_AUDIO_CONTROLLER_H_
