// Copyright (c) 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SHELF_TEST_SHELF_LAYOUT_MANAGER_TEST_BASE_H_
#define ASH_SHELF_TEST_SHELF_LAYOUT_MANAGER_TEST_BASE_H_

#include <utility>

#include "ash/public/cpp/shelf_types.h"
#include "ash/test/ash_test_base.h"
#include "ash/wm/workspace/workspace_types.h"
#include "base/time/time.h"

namespace ui {
class Layer;
}

namespace ash {

class ShelfLayoutManager;

class ShelfLayoutManagerTestBase : public AshTestBase {
 public:
  template <typename... TaskEnvironmentTraits>
  explicit ShelfLayoutManagerTestBase(TaskEnvironmentTraits&&... traits)
      : AshTestBase(std::forward<TaskEnvironmentTraits>(traits)...) {}

  // Calls the private SetState() function.
  void SetState(ShelfLayoutManager* layout_manager, ShelfVisibilityState state);

  void UpdateAutoHideStateNow();

  aura::Window* CreateTestWindow();
  aura::Window* CreateTestWindowInParent(aura::Window* root_window);

  // Create a simple widget in the current context (will delete on TearDown).
  views::Widget* CreateTestWidget();

  void RunGestureDragTests(const gfx::Point& shown, const gfx::Point& hidden);

  gfx::Rect GetVisibleShelfWidgetBoundsInScreen();

  // Turn on the lock screen.
  void LockScreen();

  // Turn off the lock screen.
  void UnlockScreen();

  int64_t GetPrimaryDisplayId();
  void StartScroll(gfx::Point start);
  void UpdateScroll(float delta_y);
  void EndScroll(bool is_fling, float velocity_y);
  void IncreaseTimestamp();
  WorkspaceWindowState GetWorkspaceWindowState() const;
  const ui::Layer* GetNonLockScreenContainersContainerLayer() const;

  // If |layout_manager->auto_hide_timer_| is running, stops it, runs its task,
  // and returns true. Otherwise, returns false.
  bool TriggerAutoHideTimeout() const;

  // Performs a swipe up gesture to show an auto-hidden shelf.
  void SwipeUpOnShelf();
  void SwipeDownOnShelf();
  void FlingUpOnShelf();
  void DragHotseatDownToBezel();

  // Drag Shelf from |start| to |target| by mouse.
  void MouseDragShelfTo(const gfx::Point& start, const gfx::Point& target);

  // Move mouse to show Shelf in auto-hide mode.
  void MouseMouseToShowAutoHiddenShelf();

  // Move mouse to |location| and do a two-finger vertical scroll.
  void DoTwoFingerVerticalScrollAtLocation(gfx::Point location,
                                           int y_offset,
                                           bool reverse_scroll);

  // Move mouse to |location| and do a mousewheel scroll.
  void DoMouseWheelScrollAtLocation(gfx::Point location,
                                    int delta_y,
                                    bool reverse_scroll);

  // Run the |visibility_update_for_tray_callback_| if set in
  // ShelfLayoutManager and return true. Otherwise, return false.
  bool RunVisibilityUpdateForTrayCallback();

 private:
  base::TimeTicks timestamp_;
  gfx::Point current_point_;
};

}  //  namespace ash

#endif  // ASH_SHELF_TEST_SHELF_LAYOUT_MANAGER_TEST_BASE_H_
