// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/utility/occlusion_tracker_pauser.h"

#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "ui/aura/env.h"
#include "ui/aura/window_occlusion_tracker.h"
#include "ui/aura/window_tree_host.h"
#include "ui/compositor/compositor.h"
#include "ui/compositor/test/begin_main_frame_waiter.h"

namespace ash {
namespace {

class TestObserver final : public ui::CompositorAnimationObserver {
 public:
  TestObserver() = default;
  TestObserver(const TestObserver&) = delete;
  TestObserver& operator=(const TestObserver&) = delete;
  ~TestObserver() override = default;

  // ui::CompositorAnimationObserver:
  void OnAnimationStep(base::TimeTicks timestamp) override {}
  void OnCompositingShuttingDown(ui::Compositor* compositor) override {}
};

}  // namespace

class OcclusionTrackerPauserTest : public AshTestBase {
 public:
  OcclusionTrackerPauserTest()
      : AshTestBase(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}
  ~OcclusionTrackerPauserTest() override = default;
};

TEST_F(OcclusionTrackerPauserTest, Basic) {
  aura::WindowOcclusionTracker* tracker =
      aura::Env::GetInstance()->GetWindowOcclusionTracker();

  ASSERT_FALSE(tracker->IsPaused());
  Shell::Get()->occlusion_tracker_pauser()->PauseUntilAnimationsEnd(
      base::TimeDelta());
  EXPECT_TRUE(tracker->IsPaused());

  auto* compositor = Shell::GetPrimaryRootWindow()->GetHost()->compositor();

  TestObserver observer1, observer2;

  compositor->AddAnimationObserver(&observer1);
  EXPECT_TRUE(tracker->IsPaused());
  compositor->RemoveAnimationObserver(&observer1);

  // Wait for BeginFrame since compositor animation notifications happen
  // on BeginFrame.
  ui::BeginMainFrameWaiter(compositor).Wait();
  EXPECT_FALSE(tracker->IsPaused());

  compositor->AddAnimationObserver(&observer1);
  EXPECT_FALSE(tracker->IsPaused());
  Shell::Get()->occlusion_tracker_pauser()->PauseUntilAnimationsEnd(
      base::TimeDelta());
  EXPECT_TRUE(tracker->IsPaused());
  compositor->AddAnimationObserver(&observer2);
  EXPECT_TRUE(tracker->IsPaused());
  compositor->RemoveAnimationObserver(&observer2);
  EXPECT_TRUE(tracker->IsPaused());
  compositor->RemoveAnimationObserver(&observer1);

  // Wait for BeginFrame since compositor animation notifications happen
  // on BeginFrame.
  ui::BeginMainFrameWaiter(compositor).Wait();
  EXPECT_FALSE(tracker->IsPaused());
}

TEST_F(OcclusionTrackerPauserTest, MultiDisplay) {
  aura::WindowOcclusionTracker* tracker =
      aura::Env::GetInstance()->GetWindowOcclusionTracker();
  UpdateDisplay("800x1000, 800x1000");

  auto* compositor1 = Shell::GetAllRootWindows()[0]->GetHost()->compositor();
  auto* compositor2 = Shell::GetAllRootWindows()[1]->GetHost()->compositor();

  TestObserver observer1, observer2;

  Shell::Get()->occlusion_tracker_pauser()->PauseUntilAnimationsEnd(
      base::TimeDelta());
  EXPECT_TRUE(tracker->IsPaused());
  compositor1->AddAnimationObserver(&observer1);
  compositor2->AddAnimationObserver(&observer2);
  EXPECT_TRUE(tracker->IsPaused());
  compositor1->RemoveAnimationObserver(&observer1);

  // Wait for BeginFrame since compositor animation notifications happen
  // on BeginFrame.
  ui::BeginMainFrameWaiter(compositor1).Wait();

  // Tracker should still be paused.
  EXPECT_TRUE(tracker->IsPaused());

  compositor2->RemoveAnimationObserver(&observer2);

  // Wait for BeginFrame since compositor animation notifications happen
  // on BeginFrame.
  ui::BeginMainFrameWaiter(compositor2).Wait();
  EXPECT_FALSE(tracker->IsPaused());

  // Disconnect display.
  Shell::Get()->occlusion_tracker_pauser()->PauseUntilAnimationsEnd(
      base::TimeDelta());
  EXPECT_TRUE(tracker->IsPaused());
  compositor1->AddAnimationObserver(&observer1);
  compositor2->AddAnimationObserver(&observer2);
  EXPECT_TRUE(tracker->IsPaused());

  // Wait for BeginFrame since compositor animation notifications happen
  // on BeginFrame.
  ui::BeginMainFrameWaiter(compositor1).Wait();
  // Tracker should still be paused.
  EXPECT_TRUE(tracker->IsPaused());

  compositor1->RemoveAnimationObserver(&observer1);
  EXPECT_TRUE(tracker->IsPaused());

  // Wait for BeginFrame since compositor animation notifications happen
  // on BeginFrame.
  ui::BeginMainFrameWaiter(compositor1).Wait();
  EXPECT_TRUE(tracker->IsPaused());

  UpdateDisplay("800x1000");
  EXPECT_TRUE(tracker->IsPaused());

  // Wait for BeginFrame since compositor animation notifications happen
  // on BeginFrame.
  ui::BeginMainFrameWaiter(compositor1).Wait();
  EXPECT_FALSE(tracker->IsPaused());
}

TEST_F(OcclusionTrackerPauserTest, MultiDisplaySingleAnimatingCompositor) {
  aura::WindowOcclusionTracker* tracker =
      aura::Env::GetInstance()->GetWindowOcclusionTracker();
  UpdateDisplay("800x1000, 800x1000");

  auto* compositor1 = Shell::GetAllRootWindows()[0]->GetHost()->compositor();
  TestObserver observer1;

  Shell::Get()->occlusion_tracker_pauser()->PauseUntilAnimationsEnd(
      base::TimeDelta());
  EXPECT_TRUE(tracker->IsPaused());
  compositor1->AddAnimationObserver(&observer1);
  EXPECT_TRUE(tracker->IsPaused());

  // Wait for BeginFrame since compositor animation notifications happen
  // on BeginFrame.
  ui::BeginMainFrameWaiter(compositor1).Wait();
  EXPECT_TRUE(tracker->IsPaused());

  compositor1->RemoveAnimationObserver(&observer1);
  EXPECT_TRUE(tracker->IsPaused());

  // Wait for BeginFrame since compositor animation notifications happen
  // on BeginFrame.
  ui::BeginMainFrameWaiter(compositor1).Wait();

  // There are no more animations, so expect the tracker to be unpaused.
  EXPECT_FALSE(tracker->IsPaused());
}

TEST_F(OcclusionTrackerPauserTest, Timeout) {
  aura::WindowOcclusionTracker* tracker =
      aura::Env::GetInstance()->GetWindowOcclusionTracker();
  UpdateDisplay("800x1000, 800x1000");

  auto* compositor1 = Shell::GetAllRootWindows()[0]->GetHost()->compositor();
  auto* compositor2 = Shell::GetAllRootWindows()[1]->GetHost()->compositor();

  // Add observer to emulate animations start/end.
  TestObserver observer1, observer2;

  Shell::Get()->occlusion_tracker_pauser()->PauseUntilAnimationsEnd(
      base::Seconds(2));
  EXPECT_TRUE(tracker->IsPaused());
  compositor1->AddAnimationObserver(&observer1);
  compositor2->AddAnimationObserver(&observer2);
  EXPECT_TRUE(tracker->IsPaused());

  // Wait for BeginFrame since compositor animation notifications happen
  // on BeginFrame.
  ui::BeginMainFrameWaiter waiter1(compositor1);
  ui::BeginMainFrameWaiter waiter2(compositor2);
  waiter1.Wait();
  waiter2.Wait();

  task_environment()->FastForwardBy(base::Seconds(1));
  EXPECT_TRUE(tracker->IsPaused());
  task_environment()->FastForwardBy(base::Seconds(2));

  EXPECT_FALSE(tracker->IsPaused());
  compositor1->RemoveAnimationObserver(&observer1);
  compositor2->RemoveAnimationObserver(&observer2);
}

}  //  namespace ash
