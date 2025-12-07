// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_DESKS_DESKS_TEST_UTIL_H_
#define ASH_WM_DESKS_DESKS_TEST_UTIL_H_

#include "ash/wm/desks/desks_controller.h"
#include "base/run_loop.h"

namespace ui::test {
class EventGenerator;
}  // namespace ui::test

namespace views {
class View;
}  // namespace views

namespace ash {

class CloseButton;
class DeskActivationAnimation;
class DeskMiniView;
class OverviewDeskBarView;

constexpr int kNumFingersForFocus = 3;
constexpr int kNumFingersForDesksSwitch = 4;

// Used for waiting for the desk switch animations on all root windows to
// complete.
class DeskSwitchAnimationWaiter : public DesksController::Observer {
 public:
  DeskSwitchAnimationWaiter();

  DeskSwitchAnimationWaiter(const DeskSwitchAnimationWaiter&) = delete;
  DeskSwitchAnimationWaiter& operator=(const DeskSwitchAnimationWaiter&) =
      delete;

  ~DeskSwitchAnimationWaiter() override;

  void Wait();

  // DesksController::Observer:
  void OnDeskSwitchAnimationFinished() override;

 private:
  base::RunLoop run_loop_;
};

// Activates the given |desk| and waits for the desk switch animation to
// complete before returning.
void ActivateDesk(const Desk* desk);

// Creates a desk through keyboard.
void NewDesk();

// Removes the given `desk` and waits for the desk-removal animation to finish
// if one would launch.
// If `close_windows` is set to true, the windows in `desk` are closed as well.
void RemoveDesk(const Desk* desk,
                DeskCloseType close_type = DeskCloseType::kCombineDesks);

// Returns the active desk.
const Desk* GetActiveDesk();

// Returns the next desk.
const Desk* GetNextDesk();

// Scrolls to the adjacent desk and waits for the animation if applicable.
void ScrollToSwitchDesks(bool scroll_left,
                         ui::test::EventGenerator* event_generator);

// Wait until `animation`'s ending screenshot has been taken.
void WaitUntilEndingScreenshotTaken(DeskActivationAnimation* animation);

// Returns the desk bar view for the primary display.
const OverviewDeskBarView* GetPrimaryRootDesksBarView();

// Returns the combine desks button if it is available, and otherwise the
// close-all button.
const CloseButton* GetCloseDeskButtonForMiniView(const DeskMiniView* mini_view);

// Returns the visibility state of the desk action interface for the mini view.
bool GetDeskActionVisibilityForMiniView(const DeskMiniView* mini_view);

// Wait for `milliseconds` to be finished. Use this as a last resort as it will
// flake or lengthen tests, especially if `milliseconds` is long (> 100ms). Do
// not hard code timeouts in the code without having some way to override from
// tests. See `DesksTestApi::SetCloseAllWindowCloseTimeout(). Or use a proper
// waiter like `DeskSwitchAnimationWaiter`.
void WaitForMilliseconds(int milliseconds);

// Long press at `screen_location` through a touch pressed event.
void LongGestureTap(const gfx::Point& screen_location,
                    ui::test::EventGenerator* event_generator,
                    bool release_touch = true);

// Performs two actions:
//   1) Simulate waiting for the close all toast to disappear by running the
//   expire callback directly.
//   2) Wait for the windows to maybe be forcefully closed.
void SimulateWaitForCloseAll();

// Whether a `view` is visible. `view` is lazy initialized, so can be null
// (which is equivalent to invisible).
bool IsLazyInitViewVisible(const views::View* view);

}  // namespace ash

#endif  // ASH_WM_DESKS_DESKS_TEST_UTIL_H_
