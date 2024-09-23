// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/test/animation_metrics_test_util.h"

#include <memory>

#include "ash/shell.h"
#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/time/time.h"
#include "ui/aura/window_tree_host.h"
#include "ui/compositor/compositor.h"
#include "ui/compositor/compositor_animation_observer.h"
#include "ui/compositor/test/begin_main_frame_waiter.h"
#include "ui/compositor/test/draw_waiter_for_test.h"

namespace ash::test {
namespace {
void GiveItSomeTime(base::TimeDelta delta) {
  // Due to the |frames_to_terminate_tracker|=3 constant in
  // FrameSequenceTracker::ReportSubmitFrame we need to continue generating
  // frames to receive feedback.
  base::RepeatingTimer begin_main_frame_scheduler(
      FROM_HERE, base::Milliseconds(16), base::BindRepeating([]() {
        auto* compositor =
            Shell::GetPrimaryRootWindow()->GetHost()->compositor();
        compositor->ScheduleFullRedraw();
      }));
  begin_main_frame_scheduler.Reset();

  base::RunLoop run_loop(base::RunLoop::Type::kNestableTasksAllowed);
  auto* compositor = Shell::GetPrimaryRootWindow()->GetHost()->compositor();
  compositor->ScheduleFullRedraw();
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE, run_loop.QuitClosure(), delta);
  run_loop.Run();
}
}  // namespace

TestAnimationObserver::TestAnimationObserver(ui::Compositor* compositor)
    : compositor_(compositor) {
  compositor_->AddAnimationObserver(this);
}

TestAnimationObserver::~TestAnimationObserver() = default;

void TestAnimationObserver::OnAnimationStep(base::TimeTicks timestamp) {
  ++count_;
  if (count_ < 3) {
    compositor_->ScheduleFullRedraw();
  } else {
    compositor_->RemoveAnimationObserver(this);
  }
}

void TestAnimationObserver::OnCompositingShuttingDown(
    ui::Compositor* compositor) {}

FirstNonAnimatedFrameStartedWaiter::FirstNonAnimatedFrameStartedWaiter(
    ui::Compositor* compositor)
    : compositor_(compositor) {
  compositor->AddObserver(this);
}

FirstNonAnimatedFrameStartedWaiter::~FirstNonAnimatedFrameStartedWaiter() {
  compositor_->RemoveObserver(this);
}

void FirstNonAnimatedFrameStartedWaiter::OnFirstNonAnimatedFrameStarted(
    ui::Compositor* compositor) {
  DCHECK_EQ(compositor_, compositor);
  done_ = true;
  if (run_loop_) {
    run_loop_->Quit();
  }
}

void FirstNonAnimatedFrameStartedWaiter::Wait() {
  if (done_) {
    return;
  }

  run_loop_ = std::make_unique<base::RunLoop>(
      base::RunLoop::Type::kNestableTasksAllowed);
  run_loop_->Run();
  run_loop_.reset();
}

MetricsWaiter::MetricsWaiter(base::HistogramTester* histogram_tester,
                             std::string metrics_name)
    : histogram_tester_(histogram_tester), metrics_name_(metrics_name) {}

MetricsWaiter::~MetricsWaiter() = default;

void MetricsWaiter::Wait() {
  while (histogram_tester_->GetAllSamples(metrics_name_).empty()) {
    GiveItSomeTime(base::Milliseconds(16));
  }
}

void RunSimpleAnimation() {
  ui::Compositor* compositor =
      Shell::GetPrimaryRootWindow()->GetHost()->compositor();
  TestAnimationObserver observer(compositor);
  ui::BeginMainFrameWaiter(compositor).Wait();
  FirstNonAnimatedFrameStartedWaiter(compositor).Wait();
  ui::DrawWaiterForTest::WaitForCompositingEnded(compositor);
}

}  // namespace ash::test
