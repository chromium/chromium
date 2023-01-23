// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/multi_display/multi_display_metrics_controller.h"

#include "ash/accessibility/magnifier/docked_magnifier_controller.h"
#include "ash/display/screen_orientation_controller_test_api.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "base/test/metrics/histogram_tester.h"
#include "ui/display/test/display_manager_test_api.h"
#include "ui/wm/core/window_util.h"

namespace ash {

class MultiDisplayMetricsControllerTest : public AshTestBase {
 public:
  MultiDisplayMetricsControllerTest() = default;
  MultiDisplayMetricsControllerTest(const MultiDisplayMetricsControllerTest&) =
      delete;
  MultiDisplayMetricsControllerTest& operator=(
      const MultiDisplayMetricsControllerTest&) = delete;
  ~MultiDisplayMetricsControllerTest() override = default;

  void MaybeFireTimerNow() {
    base::OneShotTimer* timer =
        &Shell::Get()->multi_display_metrics_controller()->timer_;
    if (timer->IsRunning()) {
      timer->FireNow();
    }
  }

  // Helpers to move and resize a given window using the event generator.
  // Asserts that the window bounds have changed.
  void MoveWindow(aura::Window* window) {
    wm::ActivateWindow(window);

    const gfx::Rect old_bounds = window->GetBoundsInScreen();
    const gfx::Point point_on_header_in_screen(old_bounds.CenterPoint().x(),
                                               old_bounds.y() + 10);
    GetEventGenerator()->set_current_screen_location(point_on_header_in_screen);
    GetEventGenerator()->DragMouseBy(10, 10);
    const gfx::Rect expected_new_bounds = old_bounds + gfx::Vector2d(10, 10);
    ASSERT_EQ(expected_new_bounds, window->GetBoundsInScreen());
  }

  void ResizeWindow(aura::Window* window) {
    wm::ActivateWindow(window);

    const gfx::Rect old_bounds = window->GetBoundsInScreen();
    GetEventGenerator()->set_current_screen_location(old_bounds.bottom_right());
    GetEventGenerator()->DragMouseBy(5, 5);
    gfx::Rect expected_new_bounds(old_bounds);
    expected_new_bounds.Outset(gfx::Outsets::TLBR(0, 0, 5, 5));
    ASSERT_EQ(expected_new_bounds, window->GetBoundsInScreen());
  }

  // AshTestBase:
  void SetUp() override {
    AshTestBase::SetUp();
    for (int i = 0; i < 4; ++i) {
      test_windows_.push_back(CreateAppWindow());
    }
  }

  void TearDown() override {
    test_windows_.clear();
    AshTestBase::TearDown();
  }

 protected:
  // Most tests in this suite use multiple windows per test. This vector stores
  // the needed app windows, which are resizable by default and would be in the
  // MRU list.
  std::vector<std::unique_ptr<aura::Window>> test_windows_;

