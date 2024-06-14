// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_DISPLAY_MOUSE_CURSOR_EVENT_FILTER_H_
#define ASH_DISPLAY_MOUSE_CURSOR_EVENT_FILTER_H_

#include <memory>

#include "ash/ash_export.h"
#include "base/compiler_specific.h"
#include "base/gtest_prod_util.h"
#include "ui/display/manager/display_manager_observer.h"
#include "ui/events/event_handler.h"
#include "ui/gfx/geometry/rect.h"

namespace aura {
class Window;
}

namespace ash {

class MouseWarpController;

// An event filter that controls mouse location in extended desktop
// environment.
class ASH_EXPORT MouseCursorEventFilter
    : public ui::EventHandler,
      public display::DisplayManagerObserver {
 public:
  MouseCursorEventFilter();

  MouseCursorEventFilter(const MouseCursorEventFilter&) = delete;
  MouseCursorEventFilter& operator=(const MouseCursorEventFilter&) = delete;

  ~MouseCursorEventFilter() override;

  bool mouse_warp_enabled() const { return mouse_warp_enabled_; }
  void set_mouse_warp_enabled(bool enabled) { mouse_warp_enabled_ = enabled; }

  // Shows/Hide the indicator for window dragging. The |from|
  // is the window where the dragging started.
  void ShowSharedEdgeIndicator(aura::Window* from);
  void HideSharedEdgeIndicator();

  // display::DisplayManagerObserver:
  void OnDisplaysInitialized() override;
  void OnDidApplyDisplayChanges() override;

  // ui::EventHandler:
  void OnMouseEvent(ui::MouseEvent* event) override;

 private:
  friend class AshTestBase;
  friend class ExtendedMouseWarpControllerTest;
  friend class MouseCursorEventFilterTest;
  friend class UnifiedMouseWarpControllerTest;
  FRIEND_TEST_ALL_PREFIXES(MouseCursorEventFilterTest, DoNotWarpTwice);
  FRIEND_TEST_ALL_PREFIXES(MouseCursorEventFilterTest, SetMouseWarpModeFlag);
  FRIEND_TEST_ALL_PREFIXES(MouseCursorEventFilterTest,
                           WarpMouseDifferentScaleDisplaysInNative);
  FRIEND_TEST_ALL_PREFIXES(DragWindowResizerTest, WarpMousePointer);

  MouseWarpController* mouse_warp_controller_for_test() {
    return mouse_warp_controller_.get();
  }

  bool mouse_warp_enabled_;

  std::unique_ptr<MouseWarpController> mouse_warp_controller_;
};

}  // namespace ash

#endif  // ASH_DISPLAY_MOUSE_CURSOR_EVENT_FILTER_H_
