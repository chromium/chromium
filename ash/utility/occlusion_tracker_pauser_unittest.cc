// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/utility/occlusion_tracker_pauser.h"

#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "base/run_loop.h"
#include "ui/aura/env.h"
#include "ui/aura/window_occlusion_tracker.h"
#include "ui/aura/window_tree_host.h"
#include "ui/compositor/compositor.h"

namespace ash {
namespace {

class TestObserver : public ui::CompositorAnimationObserver {
 public:
  TestObserver() = default;
  TestObserver(const TestObserver&) = delete;
  TestObserver& operator=(const TestObserver&) = delete;
  ~TestObserver() final = default;

  // ui::CompositorAnimationObserver:
  void OnAnimationStep(base::TimeTicks timestamp) override {}
  void OnCompositingShuttingDown(ui::Compositor* compositor) override {}
};

}  // namespace

using OcclusionTrackerPauserTest = AshTestBase;

TEST_F(OcclusionTrackerPauserTest, Basic) {
  aura::WindowOcclusionTracker* tracker =
      aura::Env::GetInstance()->GetWindowOcclusionTracker();

  ASSERT_FALSE(tracker->IsPaused());
  Shell::Get()->occlusion_tracker_pauser()->PauseUntilAnimationsEnd();
  EXPECT_TRUE(tracker->IsPaused());

  auto* compositor = Shell::GetPrimaryRootWindow()->GetHost()->compositor();

  TestObserver observer1, observer2;

  compositor->AddAnimationObserver(&observer1);
  EXPECT_TRUE(tracker->IsPaused());
  compositor->RemoveAnimationObserver(&observer1);
  EXPECT_FALSE(tracker->IsPaused());

  compositor->AddAnimationObserver(&observer1);
  EXPECT_FALSE(tracker->IsPaused());
  Shell::Get()->occlusion_tracker_pauser()->PauseUntilAnimationsEnd();
  EXPECT_TRUE(tracker->IsPaused());
  compositor->AddAnimationObserver(&observer2);
  EXPECT_TRUE(tracker->IsPaused());
  compositor->RemoveAnimationObserver(&observer2);
  EXPECT_TRUE(tracker->IsPaused());
  compositor->RemoveAnimationObserver(&observer1);
  EXPECT_FALSE(tracker->IsPaused());
}

TEST_F(OcclusionTrackerPauserTest, MultiDisplay) {
  aura::WindowOcclusionTracker* tracker =
      aura::Env::GetInstance()->GetWindowOcclusionTracker();
  UpdateDisplay("800x1000, 800x1000");

  auto* compositor1 = Shell::GetAllRootWindows()[0]->GetHost()->compositor();
  auto* compositor2 = Shell::GetAllRootWindows()[1]->GetHost()->compositor();

  TestObserver observer1, observer2;

  Shell::Get()->occlusion_tracker_pauser()->PauseUntilAnimationsEnd();
  EXPECT_TRUE(tracker->IsPaused());
  compositor1->AddAnimationObserver(&observer1);
  compositor2->AddAnimationObserver(&observer2);
  EXPECT_TRUE(tracker->IsPaused());
  compositor1->RemoveAnimationObserver(&observer1);
  EXPECT_TRUE(tracker->IsPaused());
  compositor2->RemoveAnimationObserver(&observer2);
  EXPECT_FALSE(tracker->IsPaused());

  // Disconnect display.
  Shell::Get()->occlusion_tracker_pauser()->PauseUntilAnimationsEnd();
  EXPECT_TRUE(tracker->IsPaused());
  compositor1->AddAnimationObserver(&observer1);
  compositor2->AddAnimationObserver(&observer2);
  EXPECT_TRUE(tracker->IsPaused());
  compositor1->RemoveAnimationObserver(&observer1);
  EXPECT_TRUE(tracker->IsPaused());
  UpdateDisplay("800x1000");
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(tracker->IsPaused());
}

}  //  namespace ash
