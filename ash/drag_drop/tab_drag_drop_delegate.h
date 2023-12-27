// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_DRAG_DROP_TAB_DRAG_DROP_DELEGATE_H_
#define ASH_DRAG_DROP_TAB_DRAG_DROP_DELEGATE_H_

#include <memory>

#include "ash/ash_export.h"
#include "ash/drag_drop/drag_drop_capture_delegate.h"
#include "ash/drag_drop/tab_drag_drop_windows_hider.h"
#include "ash/wm/splitview/split_view_controller.h"
#include "ash/wm/splitview/split_view_types.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "ui/aura/window_observer.h"

namespace aura {
class Window;
}

namespace ui {
class OSExchangeData;
class Point;
class PresentationTimeRecorder;
}  // namespace ui

namespace ash {

class SplitViewDragIndicators;
class TabDragDropWindowsHider;

// Provides special handling for Chrome tab drags on behalf of
// DragDropController. This must be created at the beginning of a tab drag and
// destroyed at the end.
class ASH_EXPORT TabDragDropDelegate : public DragDropCaptureDelegate,
                                       public aura::WindowObserver {
 public:
  // Determines whether |drag_data| indicates a tab drag from a WebUI tab strip
  // (or simply returns false if the integration is disabled).
  static bool IsChromeTabDrag(const ui::OSExchangeData& drag_data);

  // Returns whether a tab from |window| is actively being dragged.
  static bool IsSourceWindowForDrag(const aura::Window* window);

  // |root_window| is the root window from which the drag originated. The drag
  // is expected to stay in |root_window|. |source_window| is the Chrome window
  // the tab was dragged from. |start_location_in_screen| is the location of
  // the touch or click that started the drag.
  TabDragDropDelegate(aura::Window* root_window,
                      aura::Window* source_window,
                      const gfx::Point& start_location_in_screen);
  ~TabDragDropDelegate() override;

  TabDragDropDelegate(const TabDragDropDelegate&) = delete;
  TabDragDropDelegate& operator=(const TabDragDropDelegate&) = delete;

  const aura::Window* root_window() const { return root_window_; }

  // Must be called on every drag update.
  void DragUpdate(const gfx::Point& location_in_screen);

  // Must be called on drop if it was not accepted by the drop target. After
  // calling this, this delegate must not be used.
  void DropAndDeleteSelf(const gfx::Point& location_in_screen,
                         const ui::OSExchangeData& drop_data);

  // Overridden from aura::WindowObserver.
  void OnWindowDestroying(aura::Window* window) override;

 private:
  FRIEND_TEST_ALL_PREFIXES(TabDragDropDelegateTest, DropWithoutNewWindow);

  // Scales or transforms the source window if appropriate for this drag.
  // |candidate_snap_position| is where the dragged tab will be snapped
  // if dropped immediately.
  void UpdateSourceWindowBoundsIfNecessary(
      SnapPosition candidate_snap_position,
      const gfx::Point& location_in_screen);

  // Puts the source window back into its original position.
  void RestoreSourceWindowBounds();

  // Effectively handles the new window creation in DropAndDeleteSelf(). This
  // method can be called asynchronously in case of Lacros.
  void OnNewBrowserWindowCreated(const gfx::Point& location_in_screen,
                                 aura::Window* new_window);

  // This method returns true when all of the conditions below are met
  //
  // - Not in split view mode
  // - In landscape mode
  // - The current drag location is inside the WebUI tab strip
  //
  // When it returns true, we don't allow to enter split view because
  // it hinders dragging tabs within the tab strip to trigger auto-scroll.
  // This restriction does not apply to split screen mode because either
  // the left/right could be non browser window, which may lead to
  // confusing behavior.
  // It also does not apply to portrait mode because dragging up/down to
  // enter split screen does not hinder dragging left/right to move tabs.
  //
  // https://crbug.com/1316070
  bool ShouldPreventSnapToTheEdge(const gfx::Point& location_in_screen);

  const raw_ptr<aura::Window> root_window_;
  raw_ptr<aura::Window> source_window_;
  const gfx::Point start_location_in_screen_;

  std::unique_ptr<SplitViewDragIndicators> split_view_drag_indicators_;
  std::unique_ptr<TabDragDropWindowsHider> windows_hider_;

  // Presentation time recorder for tab dragging in tablet mode with webui
  // tab strip enable.
  std::unique_ptr<ui::PresentationTimeRecorder> tab_dragging_recorder_;
};

}  // namespace ash

#endif  // ASH_DRAG_DROP_TAB_DRAG_DROP_DELEGATE_H_
