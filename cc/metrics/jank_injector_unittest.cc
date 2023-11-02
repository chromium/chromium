// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/metrics/jank_injector.h"

#include <string>

#include "base/test/scoped_feature_list.h"
#include "base/test/test_simple_task_runner.h"
#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cc {
namespace {

class JankInjectorTest : public testing::Test {
 public:
  JankInjectorTest() = default;
  ~JankInjectorTest() override = default;
};

TEST_F(JankInjectorTest, Basic) {
  base::test::ScopedFeatureList features;
  features.InitFromCommandLine("JankInjectionAblation:cluster/4/percent/10",
                               std::string());

  scoped_refptr<base::TestSimpleTaskRunner> task_runner(
      new base::TestSimpleTaskRunner());

  ScopedJankInjectionEnabler enable_jank;
  JankInjector injector;
  const auto& config = injector.config();
  EXPECT_EQ(config.target_dropped_frames_percent, 10u);
  EXPECT_EQ(config.dropped_frame_cluster_size, 4u);

  const uint32_t kSourceId = 1;
  uint32_t sequence_number = 1;
  constexpr base::TimeDelta kInterval = base::Milliseconds(16);
  base::TimeTicks frame_time = base::TimeTicks::Now();
  base::TimeTicks deadline = frame_time + kInterval;

  auto args = viz::BeginFrameArgs::Create(
      BEGINFRAME_FROM_HERE, kSourceId, ++sequence_number, frame_time, deadline,
      kInterval, viz::BeginFrameArgs::NORMAL);
  // For the first frame, no janks scheduled.
  injector.ScheduleJankIfNeeded(args, task_runner.get());
  EXPECT_FALSE(task_runner->HasPendingTask());

  // Generate over 100 frames. This should cause jank to be injected 3 times.
  for (uint32_t count = 0; count < 100; ++count) {
    args.frame_time += kInterval;
    args.deadline += kInterval;
    ++args.frame_id.sequence_number;
    injector.ScheduleJankIfNeeded(args, task_runner.get());
  }

  // Jank should be injected 3 times for the 100 frames.
  EXPECT_EQ(task_runner->NumPendingTasks(), 3u);
}

}  // namespace
}  // namespace cc
