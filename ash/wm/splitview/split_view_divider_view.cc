// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/splitview/split_view_divider_view.h"

#include "ash/display/screen_orientation_controller.h"
#include "ash/public/cpp/window_properties.h"
#include "ash/shell.h"
#include "ash/utility/cursor_setter.h"
#include "ash/wm/snap_group/snap_group.h"
#include "ash/wm/splitview/split_view_constants.h"
#include "ash/wm/splitview/split_view_controller.h"
#include "ash/wm/splitview/split_view_divider.h"
#include "ash/wm/splitview/split_view_divider_handler_view.h"
#include "ash/wm/splitview/split_view_utils.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/display/screen.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/background.h"
#include "ui/views/highlight_border.h"
#include "ui/views/view.h"
#include "ui/wm/core/coordinate_conversion.h"

namespace ash {

SplitViewDividerView::SplitViewDividerView(SplitViewController* controller,
                                           SplitViewDivider* divider)
    : split_view_controller_(controller),
      divider_handler_view_(
          AddChildView(std::make_unique<SplitViewDividerHandlerView>())),
      divider_(divider) {
  SetEventTargeter(std::make_unique<views::ViewTargeter>(this));

  SetPaintToLayer(ui::LAYER_TEXTURED);
  layer()->SetFillsBoundsOpaquely(false);

  SetBackground(views::CreateThemedSolidBackground(
      cros_tokens::kCrosSysSystemBaseElevated));
  SetBorder(std::make_unique<views::HighlightBorder>(
      /*corner_radius=*/0,
      views::HighlightBorder::Type::kHighlightBorderNoShadow));
}

SplitViewDividerView::~SplitViewDividerView() = default;

void SplitViewDividerView::DoSpawningAnimation(int spawn_position) {
  const gfx::Rect bounds = GetBoundsInScreen();
  int divider_signed_offset;

  // To animate the divider scaling up from nothing, animate its bounds rather
  // than its transform, mostly because a transform that scales by zero would
  // be singular. For that bounds animation, express `spawn_position` in local
  // coordinates by subtracting a coordinate of the origin. Compute
  // `divider_signed_offset` as described in the comment for
  // `SplitViewDividerHandlerView::DoSpawningAnimation`.
  if (IsCurrentScreenOrientationLandscape()) {
    SetBounds(spawn_position - bounds.x(), 0, 0, bounds.height());
    divider_signed_offset = spawn_position - bounds.CenterPoint().x();
  } else {
    SetBounds(0, spawn_position - bounds.y(), bounds.width(), 0);
    divider_signed_offset = spawn_position - bounds.CenterPoint().y();
  }

  ui::LayerAnimator* divider_animator = layer()->GetAnimator();
  ui::ScopedLayerAnimationSettings settings(divider_animator);
  settings.SetTransitionDuration(kSplitviewDividerSpawnDuration);
  settings.SetTweenType(gfx::Tween::LINEAR_OUT_SLOW_IN);
  settings.SetPreemptionStrategy(ui::LayerAnimator::ENQUEUE_NEW_ANIMATION);
  divider_animator->SchedulePauseForProperties(
      kSplitviewDividerSpawnDelay, ui::LayerAnimationElement::BOUNDS);
  SetBounds(0, 0, bounds.width(), bounds.height());
  divider_handler_view_->DoSpawningAnimation(divider_signed_offset);
}

void SplitViewDividerView::SetDividerBarVisible(bool visible) {
  divider_handler_view_->SetVisible(visible);
}

void SplitViewDividerView::Layout() {
  // There is no divider in clamshell split view unless the feature flag
  // `kSnapGroup` is enabled. If we are in clamshell mode without the feature
  // flag and params, then we must be transitioning from tablet mode, and the
  // divider will be destroyed and there is no need to update it.
  if (!display::Screen::GetScreen()->InTabletMode() &&
      !IsSnapGroupEnabledInClamshellMode()) {
    return;
  }

  SetBoundsRect(GetLocalBounds());
  divider_handler_view_->Refresh(
      split_view_controller_->IsResizingWithDivider());
}

void SplitViewDividerView::OnMouseEntered(const ui::MouseEvent& event) {
  gfx::Point screen_location = event.location();
  ConvertPointToScreen(this, &screen_location);

  // Set cursor type as the resize cursor when it's on the split view divider.
  cursor_setter_.UpdateCursor(split_view_controller_->root_window(),
                              ui::mojom::CursorType::kColumnResize);
}

void SplitViewDividerView::OnMouseExited(const ui::MouseEvent& event) {
  // Since `notify_enter_exit_on_child_` in view.h is default to false, on mouse
  // exit `this` the cursor will be reset.
  cursor_setter_.ResetCursor();
}

bool SplitViewDividerView::OnMousePressed(const ui::MouseEvent& event) {
  gfx::Point location(event.location());
  views::View::ConvertPointToScreen(this, &location);
  divider_->StartResizeWithDivider(location);
  OnResizeStatusChanged();
  return true;
}

bool SplitViewDividerView::OnMouseDragged(const ui::MouseEvent& event) {
  gfx::Point location(event.location());
  views::View::ConvertPointToScreen(this, &location);
  divider_->ResizeWithDivider(location);
  return true;
}

void SplitViewDividerView::OnMouseReleased(const ui::MouseEvent& event) {
  gfx::Point location(event.location());
  views::View::ConvertPointToScreen(this, &location);
  divider_->EndResizeWithDivider(location);
  OnResizeStatusChanged();
  if (event.GetClickCount() == 2) {
    SwapWindows();
  }
}

void SplitViewDividerView::OnGestureEvent(ui::GestureEvent* event) {
  gfx::Point location(event->location());
  views::View::ConvertPointToScreen(this, &location);
  switch (event->type()) {
    case ui::ET_GESTURE_TAP:
      if (event->details().tap_count() == 2) {
        SwapWindows();
      }
      break;
    case ui::ET_GESTURE_TAP_DOWN:
    case ui::ET_GESTURE_SCROLL_BEGIN:
      divider_->StartResizeWithDivider(location);
      OnResizeStatusChanged();
      break;
    case ui::ET_GESTURE_SCROLL_UPDATE:
      divider_->ResizeWithDivider(location);
      break;
    case ui::ET_GESTURE_END:
      divider_->EndResizeWithDivider(location);
      OnResizeStatusChanged();
      break;
    default:
      break;
  }
  event->SetHandled();
}

bool SplitViewDividerView::DoesIntersectRect(const views::View* target,
                                             const gfx::Rect& rect) const {
  DCHECK_EQ(target, this);
  return true;
}

void SplitViewDividerView::SwapWindows() {
  if (IsSnapGroupEnabledInClamshellMode()) {
    // TODO(sophiewen): Consider adding a reference to `snap_group_` in
    // `SplitViewDivider` when multiple groups are added.
    // The divider would only be created between two windows.
    CHECK_EQ(2u, divider_->observed_windows().size());
    aura::Window* window = divider_->observed_windows().front();
    if (SnapGroup* snap_group =
            SnapGroupController::Get()->GetSnapGroupForGivenWindow(window)) {
      snap_group->SwapWindows();
    }
    return;
  }
  split_view_controller_->SwapWindows();
}

void SplitViewDividerView::OnResizeStatusChanged() {
  // It's possible that when this function is called, split view mode has
  // been ended, and the divider widget is to be deleted soon. In this case
  // no need to update the divider layout and do the animation. Also no need to
  // update if the divider is not resizing, which can happen if during
  // mid-gesture we do tablet <-> clamshell transition.
  // TODO(b/314018158): Remove `split_view_controller_` from
  // `SplitViewDividerView`.
  if (!split_view_controller_->InSplitViewMode() ||
      !split_view_controller_->IsResizingWithDivider()) {
    return;
  }

  // If `divider_view_`'s bounds are animating, it is for the divider spawning
  // animation. Stop that before animating `divider_view_`'s transform.
  ui::LayerAnimator* divider_animator = layer()->GetAnimator();
  divider_animator->StopAnimatingProperty(ui::LayerAnimationElement::BOUNDS);

  // Do the divider enlarge/shrink animation when starting/ending dragging.
  SetBoundsRect(GetLocalBounds());
  const gfx::Rect old_bounds =
      divider_->GetDividerBoundsInScreen(/*is_dragging=*/false);
  const gfx::Rect new_bounds = divider_->GetDividerBoundsInScreen(
      split_view_controller_->IsResizingWithDivider());
  gfx::Transform transform;
  transform.Translate(new_bounds.x() - old_bounds.x(),
                      new_bounds.y() - old_bounds.y());
  transform.Scale(
      static_cast<float>(new_bounds.width()) / old_bounds.width(),
      static_cast<float>(new_bounds.height()) / old_bounds.height());
  ui::ScopedLayerAnimationSettings settings(divider_animator);
  settings.SetTransitionDuration(
      kSplitviewDividerSelectionStatusChangeDuration);
  settings.SetTweenType(gfx::Tween::FAST_OUT_SLOW_IN);
  settings.SetPreemptionStrategy(
      ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET);
  SetTransform(transform);

  divider_handler_view_->Refresh(
      split_view_controller_->IsResizingWithDivider());
}

BEGIN_METADATA(SplitViewDividerView, views::View)
END_METADATA

}  // namespace ash
