// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/desks/desk_drag_proxy.h"

#include "ash/drag_drop/drag_image_view.h"
#include "ash/style/system_shadow.h"
#include "ash/wm/desks/desk_bar_view_base.h"
#include "ash/wm/desks/desk_mini_view.h"
#include "ash/wm/desks/desk_preview_view.h"
#include "chromeos/constants/chromeos_features.h"
#include "ui/base/dragdrop/mojom/drag_drop_types.mojom-shared.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/display/screen.h"
#include "ui/events/event_observer.h"
#include "ui/gfx/geometry/transform_util.h"
#include "ui/views/widget/widget.h"

namespace ash {

namespace {

// Scale of dragged desk proxy.
constexpr float kDragProxyScale = 1.2f;
// Time duration of scaling up dragged desk proxy.
constexpr base::TimeDelta kDragProxyScaleUpDuration = base::Milliseconds(200);
// Time duration of snapping back drag proxy.
constexpr base::TimeDelta kDragProxySnapBackDuration = base::Milliseconds(300);

}  // namespace

DeskDragProxy::DeskDragProxy(
    DeskBarViewBase* desk_bar_view,
    DeskMiniView* drag_view,
    float init_offset_x,
    base::WeakPtr<WindowOcclusionCalculator> window_occlusion_calculator)
    : desk_bar_view_(desk_bar_view),
      drag_view_(drag_view),
      drag_preview_size_(drag_view->GetPreviewBoundsInScreen().size()),
      preview_screen_y_(drag_view->GetPreviewBoundsInScreen().y()),
      init_offset_x_(init_offset_x),
      window_occlusion_calculator_(window_occlusion_calculator) {}

DeskDragProxy::~DeskDragProxy() = default;

void DeskDragProxy::OnImplicitAnimationsCompleted() {
  DCHECK(desk_bar_view_);

  state_ = State::kEnded;

  // `this` is destroyed here.
  desk_bar_view_->FinalizeDragDesk();
}

gfx::Rect DeskDragProxy::GetBoundsInScreen() const {
  return gfx::Rect(
      drag_widget_->GetWindowBoundsInScreen().origin(),
      gfx::ScaleToFlooredSize(drag_preview_size_, kDragProxyScale));
}

void DeskDragProxy::InitAndScaleAndMoveToX(float location_screen_x) {
  DCHECK(drag_view_);

  aura::Window* root_window =
      drag_view_->GetWidget()->GetNativeWindow()->GetRootWindow();
  // Create a drag widget.
  drag_widget_ =
      DragImageView::Create(root_window, ui::mojom::DragEventSource::kMouse);
  // Turn off the fade animation.
  drag_widget_->SetVisibilityAnimationTransition(views::Widget::ANIMATE_NONE);

  // Copy the preview of the dragged desk to the widget content.
  drag_widget_->SetContentsView(std::make_unique<DeskPreviewView>(
      views::Button::PressedCallback(), drag_view_,
      window_occlusion_calculator_));

  // Set the bounds of dragged preview to drag proxy.
  drag_widget_->SetBounds(drag_view_->GetPreviewBoundsInScreen());

  drag_widget_->Show();

  ui::Layer* layer = drag_widget_->GetLayer();
  // Perform and animate scaling.
  gfx::Transform scale_transform;
  scale_transform.Scale(kDragProxyScale, kDragProxyScale);
  ui::ScopedLayerAnimationSettings settings(layer->GetAnimator());
  settings.SetTransitionDuration(kDragProxyScaleUpDuration);
  // Scale the bounds around its center.
  gfx::Rect proxy_bounds_in_screen = drag_widget_->GetWindowBoundsInScreen();
  layer->SetTransform(gfx::TransformAboutPivot(
      gfx::PointF(0.5f * proxy_bounds_in_screen.width(),
                  0.5f * proxy_bounds_in_screen.height()),
      scale_transform));

  // Perform Moving.
  DragToX(location_screen_x);

  state_ = State::kStarted;
}

void DeskDragProxy::DragToX(float location_screen_x) {
  drag_widget_->SetBounds(
      gfx::Rect(gfx::Point(base::ClampRound(location_screen_x - init_offset_x_),
                           preview_screen_y_),
                drag_preview_size_));
}

void DeskDragProxy::SnapBackToDragView() {
  DCHECK_NE(state_, State::kSnappingBack);

  // Cache proxy's bounds and drag view's bounds.
  gfx::RectF scaled_proxy_bounds(drag_widget_->GetWindowBoundsInScreen());
  scaled_proxy_bounds.set_size(
      gfx::ScaleSize(scaled_proxy_bounds.size(), kDragProxyScale));
  const gfx::Rect drag_view_bounds = drag_view_->GetPreviewBoundsInScreen();
  // Set bounds of drag view to drag proxy.
  drag_widget_->SetBounds(drag_view_bounds);

  ui::Layer* layer = drag_widget_->GetLayer();
  // Animate snapping back.
  layer->SetTransform(gfx::TransformBetweenRects(gfx::RectF(drag_view_bounds),
                                                 scaled_proxy_bounds));
  ui::ScopedLayerAnimationSettings settings(layer->GetAnimator());
  settings.SetTransitionDuration(kDragProxySnapBackDuration);
  settings.SetTweenType(gfx::Tween::ACCEL_LIN_DECEL_60);
  settings.AddObserver(this);
  layer->SetTransform(gfx::Transform());

  state_ = State::kSnappingBack;
}

}  // namespace ash
