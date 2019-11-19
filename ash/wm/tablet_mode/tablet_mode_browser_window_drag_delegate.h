// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_TABLET_MODE_TABLET_MODE_BROWSER_WINDOW_DRAG_DELEGATE_H_
#define ASH_WM_TABLET_MODE_TABLET_MODE_BROWSER_WINDOW_DRAG_DELEGATE_H_

#include "ash/ash_export.h"
#include "ash/wm/tablet_mode/tablet_mode_window_drag_delegate.h"
#include "base/macros.h"

namespace ash {

// The drag delegate for browser windows. It not only includes the logic in
// TabletModeWindowDragDelegate, but also has special logic for browser windows,
// e.g., scales the source window, shows/hides the other windows below the
// source window.
class ASH_EXPORT TabletModeBrowserWindowDragDelegate
    : public TabletModeWindowDragDelegate {
 public:
  TabletModeBrowserWindowDragDelegate();
  ~TabletModeBrowserWindowDragDelegate() override;

 private:
  class WindowsHider;

  // TabletModeWindowDragDelegate:
  void PrepareWindowDrag(const gfx::Point& location_in_screen) override;
  void UpdateWindowDrag(const gfx::Point& location_in_screen) override;
  void EndingWindowDrag(ToplevelWindowEventHandler::DragResult result,
                        const gfx::Point& location_in_screen) override;
  void EndedWindowDrag(const gfx::Point& location_in_screen) override;
  void StartFling(const ui::GestureEvent* event) override;
  bool ShouldOpenOverviewWhenDragStarts() override;

  // Scales down the source window if the dragged window is dragged past the
  // |kIndicatorThresholdRatio| threshold and restores it if the dragged window
  // is dragged back toward the top of the screen. |location_in_screen| is the
  // current drag location for the dragged window.
  void UpdateSourceWindow(const gfx::Point& location_in_screen);

  // After drag ends, the dragged window might need to merge back into the
  // source window if 1) the dragged window or the source window is not added to
  // overview and 2) the dragged window has dragged farther than half of the
  // screen height and 3) the dragged window is not in snap preview area and 4)
  // the dragged window is not dragged to the other side of the split screen.
  void MergeBackToSourceWindowIfApplicable(
      const gfx::Point& location_in_screen);

  // It's used to hide all visible windows if the source window needs to be
  // scaled up/down during dragging a tab out of the source window. It also
  // hides the home launcher if home launcher is enabled, blurs and darkens the
  // background upon its creation. All of these will be restored upon its
  // destruction.
  std::unique_ptr<WindowsHider> windows_hider_;

  // The observer to observe the source window's bounds change animation during
  // dragging. It's used to prevent the dragged window to merge back into the
  // source window during dragging. Only when the source window restores to its
  // maximized window size, the dragged window can be merged back into the
  // source window.
  std::unique_ptr<ui::ImplicitAnimationObserver> source_window_bounds_observer_;

  // True if the dragged window is capable of merging back to source window
  // after drag ends. If it's false, it means the drag ends because of a fling
  // event and the fling velocity has exceeded kFlingToStayAsNewWindowThreshold.
  bool can_merge_back_to_source_window_ = true;

  DISALLOW_COPY_AND_ASSIGN(TabletModeBrowserWindowDragDelegate);
};

}  // namespace ash

#endif  // ASH_WM_TABLET_MODE_TABLET_MODE_BROWSER_WINDOW_DRAG_DELEGATE_H_
