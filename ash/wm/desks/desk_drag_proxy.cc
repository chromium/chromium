// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/desks/desk_drag_proxy.h"

#include "ash/drag_drop/drag_image_view.h"
#include "ash/wm/desks/desk_mini_view.h"
#include "ash/wm/desks/desk_preview_view.h"
#include "ash/wm/desks/desks_bar_view.h"
#include "ui/base/dragdrop/mojom/drag_drop_types.mojom-shared.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/display/screen.h"
#include "ui/events/event_observer.h"
#include "ui/gfx/transform_util.h"
#include "ui/views/widget/widget.h"

namespace ash {

namespace {

// Scale of dragged desk proxy.
constexpr float kDragProxyScale = 1.2f;
// Time duration of scaling up dragged desk proxy.
constexpr base::TimeDelta kDragProxyScaleUpDuration =
    base::TimeDelta::FromMilliseconds(200);
// Time duration of snapping back drag proxy.
constexpr base::TimeDelta kDragProxySnapBackDuration =
    base::TimeDelta::FromMilliseconds(300);

}  // namespace

DeskDragProxy::DeskDragProxy(DesksBarView* desks_bar_view,
                             DeskMiniView* drag_view,
                             const gfx::Vector2dF& init_offset)
    : desks_bar_view_(desks_bar_view),
      drag_view_(drag_view),
      drag_preview_size_(drag_view->GetPreviewBoundsInScreen().size()),
      init_offset_(init_offset) {
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
      views::Button::PressedCallback(), drag_view_));

  // Set the bounds of dragged preview to drag proxy.
  drag_widget_->SetBounds(drag_view_->GetPreviewBoundsInScreen());

  drag_widget_->Show();
}

DeskDragProxy::~DeskDragProxy() = default;

void DeskDragProxy::OnImplicitAnimationsCompleted() {
  DCHECK(desks_bar_view_);

  desks_bar_view_->FinalizeDragDesk();
}

gfx::Point DeskDragProxy::GetPositionInScreen() const {
  return drag_widget_->GetWindowBoundsInScreen().origin();
}

void DeskDragProxy::ScaleAndMoveTo(const gfx::PointF& location_in_screen) {
  ui::Layer* layer = drag_widget_->GetLayer();

  // Perform and animate scaling.
  gfx::Transform scale_transform;
  scale_transform.Scale(kDragProxyScale, kDragProxyScale);
  ui::ScopedLayerAnimationSettings settings(layer->GetAnimator());
  settings.SetTransitionDuration(kDragProxyScaleUpDuration);
  // Scale the bounds around its center.
  gfx::Rect proxy_bounds_in_screen = drag_widget_->GetWindowBoundsInScreen();
  layer->SetTransform(gfx::TransformAboutPivot(
      proxy_bounds_in_screen.CenterPoint() -
          proxy_bounds_in_screen.origin().OffsetFromOrigin(),
      scale_transform));

  // Perform Moving.
  DragTo(location_in_screen);
}

void DeskDragProxy::DragTo(const gfx::PointF& location_in_screen) {
  drag_widget_->SetBounds(
      gfx::Rect(gfx::ToRoundedPoint(location_in_screen - init_offset_),
                drag_preview_size_));
}

void DeskDragProxy::SnapBackToDragView() {
  ui::Layer* layer = drag_widget_->GetLayer();

  // Do not snap back again if the proxy is already doing it.
  if (layer->GetAnimator()->is_animating() &&
      layer->GetTargetTransform().IsIdentity())
    return;

  // Cache proxy's bounds and drag view's bounds.
  gfx::RectF scaled_proxy_bounds(drag_widget_->GetWindowBoundsInScreen());
  scaled_proxy_bounds.set_size(
      gfx::ScaleSize(scaled_proxy_bounds.size(), kDragProxyScale));
  const gfx::Rect drag_view_bounds = drag_view_->GetPreviewBoundsInScreen();
  // Set bounds of drag view to drag proxy.
  drag_widget_->SetBounds(drag_view_bounds);

  // Animate snapping back.
  layer->SetTransform(gfx::TransformBetweenRects(gfx::RectF(drag_view_bounds),
                                                 scaled_proxy_bounds));
  ui::ScopedLayerAnimationSettings settings(layer->GetAnimator());
  settings.SetTransitionDuration(kDragProxySnapBackDuration);
  settings.AddObserver(this);
  layer->SetTransform(gfx::Transform());
}

}  // namespace ash
