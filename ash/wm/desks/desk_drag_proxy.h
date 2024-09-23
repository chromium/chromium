// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_DESKS_DESK_DRAG_PROXY_H_
#define ASH_WM_DESKS_DESK_DRAG_PROXY_H_

#include "base/memory/raw_ptr.h"
#include "ui/compositor/layer_animation_observer.h"
#include "ui/views/widget/unique_widget_ptr.h"

namespace ash {

class DeskMiniView;
class DeskPreviewView;
class DeskBarViewBase;
class WindowOcclusionCalculator;

// A helper class includes a widget whose content is the preview of the dragged
// desk.
// TODO(zxdan): Consider adding a `DeskDragController` to handle the
// communication between `DeskPreviewView` and `DeskBarViewBase` after M89.
class DeskDragProxy : public ui::ImplicitAnimationObserver {
 public:
  enum class State {
    kInitialized,   // A desk preview is clicked, but not dragged.
    kStarted,       // Drag started.
    kSnappingBack,  // A desk is dropped and snapping back to the target
                    // position.
    kEnded,         // The drag and drop finished.
  };

  DeskDragProxy(
      DeskBarViewBase* desk_bar_view,
      DeskMiniView* drag_view,
      float init_offset_x,
      base::WeakPtr<WindowOcclusionCalculator> window_occlusion_calculator);
  DeskDragProxy(const DeskDragProxy&) = delete;
  DeskDragProxy& operator=(const DeskDragProxy&) = delete;
  ~DeskDragProxy() override;

  // ui::ImplicitAnimationObserver:
  void OnImplicitAnimationsCompleted() override;

  gfx::Rect GetBoundsInScreen() const;

  // Drag is started. Create a drag proxy widget, scale it up and move its
  // x-coordinate according to |location_screen_x|.
  void InitAndScaleAndMoveToX(float location_screen_x);

  // Move drag proxy's x-coordinate according to |location_screen_x|.
  void DragToX(float location_screen_x);

  // Perform and animate snapping back to drag view.
  void SnapBackToDragView();

  State state() const { return state_; }

 private:
  raw_ptr<DeskBarViewBase> desk_bar_view_ = nullptr;
  // The desk's mini view being dragged.
  raw_ptr<DeskMiniView> drag_view_ = nullptr;
  // The size of dragged preview.
  const gfx::Size drag_preview_size_;
  // The y of the dragged preview in screen coordinate.
  const int preview_screen_y_;
  // The x of initial offset between cursor and drag view's origin.
  const float init_offset_x_;
  const base::WeakPtr<WindowOcclusionCalculator> window_occlusion_calculator_;
  // The widget of drag proxy.
  views::UniqueWidgetPtr drag_widget_;
  // The state of the drag proxy.
  State state_ = State::kInitialized;
};

}  // namespace ash

#endif  // ASH_WM_DESKS_DESK_DRAG_PROXY_H_
