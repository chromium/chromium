// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/tracing/arc_tracing_graphics_model.h"

#include "chrome/browser/ash/arc/tracing/arc_tracing_model.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace arc {

TEST(ArcTracingGraphicsModelTest, IncludesCommitTimestamps) {
  ArcTracingGraphicsModel model;
  ArcTracingModel common;
  TraceTimestamps commits;
  commits.Add(base::TimeTicks::FromUptimeMillis(4200));
  commits.Add(base::TimeTicks::FromUptimeMillis(8400));
  commits.Add(base::TimeTicks::FromUptimeMillis(8484));
  model.Build(common, std::move(commits));

  ASSERT_EQ(model.view_buffers().size(), 1ul);
  auto& buffer_events = model.view_buffers().begin()->second.buffer_events();
  ASSERT_EQ(buffer_events.size(), 1ul);

  std::ostringstream timestamps, types;
  for (const auto& event : buffer_events[0]) {
    timestamps << event.timestamp << " ";
    types << event.type << " ";
  }
  EXPECT_EQ(timestamps.view(), "0 4200000 4284000 ");
  EXPECT_EQ(types.view(), "206 206 206 ");
}

}  // namespace arc
