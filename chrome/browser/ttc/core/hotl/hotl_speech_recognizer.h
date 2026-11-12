// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TTC_CORE_HOTL_HOTL_SPEECH_RECOGNIZER_H_
#define CHROME_BROWSER_TTC_CORE_HOTL_HOTL_SPEECH_RECOGNIZER_H_

#include <stdint.h>

#include <string_view>

#include "base/containers/span.h"
#include "base/functional/callback.h"

namespace media {
class AudioParameters;
}  // namespace media

namespace ttc {

// Interface for local speech recognition during TTC Human-on-the-Loop (HOTL)
// action confirmation.
class HotlSpeechRecognizer {
 public:
  using TranscriptCallback = base::RepeatingCallback<void(std::u16string_view)>;
  using ErrorCallback = base::OnceClosure;

  virtual ~HotlSpeechRecognizer() = default;

  // Starts local speech recognition for `locale` (BCP-47 tag, e.g. "en-US";
  // if empty, defaults to the user's Live Caption language preference).
  // Invokes `on_transcript` whenever a finalized speech segment arrives, or
  // `on_error` if local recognition cannot start or disconnects.
  virtual void Start(std::string_view locale,
                     TranscriptCallback on_transcript,
                     ErrorCallback on_error) = 0;

  // Stops any active recognition session and resets pending callbacks.
  virtual void Stop() = 0;

  // Forwards a captured signed 16-bit PCM audio chunk to the local speech
  // recognizer.
  virtual void PushAudioChunk(base::span<const int16_t> pcm_data,
                              const media::AudioParameters& params) = 0;
};

}  // namespace ttc

#endif  // CHROME_BROWSER_TTC_CORE_HOTL_HOTL_SPEECH_RECOGNIZER_H_
