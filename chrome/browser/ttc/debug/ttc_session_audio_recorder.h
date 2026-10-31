// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TTC_DEBUG_TTC_SESSION_AUDIO_RECORDER_H_
#define CHROME_BROWSER_TTC_DEBUG_TTC_SESSION_AUDIO_RECORDER_H_

#include <stddef.h>
#include <stdint.h>

#include <optional>
#include <vector>

#include "base/containers/span.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"

namespace ttc {

// Records the exact PCM16 audio sent to and received from the server during a
// TTC session, for playback in the debug tools. In memory only, bounded by a
// byte cap; once the cap is reached nothing more is recorded.
//
// Each chunk is stored as it was appended, and placed on the session timeline
// right after the previous chunk. If its inferred start (its arrival time
// minus its duration) is more than kMaxGap after the previous chunk's end, it
// is placed at its inferred start instead, leaving a gap. This keeps captured
// audio gapless despite delivery jitter, and keeps server audio (which arrives
// faster than real time) contiguous. Chunks without a gap between them form a
// segment.
class TtcSessionAudioRecorder {
 public:
  enum class Stream { kInput, kOutput };

  static constexpr int kInputSampleRate = 16000;
  static constexpr int kOutputSampleRate = 24000;
  static constexpr base::TimeDelta kMaxGap = base::Milliseconds(300);
  // Upper bound on the duration returned by a single GetRange() call.
  static constexpr base::TimeDelta kMaxRange = base::Minutes(15);

  struct Placement {
    base::TimeDelta start;
    base::TimeDelta end;
  };

  explicit TtcSessionAudioRecorder(size_t cap_bytes);
  TtcSessionAudioRecorder(const TtcSessionAudioRecorder&) = delete;
  TtcSessionAudioRecorder& operator=(const TtcSessionAudioRecorder&) = delete;
  ~TtcSessionAudioRecorder();

  static int SampleRate(Stream stream);

  // Records `pcm`, which arrived at `arrival_time` on the session timeline.
  // Returns where the chunk was placed, or nullopt if the cap is reached.
  std::optional<Placement> Append(Stream stream,
                                  base::TimeDelta arrival_time,
                                  base::span<const int16_t> pcm);

  // Discards the audio of `stream` placed after `time`. Server audio is placed
  // as if all of it played, so this is for when playback stopped early, e.g.
  // because the user interrupted the reply.
  void DiscardAfter(Stream stream, base::TimeDelta time);

  // Returns the samples of `stream` covering [start, end), with silence where
  // nothing was recorded. The range is clamped to kMaxRange and to the end of
  // the recorded audio; it is empty if no recorded audio overlaps it.
  std::vector<int16_t> GetRange(Stream stream,
                                base::TimeDelta start,
                                base::TimeDelta end) const;

  // The position of each segment of `stream` on the session timeline.
  std::vector<Placement> SegmentRanges(Stream stream) const;

  // Bytes of audio recorded, which is what the cap applies to.
  size_t bytes() const { return bytes_; }
  size_t cap_bytes() const { return cap_bytes_; }
  bool cap_reached() const { return cap_reached_; }

 private:
  // A chunk of audio as it was appended.
  struct Chunk {
    Chunk(int64_t start, base::span<const int16_t> pcm);
    Chunk(Chunk&&);
    Chunk& operator=(Chunk&&);
    ~Chunk();

    int64_t end() const;

    // Position on the session timeline, in samples of the stream.
    int64_t start;
    std::vector<int16_t> samples;
  };

  // In order on the timeline, without overlap.
  const std::vector<Chunk>& GetChunks(Stream stream) const;
  std::vector<Chunk>& GetMutableChunks(Stream stream);

  SEQUENCE_CHECKER(sequence_checker_);

  const size_t cap_bytes_;
  size_t bytes_ = 0;
  bool cap_reached_ = false;
  std::vector<Chunk> input_;
  std::vector<Chunk> output_;
};

}  // namespace ttc

#endif  // CHROME_BROWSER_TTC_DEBUG_TTC_SESSION_AUDIO_RECORDER_H_
