// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/tracing/arc_tracing_graphics_model.h"

#include <initializer_list>

#include "chrome/browser/ash/arc/tracing/arc_tracing_model.h"
#include "chrome/browser/ash/arc/tracing/present_frames_tracer.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace arc {

TEST(ArcTracingGraphicsModelTest, IncludesCommitTimestamps) {
  const std::initializer_list<int> kCommitsAbsoluteMs = {4200, 8400, 8484};
  const std::string_view kCommitsRelativeUs = "0 4200000 4284000 ";

  ArcTracingGraphicsModel model;
  ArcTracingModel common;
  PresentFramesTracer present_frames;

  for (int commit_ts : kCommitsAbsoluteMs) {
    present_frames.AddCommit(base::TimeTicks::FromUptimeMillis(commit_ts));
  }
  model.Build(common, present_frames);

  ASSERT_EQ(model.view_buffers().size(), 1ul);
  auto& buffer_events = model.view_buffers().begin()->second.buffer_events();
  ASSERT_EQ(buffer_events.size(), 1ul);

  std::ostringstream timestamp_str, types;
  for (const auto& event : buffer_events[0]) {
    timestamp_str << event.timestamp << " ";
    types << event.type << " ";
  }
  EXPECT_EQ(timestamp_str.view(), kCommitsRelativeUs);
  EXPECT_EQ(types.view(), "206 206 206 ");
}

}  // namespace arc
