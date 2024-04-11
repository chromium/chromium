// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/splitview/split_view_divider_view.h"

#include "ash/display/screen_orientation_controller.h"
#include "ash/public/cpp/window_properties.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/shell.h"
#include "ash/shell_delegate.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/icon_button.h"
#include "ash/wm/snap_group/snap_group.h"
#include "ash/wm/splitview/split_view_constants.h"
#include "ash/wm/splitview/split_view_divider.h"
#include "ash/wm/splitview/split_view_divider_handler_view.h"
#include "ash/wm/splitview/split_view_utils.h"
#include "base/functional/callback_helpers.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/display/screen.h"
#include "ui/events/types/event_type.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/background.h"
#include "ui/views/highlight_border.h"
#include "ui/views/view.h"
#include "ui/wm/core/coordinate_conversion.h"

namespace ash {

namespace {

// Distance between the bottom / right edge of the feedback button and the
// bottom / right of the work area.
constexpr int kFeedbackButtonDistanceFromEdge = 58;

// Feedback button size.
constexpr gfx::Size kFeedbackButtonSize{40, 40};

// Feedback button icon size.
constexpr int kFeedbackButtonIconSize = 20;

}  // namespace

SplitViewDividerView::SplitViewDividerView(SplitViewDivider* divider)
    : divider_handler_view_(
          AddChildView(std::make_unique<SplitViewDividerHandlerView>())),
      divider_(divider) {
  SetEventTargeter(std::make_unique<views::ViewTargeter>(this));

  SetPaintToLayer(ui::LAYER_TEXTURED);
  layer()->SetFillsBoundsOpaquely(false);

  SetBackground(
      views::CreateThemedSolidBackground(cros_tokens::kCrosSysSecondary));
  RefreshFeedbackButton(false);
}

SplitViewDividerView::~SplitViewDividerView() = default;

void SplitViewDividerView::OnDividerClosing() {
  divider_ = nullptr;
}

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

void SplitViewDividerView::Layout(PassKey) {
  // There is no divider in clamshell split view unless the feature flag
  // `kSnapGroup` is enabled. If we are in clamshell mode without the feature
  // flag and params, then we must be transitioning from tablet mode, and the
  // divider will be destroyed and there is no need to update it.
  if (!divider_ || (!display::Screen::GetScreen()->InTabletMode() &&
                    !IsSnapGroupEnabledInClamshellMode())) {
    return;
  }

  SetBoundsRect(GetLocalBounds());
  RefreshFeedbackButtonBounds();
  divider_handler_view_->Refresh(divider_->is_resizing_with_divider());
}

void SplitViewDividerView::OnMouseEntered(const ui::MouseEvent& event) {
  gfx::Point screen_location = event.location();
  ConvertPointToScreen(this, &screen_location);

  if (!feedback_button_ ||
      !feedback_button_->GetBoundsInScreen().Contains(screen_location)) {
    // Show `feedback_button_` on mouse entered.
    RefreshFeedbackButton(/*visible=*/true);
  }
}

void SplitViewDividerView::OnMouseExited(const ui::MouseEvent& event) {
  gfx::Point screen_location = event.location();
  ConvertPointToScreen(this, &screen_location);
  // Hide `feedback_button_` on mouse exited.
  if (feedback_button_ &&
      !feedback_button_->GetBoundsInScreen().Contains(screen_location)) {
    RefreshFeedbackButton(/*visible=*/false);
  }
}

bool SplitViewDividerView::OnMousePressed(const ui::MouseEvent& event) {
  gfx::Point location(event.location());
  views::View::ConvertPointToScreen(this, &location);
  initial_mouse_event_location_ = location;
  return true;
}

bool SplitViewDividerView::OnMouseDragged(const ui::MouseEvent& event) {
  CHECK(divider_);
  RefreshFeedbackButton(/*visible=*/false);
  if (!mouse_move_started_) {
    // If this is the first mouse drag event, start the resize and reset
    // `mouse_move_started_`.
    DCHECK_NE(initial_mouse_event_location_, gfx::Point());
    mouse_move_started_ = true;
    StartResizing(initial_mouse_event_location_);
    return true;
  }

  // Else continue with the resize.
  gfx::Point location(event.location());
  views::View::ConvertPointToScreen(this, &location);
  divider_->ResizeWithDivider(location);
  return true;
}

void SplitViewDividerView::OnMouseReleased(const ui::MouseEvent& event) {
  gfx::Point location(event.location());
  views::View::ConvertPointToScreen(this, &location);
  initial_mouse_event_location_ = gfx::Point();
  mouse_move_started_ = false;
  EndResizing(location, /*swap_windows=*/event.GetClickCount() == 2);

  RefreshFeedbackButton(/*visible=*/true);
}

void SplitViewDividerView::OnGestureEvent(ui::GestureEvent* event) {
  CHECK(divider_);
  if (event->IsSynthesized()) {
    // When `divider_` is destroyed, closing the widget can cause a window
    // visibility change which will cancel active touches and dispatch a
    // synthetic touch event.
    return;
  }
  gfx::Point location(event->location());
  views::View::ConvertPointToScreen(this, &location);
  switch (event->type()) {
    case ui::ET_GESTURE_TAP:
      if (event->details().tap_count() == 2) {
        SwapWindows();
      }
      break;
    case ui::ET_GESTURE_TAP_DOWN:
      break;
    case ui::ET_GESTURE_SCROLL_BEGIN:
      StartResizing(location);
      break;
    case ui::ET_GESTURE_SCROLL_UPDATE:
      divider_->ResizeWithDivider(location);
      break;
    case ui::ET_GESTURE_END:
      EndResizing(location, /*swap_windows=*/false);
      break;

    default:
      break;
  }
  event->SetHandled();
}

ui::Cursor SplitViewDividerView::GetCursor(const ui::MouseEvent& event) {
  return IsLayoutHorizontal(divider_->divider_widget()->GetNativeWindow())
             ? ui::mojom::CursorType::kColumnResize
             : ui::mojom::CursorType::kRowResize;
}

bool SplitViewDividerView::DoesIntersectRect(const views::View* target,
                                             const gfx::Rect& rect) const {
  DCHECK_EQ(target, this);
  return true;
}

void SplitViewDividerView::SwapWindows() {
  CHECK(divider_);
  divider_->SwapWindows();
}

void SplitViewDividerView::OnResizeStatusChanged() {
  // If split view has ended, the divider widget will be closing. In this case
  // no need to update the divider layout and do the animation.
  if (!divider_ || !divider_->divider_widget()) {
    return;
  }

  // If `divider_view_`'s bounds are animating, it is for the divider spawning
  // animation. Stop that before animating `divider_view_`'s transform.
  ui::LayerAnimator* divider_animator = layer()->GetAnimator();
  divider_animator->StopAnimatingProperty(ui::LayerAnimationElement::BOUNDS);

  // Do the divider enlarge/shrink animation when starting/ending dragging.
  const bool is_resizing = divider_->is_resizing_with_divider();
  SetBoundsRect(GetLocalBounds());
  const gfx::Rect old_bounds =
      divider_->GetDividerBoundsInScreen(/*is_dragging=*/false);
  const gfx::Rect new_bounds = divider_->GetDividerBoundsInScreen(is_resizing);
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

  divider_handler_view_->Refresh(is_resizing);
}

void SplitViewDividerView::StartResizing(gfx::Point location) {
  CHECK(divider_);
  // `StartResizeWithDivider()` may cause this view to be destroyed.
  auto weak_ptr = weak_ptr_factory_.GetWeakPtr();
  divider_->StartResizeWithDivider(location);
  if (weak_ptr) {
    OnResizeStatusChanged();
  }
}

void SplitViewDividerView::RefreshFeedbackButton(bool visible) {
  if (!IsSnapGroupEnabledInClamshellMode()) {
    return;
  }

  if (!feedback_button_) {
    feedback_button_ = AddChildView(std::make_unique<IconButton>(
        base::BindRepeating(&SplitViewDividerView::OnFeedbackButtonPressed,
                            base::Unretained(this)),
        IconButton::Type::kMediumFloating, &kFeedbackIcon,
        IDS_ASH_SNAP_GROUP_SEND_FEEDBACK,
        /*is_togglable=*/false,
        /*has_border=*/false));
    feedback_button_->SetPaintToLayer();
    feedback_button_->layer()->SetFillsBoundsOpaquely(false);
    feedback_button_->SetPreferredSize(kFeedbackButtonSize);
    feedback_button_->SetIconSize(kFeedbackButtonIconSize);
    feedback_button_->SetIconColor(cros_tokens::kCrosSysOnSecondary);
    feedback_button_->SetVisible(true);
    feedback_button_->SetBackground(views::CreateThemedRoundedRectBackground(
        cros_tokens::kCrosSysSecondary, kFeedbackButtonSize.height() / 2.f));
  }

  feedback_button_->SetVisible(visible);
  RefreshFeedbackButtonBounds();
}

void SplitViewDividerView::RefreshFeedbackButtonBounds() {
  if (feedback_button_ && feedback_button_->GetVisible()) {
    const gfx::Size feedback_button_size = feedback_button_->GetPreferredSize();
    gfx::Rect feedback_button_bounds = gfx::Rect();
    if (IsLayoutHorizontal(divider_->GetRootWindow())) {
      feedback_button_bounds = gfx::Rect(
          (width() - feedback_button_size.width()) / 2.f,
          height() - feedback_button_size.height() -
              kFeedbackButtonDistanceFromEdge,
          feedback_button_size.width(), feedback_button_size.height());
    } else {
      feedback_button_bounds = gfx::Rect(
          width() - feedback_button_size.width() / 2.f -
              kFeedbackButtonDistanceFromEdge,
          height() - feedback_button_size.height() / 2,
          feedback_button_size.width(), feedback_button_size.height());
    }
    feedback_button_->SetBoundsRect(feedback_button_bounds);
  }
}

void SplitViewDividerView::EndResizing(gfx::Point location, bool swap_windows) {
  CHECK(divider_);
  // `EndResizeWithDivider()` may cause this view to be destroyed.
  auto weak_ptr = weak_ptr_factory_.GetWeakPtr();
  divider_->EndResizeWithDivider(location);
  if (!weak_ptr) {
    return;
  }
  OnResizeStatusChanged();
  if (swap_windows) {
    SwapWindows();
  }
}

void SplitViewDividerView::OnFeedbackButtonPressed() {
  Shell::Get()->shell_delegate()->OpenFeedbackDialog(
      /*source=*/ShellDelegate::FeedbackSource::kSnapGroups,
      /*description_template=*/std::string(),
      /*category_tag=*/"FromSnapGroups");
}

BEGIN_METADATA(SplitViewDividerView)
END_METADATA

}  // namespace ash
