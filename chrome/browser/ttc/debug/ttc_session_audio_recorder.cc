// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ttc/debug/ttc_session_audio_recorder.h"

#include <algorithm>

#include "base/check_op.h"
#include "base/numerics/safe_conversions.h"

namespace ttc {

namespace {

// Rounded to the nearest microsecond, so that converting back with
// TimeToSamples() gives the same sample.
base::TimeDelta SamplesToTime(int64_t samples, int sample_rate) {
  return base::Microseconds(base::ClampRound<int64_t>(
      static_cast<double>(samples) * base::Time::kMicrosecondsPerSecond /
      sample_rate));
}

int64_t TimeToSamples(base::TimeDelta time, int sample_rate) {
  return base::ClampRound<int64_t>(time.InSecondsF() * sample_rate);
}

}  // namespace

TtcSessionAudioRecorder::Chunk::Chunk(int64_t start,
                                      base::span<const int16_t> pcm)
    : start(start), samples(pcm.begin(), pcm.end()) {}
TtcSessionAudioRecorder::Chunk::Chunk(Chunk&&) = default;
TtcSessionAudioRecorder::Chunk& TtcSessionAudioRecorder::Chunk::operator=(
    Chunk&&) = default;
TtcSessionAudioRecorder::Chunk::~Chunk() = default;

int64_t TtcSessionAudioRecorder::Chunk::end() const {
  return start + base::checked_cast<int64_t>(samples.size());
}

TtcSessionAudioRecorder::TtcSessionAudioRecorder(size_t cap_bytes)
    : cap_bytes_(cap_bytes) {}

TtcSessionAudioRecorder::~TtcSessionAudioRecorder() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

// static
int TtcSessionAudioRecorder::SampleRate(Stream stream) {
  return stream == Stream::kInput ? kInputSampleRate : kOutputSampleRate;
}

std::optional<TtcSessionAudioRecorder::Placement>
TtcSessionAudioRecorder::Append(Stream stream,
                                base::TimeDelta arrival_time,
                                base::span<const int16_t> pcm) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (pcm.empty() || cap_reached_) {
    return std::nullopt;
  }
  DCHECK_LE(bytes_, cap_bytes_);
  if (pcm.size_bytes() > cap_bytes_ - bytes_) {
    cap_reached_ = true;
    return std::nullopt;
  }

  const int rate = SampleRate(stream);
  std::vector<Chunk>& chunks = GetMutableChunks(stream);
  // The chunk was (roughly) captured, or due to start playing, one duration
  // before it arrived.
  int64_t start =
      std::max<int64_t>(0, TimeToSamples(arrival_time, rate) -
                               base::checked_cast<int64_t>(pcm.size()));
  if (!chunks.empty()) {
    const int64_t previous_end = chunks.back().end();
    if (start <= previous_end + TimeToSamples(kMaxGap, rate)) {
      start = previous_end;
    }
  }

  chunks.emplace_back(start, pcm);
  bytes_ += pcm.size_bytes();
  return Placement{SamplesToTime(start, rate),
                   SamplesToTime(chunks.back().end(), rate)};
}

void TtcSessionAudioRecorder::DiscardAfter(Stream stream,
                                           base::TimeDelta time) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  const int64_t limit =
      TimeToSamples(std::max(time, base::TimeDelta()), SampleRate(stream));
  std::vector<Chunk>& chunks = GetMutableChunks(stream);
  while (!chunks.empty() && chunks.back().start >= limit) {
    bytes_ -= chunks.back().samples.size() * sizeof(int16_t);
    chunks.pop_back();
  }
  if (!chunks.empty() && chunks.back().end() > limit) {
    std::vector<int16_t>& samples = chunks.back().samples;
    const size_t kept = base::checked_cast<size_t>(limit - chunks.back().start);
    bytes_ -= (samples.size() - kept) * sizeof(int16_t);
    samples.resize(kept);
    // So that the memory used matches bytes().
    samples.shrink_to_fit();
  }
}

std::vector<int16_t> TtcSessionAudioRecorder::GetRange(
    Stream stream,
    base::TimeDelta start,
    base::TimeDelta end) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  const std::vector<Chunk>& chunks = GetChunks(stream);
  if (chunks.empty()) {
    return {};
  }
  const int rate = SampleRate(stream);
  start = std::max(start, base::TimeDelta());
  // Never allocate beyond the recorded audio: the range comes from the page.
  end = std::min(
      {end, start + kMaxRange, SamplesToTime(chunks.back().end(), rate)});
  if (end <= start) {
    return {};
  }

  const int64_t range_start = TimeToSamples(start, rate);
  const int64_t range_end = TimeToSamples(end, rate);
  // The first chunk that ends after the range starts.
  auto chunk = std::ranges::partition_point(
      chunks, [&](const Chunk& c) { return c.end() <= range_start; });
  if (chunk == chunks.end() || chunk->start >= range_end) {
    return {};
  }

  std::vector<int16_t> out(base::checked_cast<size_t>(range_end - range_start),
                           0);
  for (; chunk != chunks.end() && chunk->start < range_end; ++chunk) {
    const int64_t copy_start = std::max(chunk->start, range_start);
    const int64_t copy_end = std::min(chunk->end(), range_end);
    base::span(out)
        .subspan(static_cast<size_t>(copy_start - range_start),
                 static_cast<size_t>(copy_end - copy_start))
        .copy_from(base::span(chunk->samples)
                       .subspan(static_cast<size_t>(copy_start - chunk->start),
                                static_cast<size_t>(copy_end - copy_start)));
  }
  return out;
}

std::vector<TtcSessionAudioRecorder::Placement>
TtcSessionAudioRecorder::SegmentRanges(Stream stream) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  const int rate = SampleRate(stream);
  std::vector<Placement> ranges;
  // Chunks start at sample 0 or later, so the first one starts a segment.
  int64_t previous_end = -1;
  for (const Chunk& chunk : GetChunks(stream)) {
    const base::TimeDelta end = SamplesToTime(chunk.end(), rate);
    if (chunk.start == previous_end) {
      ranges.back().end = end;
    } else {
      ranges.push_back({SamplesToTime(chunk.start, rate), end});
    }
    previous_end = chunk.end();
  }
  return ranges;
}

const std::vector<TtcSessionAudioRecorder::Chunk>&
TtcSessionAudioRecorder::GetChunks(Stream stream) const {
  return stream == Stream::kInput ? input_ : output_;
}

std::vector<TtcSessionAudioRecorder::Chunk>&
TtcSessionAudioRecorder::GetMutableChunks(Stream stream) {
  return stream == Stream::kInput ? input_ : output_;
}

}  // namespace ttc
