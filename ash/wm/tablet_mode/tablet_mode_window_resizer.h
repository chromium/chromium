// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_TABLET_MODE_TABLET_MODE_WINDOW_RESIZER_H_
#define ASH_WM_TABLET_MODE_TABLET_MODE_WINDOW_RESIZER_H_

#include <memory>

#include "ash/ash_export.h"
#include "ash/wm/splitview/split_view_types.h"
#include "ash/wm/toplevel_window_event_handler.h"
#include "ash/wm/window_resizer.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/rect.h"

namespace aura {
class Window;
}

namespace ui {
class GestureEvent;
}

namespace ash {

class SplitViewController;
class SplitViewDragIndicators;
class TabletModeWindowDragDelegate;
class WindowState;

class ASH_EXPORT TabletModeWindowResizer : public WindowResizer {
 public:
  TabletModeWindowResizer(
      WindowState* window_state,
      std::unique_ptr<TabletModeWindowDragDelegate> drag_delegate);

  TabletModeWindowResizer(const TabletModeWindowResizer&) = delete;
  TabletModeWindowResizer& operator=(const TabletModeWindowResizer&) = delete;

  ~TabletModeWindowResizer() override;

  // WindowResizer:
  void Drag(const gfx::PointF& location_in_parent, int event_flags) override;
  void CompleteDrag() override;
  void RevertDrag() override;
  void FlingOrSwipe(ui::GestureEvent* event) override;

 private:
  std::unique_ptr<TabletModeWindowDragDelegate> drag_delegate_;
  gfx::PointF previous_location_in_screen_;

  // Converts the given parent-relative location to a screen coordinate and
  // stores it in previous_location_in_screen_, returning that new value.
  const gfx::PointF& ConvertAndSetPreviousLocationInScreen(
      const gfx::PointF& location_in_parent);
};

class ASH_EXPORT TabletModeWindowDragDelegate {
 public:
  TabletModeWindowDragDelegate();

  TabletModeWindowDragDelegate(const TabletModeWindowDragDelegate&) = delete;
  TabletModeWindowDragDelegate& operator=(const TabletModeWindowDragDelegate&) =
      delete;

  virtual ~TabletModeWindowDragDelegate();

  void StartWindowDrag(aura::Window* window,
                       const gfx::PointF& location_in_screen);

  // |target_bounds| is the desired bounds of the dragged window.
  void ContinueWindowDrag(const gfx::PointF& location_in_screen,
                          const gfx::Rect& target_bounds = gfx::Rect());

  void EndWindowDrag(ToplevelWindowEventHandler::DragResult result,
                     const gfx::PointF& location_in_screen);

 private:
  SnapPosition GetSnapPosition(const gfx::PointF& location_in_screen) const;

  raw_ref<SplitViewController> split_view_controller_;
  std::unique_ptr<SplitViewDragIndicators> split_view_drag_indicators_;
  raw_ptr<aura::Window> dragged_window_ = nullptr;
  gfx::PointF initial_location_in_screen_;
  base::WeakPtrFactory<TabletModeWindowDragDelegate> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // ASH_WM_TABLET_MODE_TABLET_MODE_WINDOW_RESIZER_H_
