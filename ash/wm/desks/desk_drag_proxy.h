// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_DESKS_DESK_DRAG_PROXY_H_
#define ASH_WM_DESKS_DESK_DRAG_PROXY_H_

#include "ui/compositor/layer_animation_observer.h"
#include "ui/views/widget/unique_widget_ptr.h"

namespace views {
class View;
}  // namespace views

namespace aura {
class Window;
}  // namespace aura

namespace ash {

class DeskMiniView;
class DeskPreviewView;
class DesksBarView;

// A helper class includes a widget whose content is the preview of the dragged
// desk.
// TODO(zxdan): Consider adding a DeskDragController to handle the communication
// between DeskPreviewView and DesksBarView after M89.
class DeskDragProxy : public ui::ImplicitAnimationObserver {
 public:
  DeskDragProxy(DesksBarView* desks_bar_view,
                DeskMiniView* drag_view,
                const gfx::Vector2dF& init_offset);
  DeskDragProxy(const DeskDragProxy&) = delete;
  DeskDragProxy& operator=(const DeskDragProxy&) = delete;
  ~DeskDragProxy() override;

  // ui::ImplicitAnimationObserver:
  void OnImplicitAnimationsCompleted() override;

  gfx::Point GetPositionInScreen() const;

  // Perform and animate scaling up of drag proxy. Move drag proxy to
  // |location_in_screen|.
  void ScaleAndMoveTo(const gfx::PointF& location_in_screen);

  // Move drag proxy to |location_in_screen|.
  void DragTo(const gfx::PointF& location_in_screen);

  // Perform and animate snapping back to drag view.
  void SnapBackToDragView();

 private:
  DesksBarView* desks_bar_view_ = nullptr;
  // The desk's mini view being dragged.
  DeskMiniView* drag_view_ = nullptr;
  // The size of dragged preview.
  const gfx::Size drag_preview_size_;
  // The initial offset between cursor and drag view's origin.
  gfx::Vector2dF init_offset_;
  // The widget of drag proxy.
  views::UniqueWidgetPtr drag_widget_;
};

}  // namespace ash

#endif  // ASH_WM_DESKS_DESK_DRAG_PROXY_H_
