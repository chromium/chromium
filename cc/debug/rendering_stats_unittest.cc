// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/time/time.h"
#include "base/values.h"
#include "cc/debug/rendering_stats.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cc {
namespace {

static std::string ToString(const RenderingStats::TimeDeltaList& list) {
  base::trace_event::TracedValueJSON value;
  list.AddToTracedValue("list_value", &value);
  return value.ToJSON();
}

TEST(RenderingStatsTest, TimeDeltaListEmpty) {
  RenderingStats::TimeDeltaList time_delta_list;
  EXPECT_EQ("{\"list_value\":[]}", ToString(time_delta_list));
}

TEST(RenderingStatsTest, TimeDeltaListNonEmpty) {
  RenderingStats::TimeDeltaList time_delta_list;
  time_delta_list.Append(base::Milliseconds(234));
  time_delta_list.Append(base::Milliseconds(827));

  EXPECT_EQ("{\"list_value\":[234.0,827.0]}", ToString(time_delta_list));
}

TEST(RenderingStatsTest, TimeDeltaListAdd) {
  RenderingStats::TimeDeltaList time_delta_list_a;
  time_delta_list_a.Append(base::Milliseconds(810));
  time_delta_list_a.Append(base::Milliseconds(32));

  RenderingStats::TimeDeltaList time_delta_list_b;
  time_delta_list_b.Append(base::Milliseconds(43));
  time_delta_list_b.Append(base::Milliseconds(938));
  time_delta_list_b.Append(base::Milliseconds(2));

  time_delta_list_a.Add(time_delta_list_b);
  EXPECT_EQ("{\"list_value\":[810.0,32.0,43.0,938.0,2.0]}",
            ToString(time_delta_list_a));
}

}  // namespace
}  // namespace cc
