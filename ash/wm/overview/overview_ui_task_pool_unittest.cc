// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/overview/overview_ui_task_pool.h"

#include <memory>
#include <vector>

#include "base/location.h"
#include "base/run_loop.h"
#include "base/test/run_until.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/test/test_compositor_host.h"
#include "ui/compositor/test/test_context_factories.h"
#include "ui/compositor/test/test_utils.h"

namespace ash {
namespace {

constexpr double kTestCompositorFrameRate = 60;
constexpr base::TimeDelta kTestBlackoutPeriod = base::Milliseconds(100);

class OverviewUiTaskPoolTest : public ::testing::Test {
 protected:
  void SetUp() override {
    context_factories_ = std::make_unique<ui::TestContextFactories>(false);
    context_factories_->GetContextFactory()->SetRefreshRateForTests(
        kTestCompositorFrameRate);
    const gfx::Rect bounds(100, 100);
    host_.reset(ui::TestCompositorHost::Create(
        bounds, context_factories_->GetContextFactory()));
    host_->Show();
    host_->GetCompositor()->SetRootLayer(&root_);
    frame_interval_ =
        context_factories_->GetContextFactory()->GetDisplayVSyncTimeInterval(
            host_->GetCompositor());
    // Simulates an animation. To avoid test flakiness, request new frames
    // at a rate higher than they're generated. This does not effect the rate
    // at which begin frames are actually generated, but ensures that every
    // begin frame received results in a compositor draw.
    frame_request_timer_.Start(FROM_HERE, frame_interval_ / 2, this,
                               &OverviewUiTaskPoolTest::SchdeduleNewFrame);
  }

  void TearDown() override {
    frame_request_timer_.Stop();
    host_.reset();
    context_factories_.reset();
  }

  void SchdeduleNewFrame() { host_->GetCompositor()->ScheduleFullRedraw(); }

  size_t GetTasksRemaining() {
    int tasks_remaining = 0;
    for (const auto& task : tasks_) {
      if (!task->IsReady()) {
        ++tasks_remaining;
      }
    }
    return tasks_remaining;
  }

  bool AllTasksHaveCompletionStatus(bool expected_completion_status) {
    const size_t tasks_remaining = GetTasksRemaining();
    return expected_completion_status ? tasks_remaining == 0
                                      : tasks_remaining == tasks_.size();
  }

  void AddTask() {
    tasks_.push_back(std::make_unique<base::test::TestFuture<void>>());
    CHECK(task_pool_);
    task_pool_->AddTask(tasks_.back()->GetCallback());
  }

  void InitTaskPool() {
    task_pool_ = std::make_unique<OverviewUiTaskPool>(host_->GetCompositor(),
                                                      kTestBlackoutPeriod);
  }

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::MainThreadType::UI};

  std::unique_ptr<OverviewUiTaskPool> task_pool_;
  ui::Layer root_;
  std::unique_ptr<ui::TestContextFactories> context_factories_;
  std::unique_ptr<ui::TestCompositorHost> host_;
  base::TimeDelta frame_interval_;
  base::RepeatingTimer frame_request_timer_;
  std::vector<std::unique_ptr<base::test::TestFuture<void>>> tasks_;
};

TEST_F(OverviewUiTaskPoolTest, RunsAllTasksAfterBlackoutPeriod) {
  base::RunLoop blackout_run_loop;
  task_environment_.GetMainThreadTaskRunner()->PostDelayedTask(
      FROM_HERE, blackout_run_loop.QuitClosure(), kTestBlackoutPeriod);

  InitTaskPool();
  const int kNumTasks = 5;
  for (int i = 0; i < kNumTasks; ++i) {
    AddTask();
  }
  blackout_run_loop.Run();
  EXPECT_TRUE(AllTasksHaveCompletionStatus(false));

  EXPECT_TRUE(base::test::RunUntil(
      [this]() { return AllTasksHaveCompletionStatus(true); }));
}

TEST_F(OverviewUiTaskPoolTest, FlushesTasks) {
  InitTaskPool();
  AddTask();
  AddTask();

  ASSERT_TRUE(
      base::test::RunUntil([this]() { return GetTasksRemaining() == 1; }));
  task_pool_->Flush();
  EXPECT_TRUE(AllTasksHaveCompletionStatus(true));
}

TEST_F(OverviewUiTaskPoolTest, TaskPoolDestroyedWithPendingTasks) {
  InitTaskPool();
  AddTask();
  AddTask();

  ASSERT_TRUE(
      base::test::RunUntil([this]() { return GetTasksRemaining() == 1; }));
  task_pool_.reset();
  EXPECT_EQ(GetTasksRemaining(), 1u);
}

}  // namespace
}  // namespace ash
