// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ttc/debug/ttc_session_audio_recorder.h"

#include <optional>
#include <vector>

#include "base/containers/span.h"
#include "base/time/time.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ttc {
namespace {

using Placement = TtcSessionAudioRecorder::Placement;
using Stream = TtcSessionAudioRecorder::Stream;
using ::testing::Each;
using ::testing::ElementsAre;

std::vector<int16_t> Chunk(size_t samples, int16_t value) {
  return std::vector<int16_t>(samples, value);
}

std::vector<int16_t> Ramp(size_t samples, int16_t first = 0) {
  std::vector<int16_t> ramp(samples);
  for (size_t i = 0; i < ramp.size(); ++i) {
    ramp[i] = static_cast<int16_t>(first + i);
  }
  return ramp;
}

std::vector<int16_t> Slice(const std::vector<int16_t>& samples,
                           size_t offset,
                           size_t count) {
  base::span<const int16_t> slice = base::span(samples).subspan(offset, count);
  return std::vector<int16_t>(slice.begin(), slice.end());
}

MATCHER_P2(IsPlacement, start, end, "") {
  return arg.start == start && arg.end == end;
}

TEST(TtcSessionAudioRecorderTest, ContiguousInputIsOneGaplessSegment) {
  TtcSessionAudioRecorder recorder(1 << 20);
  // 100 ms chunks (1600 samples at 16 kHz) arriving with jitter.
  recorder.Append(Stream::kInput, base::Milliseconds(100), Chunk(1600, 1));
  recorder.Append(Stream::kInput, base::Milliseconds(230), Chunk(1600, 2));
  recorder.Append(Stream::kInput, base::Milliseconds(290), Chunk(1600, 3));

  EXPECT_THAT(
      recorder.SegmentRanges(Stream::kInput),
      ElementsAre(IsPlacement(base::TimeDelta(), base::Milliseconds(300))));

  // Exact samples come back in order.
  std::vector<int16_t> range = recorder.GetRange(
      Stream::kInput, base::Milliseconds(0), base::Milliseconds(300));
  ASSERT_EQ(range.size(), 4800u);
  EXPECT_EQ(range[0], 1);
  EXPECT_EQ(range[1600], 2);
  EXPECT_EQ(range[4799], 3);
}

TEST(TtcSessionAudioRecorderTest, LongGapStartsNewSegment) {
  TtcSessionAudioRecorder recorder(1 << 20);
  recorder.Append(Stream::kInput, base::Milliseconds(100), Chunk(1600, 1));
  // Arrives 2 s later: a new segment starting one chunk before arrival.
  recorder.Append(Stream::kInput, base::Milliseconds(2100), Chunk(1600, 2));
  EXPECT_THAT(
      recorder.SegmentRanges(Stream::kInput),
      ElementsAre(
          IsPlacement(base::TimeDelta(), base::Milliseconds(100)),
          IsPlacement(base::Milliseconds(2000), base::Milliseconds(2100))));

  // The gap is silence.
  std::vector<int16_t> range = recorder.GetRange(
      Stream::kInput, base::Milliseconds(50), base::Milliseconds(2050));
  ASSERT_EQ(range.size(), 32000u);
  EXPECT_THAT(Slice(range, 0, 800), Each(1));
  EXPECT_THAT(Slice(range, 800, 30400), Each(0));
  EXPECT_THAT(Slice(range, 31200, 800), Each(2));

  // A range entirely inside the gap has no recorded audio.
  EXPECT_TRUE(recorder
                  .GetRange(Stream::kInput, base::Milliseconds(500),
                            base::Milliseconds(1500))
                  .empty());
}

TEST(TtcSessionAudioRecorderTest, OutputFasterThanRealTimeStaysContiguous) {
  TtcSessionAudioRecorder recorder(1 << 20);
  // Three 100 ms chunks at 24 kHz that all arrive within 10 ms.
  auto p1 = recorder.Append(Stream::kOutput, base::Milliseconds(1000),
                            Chunk(2400, 1));
  auto p2 = recorder.Append(Stream::kOutput, base::Milliseconds(1005),
                            Chunk(2400, 2));
  auto p3 = recorder.Append(Stream::kOutput, base::Milliseconds(1010),
                            Chunk(2400, 3));
  ASSERT_TRUE(p1 && p2 && p3);
  EXPECT_EQ(p1->start, base::Milliseconds(900));
  EXPECT_EQ(p2->start, p1->end);
  EXPECT_EQ(p3->end, base::Milliseconds(1200));
  EXPECT_THAT(recorder.SegmentRanges(Stream::kOutput),
              ElementsAre(IsPlacement(base::Milliseconds(900),
                                      base::Milliseconds(1200))));
}

// Chunk boundaries that aren't a whole number of microseconds are still
// consistent: a chunk ends exactly where the next one starts, and where its
// segment ends.
TEST(TtcSessionAudioRecorderTest, PlacementsAreConsistentWhenRounded) {
  TtcSessionAudioRecorder recorder(1 << 20);
  // 1 sample at 16 kHz is 62.5 us.
  std::optional<Placement> p1 =
      recorder.Append(Stream::kInput, base::Seconds(1), Chunk(1, 1));
  std::optional<Placement> p2 =
      recorder.Append(Stream::kInput, base::Seconds(1), Chunk(1, 2));
  std::optional<Placement> p3 =
      recorder.Append(Stream::kInput, base::Seconds(1), Chunk(1, 3));
  ASSERT_TRUE(p1 && p2 && p3);
  EXPECT_EQ(p1->end, p2->start);
  EXPECT_EQ(p2->end, p3->start);
  EXPECT_THAT(recorder.SegmentRanges(Stream::kInput),
              ElementsAre(IsPlacement(p1->start, p3->end)));
}

TEST(TtcSessionAudioRecorderTest, CapStopsRecording) {
  TtcSessionAudioRecorder recorder(/*cap_bytes=*/4000);
  EXPECT_TRUE(
      recorder.Append(Stream::kInput, base::Milliseconds(10), Chunk(1000, 1)));
  EXPECT_TRUE(
      recorder.Append(Stream::kOutput, base::Milliseconds(10), Chunk(500, 1)));
  // Audio that exactly fills the cap is still accepted.
  EXPECT_TRUE(
      recorder.Append(Stream::kInput, base::Milliseconds(20), Chunk(500, 1)));
  EXPECT_EQ(recorder.bytes(), 4000u);
  EXPECT_FALSE(recorder.cap_reached());
  // Any more is not.
  EXPECT_FALSE(
      recorder.Append(Stream::kInput, base::Milliseconds(30), Chunk(1, 1)));
  EXPECT_TRUE(recorder.cap_reached());
  EXPECT_EQ(recorder.bytes(), 4000u);
  // Nothing more is accepted once the cap is reached, even after discarding.
  recorder.DiscardAfter(Stream::kOutput, base::TimeDelta());
  EXPECT_FALSE(recorder.Append(Stream::kOutput, base::Seconds(1), Chunk(1, 1)));
}

TEST(TtcSessionAudioRecorderTest, CapCountsRecordedBytes) {
  TtcSessionAudioRecorder recorder(1 << 20);
  recorder.Append(Stream::kInput, base::Seconds(1), Chunk(1600, 1));
  recorder.Append(Stream::kOutput, base::Seconds(1), Chunk(2400, 1));
  EXPECT_EQ(recorder.bytes(), (1600u + 2400u) * sizeof(int16_t));
  EXPECT_EQ(recorder.cap_bytes(), 1u << 20);
}

TEST(TtcSessionAudioRecorderTest, ReadsAcrossChunks) {
  TtcSessionAudioRecorder recorder(1 << 20);
  // Two contiguous 100 ms chunks: [0, 100 ms) and [100 ms, 200 ms).
  recorder.Append(Stream::kInput, base::Milliseconds(100), Ramp(1600));
  recorder.Append(Stream::kInput, base::Milliseconds(200), Ramp(1600, 1600));
  // [90 ms, 110 ms) = samples 1440..1759, across the chunk boundary.
  std::vector<int16_t> range = recorder.GetRange(
      Stream::kInput, base::Milliseconds(90), base::Milliseconds(110));
  ASSERT_EQ(range.size(), 320u);
  EXPECT_EQ(range, Ramp(320, 1440));
}

TEST(TtcSessionAudioRecorderTest, GetRangeClampsAndHandlesEmpty) {
  TtcSessionAudioRecorder recorder(1 << 20);
  EXPECT_TRUE(recorder
                  .GetRange(Stream::kOutput, base::Milliseconds(10),
                            base::Milliseconds(5))
                  .empty());
  // Nothing is recorded, so nothing is allocated, however long the range.
  EXPECT_TRUE(recorder
                  .GetRange(Stream::kOutput, base::Milliseconds(-100),
                            base::Milliseconds(10))
                  .empty());
  EXPECT_TRUE(
      recorder.GetRange(Stream::kInput, base::TimeDelta(), base::Hours(1))
          .empty());
}

TEST(TtcSessionAudioRecorderTest, GetRangeClampsToRecordedAudio) {
  TtcSessionAudioRecorder recorder(1 << 20);
  // [0, 100 ms).
  recorder.Append(Stream::kInput, base::Milliseconds(100), Chunk(1600, 1));
  EXPECT_EQ(recorder.GetRange(Stream::kInput, base::TimeDelta(), base::Hours(1))
                .size(),
            1600u);
  EXPECT_EQ(
      recorder
          .GetRange(Stream::kInput, base::Milliseconds(50), base::Seconds(10))
          .size(),
      800u);
  // After the end of the recorded audio.
  EXPECT_TRUE(recorder
                  .GetRange(Stream::kInput, base::Milliseconds(200),
                            base::Milliseconds(300))
                  .empty());
  // The other stream has nothing recorded.
  EXPECT_TRUE(
      recorder.GetRange(Stream::kOutput, base::TimeDelta(), base::Hours(1))
          .empty());
}

TEST(TtcSessionAudioRecorderTest, SegmentRanges) {
  TtcSessionAudioRecorder recorder(1 << 20);
  EXPECT_TRUE(recorder.SegmentRanges(Stream::kInput).empty());
  recorder.Append(Stream::kInput, base::Milliseconds(100), Chunk(1600, 1));
  recorder.Append(Stream::kInput, base::Milliseconds(2100), Chunk(3200, 2));
  EXPECT_THAT(
      recorder.SegmentRanges(Stream::kInput),
      ElementsAre(
          IsPlacement(base::TimeDelta(), base::Milliseconds(100)),
          IsPlacement(base::Milliseconds(1900), base::Milliseconds(2100))));
}

TEST(TtcSessionAudioRecorderTest, PartialRangeWithinSegment) {
  TtcSessionAudioRecorder recorder(1 << 20);
  recorder.Append(Stream::kInput, base::Milliseconds(100), Ramp(1600));
  // [50 ms, 51 ms) = samples 800..815.
  std::vector<int16_t> range = recorder.GetRange(
      Stream::kInput, base::Milliseconds(50), base::Milliseconds(51));
  ASSERT_EQ(range.size(), 16u);
  EXPECT_EQ(range.front(), 800);
  EXPECT_EQ(range.back(), 815);
}

// The user interrupts a reply: only what played before then is kept.
TEST(TtcSessionAudioRecorderTest, DiscardAfterClipsInterruptedOutput) {
  TtcSessionAudioRecorder recorder(1 << 20);
  // Three 100 ms chunks, placed at [900 ms, 1200 ms).
  recorder.Append(Stream::kOutput, base::Milliseconds(1000), Chunk(2400, 1));
  recorder.Append(Stream::kOutput, base::Milliseconds(1001), Chunk(2400, 2));
  recorder.Append(Stream::kOutput, base::Milliseconds(1002), Chunk(2400, 3));

  recorder.DiscardAfter(Stream::kOutput, base::Milliseconds(1050));

  EXPECT_THAT(recorder.SegmentRanges(Stream::kOutput),
              ElementsAre(IsPlacement(base::Milliseconds(900),
                                      base::Milliseconds(1050))));
  EXPECT_EQ(recorder.bytes(), 3600u * sizeof(int16_t));
  std::vector<int16_t> range = recorder.GetRange(
      Stream::kOutput, base::Milliseconds(900), base::Milliseconds(1200));
  ASSERT_EQ(range.size(), 3600u);
  EXPECT_THAT(Slice(range, 0, 2400), Each(1));
  EXPECT_THAT(Slice(range, 2400, 1200), Each(2));

  // The next reply is placed after what was kept.
  recorder.Append(Stream::kOutput, base::Milliseconds(3000), Chunk(2400, 4));
  EXPECT_THAT(
      recorder.SegmentRanges(Stream::kOutput),
      ElementsAre(
          IsPlacement(base::Milliseconds(900), base::Milliseconds(1050)),
          IsPlacement(base::Milliseconds(2900), base::Milliseconds(3000))));
}

TEST(TtcSessionAudioRecorderTest, DiscardAfterOnlyAffectsLaterAudio) {
  TtcSessionAudioRecorder recorder(1 << 20);
  // Discarding when nothing is recorded does nothing.
  recorder.DiscardAfter(Stream::kOutput, base::TimeDelta());

  // [0, 100 ms) and [100 ms, 200 ms).
  recorder.Append(Stream::kOutput, base::Milliseconds(100), Chunk(2400, 1));
  recorder.Append(Stream::kOutput, base::Milliseconds(101), Chunk(2400, 2));
  recorder.Append(Stream::kInput, base::Milliseconds(100), Chunk(1600, 3));

  // After everything: nothing is discarded.
  recorder.DiscardAfter(Stream::kOutput, base::Seconds(1));
  EXPECT_EQ(recorder.bytes(), (2400u * 2 + 1600u) * sizeof(int16_t));

  // At a chunk boundary: the later chunk goes, the earlier one stays whole.
  recorder.DiscardAfter(Stream::kOutput, base::Milliseconds(100));
  EXPECT_THAT(
      recorder.SegmentRanges(Stream::kOutput),
      ElementsAre(IsPlacement(base::TimeDelta(), base::Milliseconds(100))));

  // Before everything: all of it goes, and the other stream is untouched.
  recorder.DiscardAfter(Stream::kOutput, base::TimeDelta());
  EXPECT_TRUE(recorder.SegmentRanges(Stream::kOutput).empty());
  EXPECT_EQ(recorder.bytes(), 1600u * sizeof(int16_t));
  EXPECT_THAT(
      recorder.SegmentRanges(Stream::kInput),
      ElementsAre(IsPlacement(base::TimeDelta(), base::Milliseconds(100))));
}

}  // namespace
}  // namespace ttc