  base::HistogramTester histogram_tester_;
};

TEST_F(MultiDisplayMetricsControllerTest, DisplayRotated) {
  // Rotate the display. Do a double rotation so our move and resize helpers
  // work properly.
  const int64_t display_id =
      display::Screen::GetScreen()->GetPrimaryDisplay().id();
  display::DisplayManager* display_manager = Shell::Get()->display_manager();
  display::test::ScopedSetInternalDisplayId set_internal(display_manager,
                                                         display_id);
  ScreenOrientationControllerTestApi orientation_test_api(
      Shell::Get()->screen_orientation_controller());
  orientation_test_api.SetDisplayRotation(
      display::Display::ROTATE_90, display::Display::RotationSource::ACTIVE);
  orientation_test_api.SetDisplayRotation(
      display::Display::ROTATE_0, display::Display::RotationSource::ACTIVE);

  MoveWindow(test_windows_[0].get());
  MaybeFireTimerNow();
  histogram_tester_.ExpectUniqueSample(
      MultiDisplayMetricsController::kRotatedHistogram, static_cast<int>(true),
      1);
}

TEST_F(MultiDisplayMetricsControllerTest, DisplaySizeChanged) {
  UpdateDisplay("800x600");

  UpdateDisplay("2000x1000");
  MoveWindow(test_windows_[0].get());
  MaybeFireTimerNow();
  histogram_tester_.ExpectUniqueSample(
      MultiDisplayMetricsController::kWorkAreaChangedHistogram,
      static_cast<int>(true), 1);
}

TEST_F(MultiDisplayMetricsControllerTest, DisplayWorkAreaChanged) {
  // We will use the docked magnifier to modify the work area in this test.
  DockedMagnifierController* docked_magnifier_controller =
      Shell::Get()->docked_magnifier_controller();
  docked_magnifier_controller->SetEnabled(true);
  ASSERT_GT(docked_magnifier_controller->GetMagnifierHeightForTesting(), 0);

  MoveWindow(test_windows_[0].get());

  MaybeFireTimerNow();
  histogram_tester_.ExpectUniqueSample(
      MultiDisplayMetricsController::kWorkAreaChangedHistogram,
      static_cast<int>(true), 1);
}

// The following tests use display size change to test several edge cases. The
// logic between all the display changes is the same and display size changed is
// the simplest scenario to test.

// Tests that if no windows are moved or resized after a display change, the
// histogram reflects that.
TEST_F(MultiDisplayMetricsControllerTest, NoWindowsMovedOrResized) {
  UpdateDisplay("800x600");

  UpdateDisplay("2000x1000");
  MaybeFireTimerNow();
  histogram_tester_.ExpectUniqueSample(
      MultiDisplayMetricsController::kWorkAreaChangedHistogram,
      static_cast<int>(false), 1);
}

TEST_F(MultiDisplayMetricsControllerTest, MultipleWindowsMoved) {
  UpdateDisplay("800x600");

  UpdateDisplay("2000x1000");

  // Move a couple of different windows after resizing a display. Test that we
  // only record it once.
  MoveWindow(test_windows_[0].get());
  MoveWindow(test_windows_[1].get());
  MoveWindow(test_windows_[2].get());
  MoveWindow(test_windows_[3].get());

  MaybeFireTimerNow();
  histogram_tester_.ExpectUniqueSample(
      MultiDisplayMetricsController::kWorkAreaChangedHistogram,
      static_cast<int>(true), 1);
}

// Tests that resizing window after a display changes records properly.
TEST_F(MultiDisplayMetricsControllerTest, WindowsResized) {
  UpdateDisplay("800x600");

  UpdateDisplay("2000x1000");
  ResizeWindow(test_windows_[0].get());

  MaybeFireTimerNow();
  histogram_tester_.ExpectUniqueSample(
      MultiDisplayMetricsController::kWorkAreaChangedHistogram,
      static_cast<int>(true), 1);
}

TEST_F(MultiDisplayMetricsControllerTest, MultipleDisplayChanges) {
  UpdateDisplay("800x600");

  UpdateDisplay("1200x800");
  UpdateDisplay("2000x1000");
  UpdateDisplay("3000x2000");

  // Move the window after changing the display size multiple times. Test that
  // we only record it once.
  MoveWindow(test_windows_[0].get());

  MaybeFireTimerNow();
  histogram_tester_.ExpectUniqueSample(
      MultiDisplayMetricsController::kWorkAreaChangedHistogram,
      static_cast<int>(true), 1);
}

// Tests that windows that are added after a display change and then moved
// and/or resized do not count.
TEST_F(MultiDisplayMetricsControllerTest, WindowAddedAfterDisplayChangeMoved) {
  UpdateDisplay("800x600");

  UpdateDisplay("1200x800");

  // Add a new window after the display has changed and move the window.
  auto new_window = CreateAppWindow();
  MoveWindow(new_window.get());
  ResizeWindow(new_window.get());

  // Verify we don't record the window change, since it was added after the
  // display change.
  MaybeFireTimerNow();
  histogram_tester_.ExpectUniqueSample(
      MultiDisplayMetricsController::kWorkAreaChangedHistogram,
      static_cast<int>(false), 1);
}

}  // namespace ash
