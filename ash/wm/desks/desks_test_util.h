// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_DESKS_DESKS_TEST_UTIL_H_
#define ASH_WM_DESKS_DESKS_TEST_UTIL_H_

#include "ash/wm/desks/desks_controller.h"
#include "base/run_loop.h"

namespace ui {
namespace test {
class EventGenerator;
}  // namespace test
}  // namespace ui

namespace ash {

class DeskActivationAnimation;
class DesksBarView;

constexpr int kNumFingersForHighlight = 3;
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
  void OnDeskAdded(const Desk* desk) override;
  void OnDeskRemoved(const Desk* desk) override;
  void OnDeskReordered(int old_index, int new_index) override;
  void OnDeskActivationChanged(const Desk* activated,
                               const Desk* deactivated) override;
  void OnDeskSwitchAnimationLaunching() override;
  void OnDeskSwitchAnimationFinished() override;
  void OnDeskNameChanged(const Desk* desk,
                         const std::u16string& new_name) override;

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
const DesksBarView* GetPrimaryRootDesksBarView();

}  // namespace ash

#endif  // ASH_WM_DESKS_DESKS_TEST_UTIL_H_
