// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TTC_APP_AUDIO_LOGGER_H_
#define CHROME_BROWSER_TTC_APP_AUDIO_LOGGER_H_

#include <stdint.h>

#include <vector>

#include "base/containers/circular_deque.h"
#include "base/containers/span.h"
#include "chrome/browser/ttc/core/states.h"

namespace media {
class AudioParameters;
}  // namespace media

namespace ttc {

class SessionJournal;

// Returns 16-bit signed linear PCM audio data prefixed with a standard 44-byte
// RIFF/WAVE header, concatenating `prerecord_buffer` and `buffer` directly
// into the output.
std::vector<uint8_t> EncodeWav(
    const base::circular_deque<uint8_t>& prerecord_buffer,
    base::span<const uint8_t> buffer,
    int sample_rate,
    int num_channels);

// Helper that buffers captured PCM audio chunks and logs them as a WAV clip to
// the session journal when requested.
class AudioLogger {
 public:
  AudioLogger();
  ~AudioLogger();

  AudioLogger(const AudioLogger&) = delete;
  AudioLogger& operator=(const AudioLogger&) = delete;

  // Buffers captured audio based on current speaker state. When the speaker is
  // not kUser, audio is prerecorded into a ring buffer so that speech preceding
  // the server turn detection is captured.
  void OnCapturedAudio(base::span<const uint8_t> pcm_data,
                       const media::AudioParameters& params,
                       Speaker speaker);

  // Clears any buffered audio and resets recorded parameters.
  void OnCaptureStopped();

  // Logs the prerecorded audio followed by the recorded audio to `journal` and
  // clears both buffers.
  void DumpToJournal(SessionJournal& journal);

 private:
  // Records into a fixed-size ring buffer of recent audio. This catches the
  // start of the user's speech, which begins before the user's turn is
  // reported.
  void PrerecordAudio(base::span<const uint8_t> pcm_data,
                      const media::AudioParameters& params);

  // Records into an unbounded buffer that's appended to the prerecorded audio.
  void RecordAudio(base::span<const uint8_t> pcm_data,
                   const media::AudioParameters& params);

  void UpdateParams(const media::AudioParameters& params);

  base::circular_deque<uint8_t> prerecord_buffer_;
  std::vector<uint8_t> buffer_;
  int sample_rate_ = 0;
  int num_channels_ = 0;
  bool params_changed_ = false;
};

}  // namespace ttc

#endif  // CHROME_BROWSER_TTC_APP_AUDIO_LOGGER_H_
