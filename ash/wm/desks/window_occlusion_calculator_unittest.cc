// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/desks/window_occlusion_calculator.h"

#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/wm/desks/desks_controller.h"
#include "ui/aura/window.h"
#include "ui/aura/window_occlusion_tracker.h"

namespace ash {

class WindowOcclusionCalculatorTest : public AshTestBase {
 public:
  WindowOcclusionCalculatorTest() = default;
  WindowOcclusionCalculatorTest(const WindowOcclusionCalculatorTest&) = delete;
  WindowOcclusionCalculatorTest& operator=(
      const WindowOcclusionCalculatorTest&) = delete;
  ~WindowOcclusionCalculatorTest() override = default;

  void SetUp() override { AshTestBase::SetUp(); }

  void TearDown() override {
    calculator_.reset();
    AshTestBase::TearDown();
  }

  void CreateCalculator() {
    calculator_ = std::make_unique<WindowOcclusionCalculator>();
  }

  aura::Window* GetActiveDeskContainer() {
    return DesksController::Get()->active_desk()->GetDeskContainerForRoot(
        Shell::GetPrimaryRootWindow());
  }

 protected:
  std::unique_ptr<WindowOcclusionCalculator> calculator_;
};

TEST_F(WindowOcclusionCalculatorTest, BasicOcclusionStateIsCorrect) {
  // Create windows first, so they are not locked initially and can compute
  // their true occlusion states.
  auto win0 = CreateWindowWithAppType(chromeos::AppType::NON_APP,
                                      gfx::Rect(10, 10, 100, 100));
  auto win1 = CreateWindowWithAppType(chromeos::AppType::NON_APP,
                                      gfx::Rect(10, 10, 100, 100));

  aura::Window* container = GetActiveDeskContainer();
  container->StackChildAtTop(win1.get());

  win0->TrackOcclusionState();
  win1->TrackOcclusionState();

  CreateCalculator();

  calculator_->SnapshotOcclusionStateForWindows({container});

  // win1 should be visible, win0 should be occluded.
  EXPECT_EQ(calculator_->GetOcclusionState(win1.get()),
            aura::Window::OcclusionState::VISIBLE);
  EXPECT_EQ(calculator_->GetOcclusionState(win0.get()),
            aura::Window::OcclusionState::OCCLUDED);
}

TEST_F(WindowOcclusionCalculatorTest, SnapshotRemainsStaticAfterWindowMoved) {
  auto win0 = CreateWindowWithAppType(chromeos::AppType::NON_APP,
                                      gfx::Rect(10, 10, 100, 100));
  auto win1 = CreateWindowWithAppType(chromeos::AppType::NON_APP,
                                      gfx::Rect(10, 10, 100, 100));
  aura::Window* container = GetActiveDeskContainer();
  container->StackChildAtTop(win1.get());

  win0->TrackOcclusionState();
  win1->TrackOcclusionState();

  CreateCalculator();

  calculator_->SnapshotOcclusionStateForWindows({container});

  EXPECT_EQ(calculator_->GetOcclusionState(win1.get()),
            aura::Window::OcclusionState::VISIBLE);
  EXPECT_EQ(calculator_->GetOcclusionState(win0.get()),
            aura::Window::OcclusionState::OCCLUDED);

  // Move win1 so it no longer occludes win0.
  win1->SetBounds(gfx::Rect(120, 10, 100, 100));

  // The calculator should STILL return OCCLUDED for win0 because we haven't
  // taken a new snapshot.
  EXPECT_EQ(calculator_->GetOcclusionState(win0.get()),
            aura::Window::OcclusionState::OCCLUDED);

  // Force cache invalidation by notifying content changed.
  auto* desks_controller = DesksController::Get();
  desks_controller->desks()[desks_controller->GetActiveDeskIndex()]
      ->NotifyContentChanged();

  // Snapshot again.
  calculator_->SnapshotOcclusionStateForWindows({container});

  // Now win0 should be visible.
  EXPECT_EQ(calculator_->GetOcclusionState(win0.get()),
            aura::Window::OcclusionState::VISIBLE);
}

TEST_F(WindowOcclusionCalculatorTest, SnapshotUpdatesAfterWindowDeleted) {
  auto win0 = CreateWindowWithAppType(chromeos::AppType::NON_APP,
                                      gfx::Rect(10, 10, 100, 100));
  auto win1 = CreateWindowWithAppType(chromeos::AppType::NON_APP,
                                      gfx::Rect(10, 10, 100, 100));
  aura::Window* container = GetActiveDeskContainer();
  container->StackChildAtTop(win1.get());

  win0->TrackOcclusionState();
  win1->TrackOcclusionState();

  CreateCalculator();

  calculator_->SnapshotOcclusionStateForWindows({container});

  EXPECT_EQ(calculator_->GetOcclusionState(win0.get()),
            aura::Window::OcclusionState::OCCLUDED);

  // Delete win1. This should trigger OnContentChanged and clear cache.
  win1.reset();

  // Snapshot again.
  calculator_->SnapshotOcclusionStateForWindows({container});

  // Now win0 should be visible.
  EXPECT_EQ(calculator_->GetOcclusionState(win0.get()),
            aura::Window::OcclusionState::VISIBLE);
}

TEST_F(WindowOcclusionCalculatorTest, DoesNotMutateGlobalWindowOcclusionState) {
  auto win0 = CreateWindowWithAppType(chromeos::AppType::NON_APP,
                                      gfx::Rect(10, 10, 100, 100));
  auto win1 = CreateWindowWithAppType(chromeos::AppType::NON_APP,
                                      gfx::Rect(10, 10, 100, 100));
  aura::Window* container = GetActiveDeskContainer();
  container->StackChildAtTop(win1.get());

  win0->TrackOcclusionState();
  win1->TrackOcclusionState();

  // Initially, global tracker computes states.
  EXPECT_EQ(win1->GetOcclusionState(), aura::Window::OcclusionState::VISIBLE);
  EXPECT_EQ(win0->GetOcclusionState(), aura::Window::OcclusionState::OCCLUDED);

  // Create calculator (locks them to current states).
  CreateCalculator();

  // Now pause the global tracker to simulate overview.
  aura::WindowOcclusionTracker::ScopedPause pause;

  // Move win1 to unocclude win0.
  win1->SetBounds(gfx::Rect(120, 10, 100, 100));

  // Since global tracker is paused, the window properties should NOT update.
  EXPECT_EQ(win0->GetOcclusionState(), aura::Window::OcclusionState::OCCLUDED);

  // Take a snapshot.
  calculator_->SnapshotOcclusionStateForWindows({container});

  // The snapshot should compute the TRUE state (win0 is VISIBLE).
  EXPECT_EQ(calculator_->GetOcclusionState(win0.get()),
            aura::Window::OcclusionState::VISIBLE);

  // But the GLOBAL state of the window should STILL be OCCLUDED (unchanged).
  EXPECT_EQ(win0->GetOcclusionState(), aura::Window::OcclusionState::OCCLUDED);
}

}  // namespace ash
