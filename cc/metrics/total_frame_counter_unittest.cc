// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/metrics/total_frame_counter.h"
#include "components/viz/common/frame_sinks/begin_frame_args.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cc {
namespace {

const uint64_t kSourceId = 1;

TEST(TotalFrameCounterTest, Basic) {
  TotalFrameCounter counter;
  uint64_t sequence_number = 1;
  auto frame_time = base::TimeTicks::Now();
  const auto interval = base::Milliseconds(16.67);

  auto args = viz::BeginFrameArgs::Create(
      BEGINFRAME_FROM_HERE, kSourceId, sequence_number++, frame_time,
      frame_time + interval, interval, viz::BeginFrameArgs::NORMAL);
  counter.OnShow(frame_time);
  counter.OnBeginFrame(args);

  auto advance = base::Seconds(1);
  frame_time += advance;
  counter.OnHide(frame_time);
  EXPECT_EQ(counter.total_frames(), 60u);
}

TEST(TotalFrameCounterTest, BeginFrameIntervalChange) {
  TotalFrameCounter counter;
  uint64_t sequence_number = 1;
  auto frame_time = base::TimeTicks::Now();
  // Use intervals that divide evenly into one second to avoid rounding issues.
  auto interval = base::Milliseconds(20);

  // Make the page visible at 50fps.
  auto args = viz::BeginFrameArgs::Create(
      BEGINFRAME_FROM_HERE, kSourceId, sequence_number++, frame_time,
      frame_time + interval, interval, viz::BeginFrameArgs::NORMAL);
  counter.OnShow(frame_time);
  counter.OnBeginFrame(args);

  // After 10 seconds, change the frame rate to be 100fps.
  interval = base::Milliseconds(10);
  frame_time += base::Seconds(10);
  args = viz::BeginFrameArgs::Create(
      BEGINFRAME_FROM_HERE, kSourceId, sequence_number++, frame_time,
      frame_time + interval, interval, viz::BeginFrameArgs::NORMAL);
  counter.OnBeginFrame(args);

  // Hide the page after 10 more seconds.
  auto advance = base::Seconds(10);
  frame_time += advance;
  counter.OnHide(frame_time);
  EXPECT_EQ(counter.total_frames(), 1500u);
}

TEST(TotalFrameCounterTest, VisibilityChange) {
  TotalFrameCounter counter;
  uint64_t sequence_number = 1;
  auto frame_time = base::TimeTicks::Now();
  auto interval = base::Milliseconds(16.67);

  // Make the page visible at the default frame rate.
  auto args = viz::BeginFrameArgs::Create(
      BEGINFRAME_FROM_HERE, kSourceId, sequence_number++, frame_time,
      frame_time + interval, interval, viz::BeginFrameArgs::NORMAL);
  counter.OnShow(frame_time);
  counter.OnBeginFrame(args);

  // Hide the page after 10 seconds.
  frame_time += base::Seconds(10);
  counter.OnHide(frame_time);
  EXPECT_EQ(counter.total_frames(), 600u);

  // After 20 more seconds, make the page visible again and keep it visible for
  // 5 more seconds.
  frame_time += base::Seconds(20);
  counter.OnShow(frame_time);
  args = viz::BeginFrameArgs::Create(
      BEGINFRAME_FROM_HERE, kSourceId, sequence_number++, frame_time,
      frame_time + interval, interval, viz::BeginFrameArgs::NORMAL);
  counter.OnBeginFrame(args);

  frame_time += base::Seconds(5);
  counter.OnHide(frame_time);
  EXPECT_EQ(counter.total_frames(), 900u);
}

}  // namespace
}  // namespace cc
