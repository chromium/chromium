// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/splitview/split_view_divider_view.h"

#include "ash/display/screen_orientation_controller.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/public/cpp/window_properties.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_id.h"
#include "ash/style/ash_color_provider.h"
#include "ash/system/screen_layout_observer.h"
#include "ash/utility/cursor_setter.h"
#include "ash/wm/snap_group/snap_group.h"
#include "ash/wm/snap_group/snap_group_controller.h"
#include "ash/wm/snap_group/snap_group_expanded_menu_view.h"
#include "ash/wm/splitview/split_view_constants.h"
#include "ash/wm/splitview/split_view_controller.h"
#include "ash/wm/splitview/split_view_divider.h"
#include "ash/wm/splitview/split_view_divider_handler_view.h"
#include "ash/wm/splitview/split_view_utils.h"
#include "chromeos/constants/chromeos_features.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/background.h"
#include "ui/views/highlight_border.h"
#include "ui/views/view.h"
#include "ui/wm/core/coordinate_conversion.h"

namespace ash {

namespace {

constexpr int kKebabButtonDistanceFromBottom = 24;
constexpr gfx::Size kKebabButtonSize{4, 24};
constexpr int kDistanceBetweenKebabButtonAndExpandedMenu = 8;
constexpr int kExpandedMenuHeight = 150;

bool IsInTabletMode() {
  TabletModeController* tablet_mode_controller =
      Shell::Get()->tablet_mode_controller();
  return tablet_mode_controller && tablet_mode_controller->InTabletMode();
}

}  // namespace

SplitViewDividerView::SplitViewDividerView(SplitViewController* controller,
                                           SplitViewDivider* divider)
    : split_view_controller_(controller),
      divider_handler_view_(
          AddChildView(std::make_unique<SplitViewDividerHandlerView>())),
      divider_(divider) {
  SetEventTargeter(std::make_unique<views::ViewTargeter>(this));

  SetPaintToLayer(ui::LAYER_TEXTURED);
  layer()->SetFillsBoundsOpaquely(false);

  const bool is_jellyroll_enabled = chromeos::features::IsJellyrollEnabled();

  SetBackground(views::CreateThemedSolidBackground(
      is_jellyroll_enabled
          ? static_cast<ui::ColorId>(cros_tokens::kCrosSysSystemBaseElevated)
          : kColorAshShieldAndBaseOpaque));
  SetBorder(std::make_unique<views::HighlightBorder>(
      /*corner_radius=*/0,
      is_jellyroll_enabled
          ? views::HighlightBorder::Type::kHighlightBorderNoShadow
          : views::HighlightBorder::Type::kHighlightBorder1));

  if (IsSnapGroupEnabledInClamshellMode()) {
    kebab_button_ = AddChildView(std::make_unique<IconButton>(
        base::BindRepeating(&SplitViewDividerView::OnKebabButtonPressed,
                            base::Unretained(this)),
        IconButton::Type::kMediumFloating, &kSnapGroupKebabIcon,
        IDS_ASH_SNAP_GROUP_MORE_OPTIONS,
        /*is_togglable=*/false,
        /*has_border=*/false));
    kebab_button_->SetPaintToLayer();
    kebab_button_->layer()->SetFillsBoundsOpaquely(false);
    kebab_button_->SetPreferredSize(kKebabButtonSize);
    kebab_button_->SetVisible(true);
  }
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
  if (!IsInTabletMode() && !IsSnapGroupEnabledInClamshellMode()) {
    return;
  }

  SetBoundsRect(GetLocalBounds());
  divider_handler_view_->Refresh(
      split_view_controller_->IsResizingWithDivider());

  if (IsSnapGroupEnabledInClamshellMode()) {
    const gfx::Size kebab_button_size = kebab_button_->GetPreferredSize();
    const gfx::Rect kebab_button_bounds(
        (width() - kebab_button_size.width()) / 2.f,
        height() - kebab_button_size.height() - kKebabButtonDistanceFromBottom,
        kebab_button_size.width(), kebab_button_size.height());
    kebab_button_->SetBoundsRect(kebab_button_bounds);
  }
}

void SplitViewDividerView::OnMouseEntered(const ui::MouseEvent& event) {
  gfx::Point screen_location = event.location();
  ConvertPointToScreen(this, &screen_location);

  // Only set cursor type as the resize cursor when it's on the split view
  // divider.
  if (kebab_button_ &&
      !kebab_button_->GetBoundsInScreen().Contains(screen_location)) {
    // TODO(b/276801578): Use `kRowResize` cursor type for vertical split view.
    cursor_setter_.UpdateCursor(split_view_controller_->root_window(),
                                ui::mojom::CursorType::kColumnResize);
  }
}

void SplitViewDividerView::OnMouseExited(const ui::MouseEvent& event) {
  // Since `notify_enter_exit_on_child_` in view.h is default to false, on mouse
  // exit `this` the cursor will be reset, which includes resetting the cursor
  // when hovering over the kebab button area.
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
    split_view_controller_->SwapWindows(
        SplitViewController::SwapWindowsSource::kDoubleTap);
  }
}

void SplitViewDividerView::OnGestureEvent(ui::GestureEvent* event) {
  gfx::Point location(event->location());
  views::View::ConvertPointToScreen(this, &location);
  switch (event->type()) {
    case ui::ET_GESTURE_TAP:
      if (event->details().tap_count() == 2) {
        split_view_controller_->SwapWindows(
            SplitViewController::SwapWindowsSource::kDoubleTap);
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

void SplitViewDividerView::OnResizeStatusChanged() {
  // It's possible that when this function is called, split view mode has
  // been ended, and the divider widget is to be deleted soon. In this case
  // no need to update the divider layout and do the animation.
  if (!split_view_controller_->InSplitViewMode()) {
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

void SplitViewDividerView::OnKebabButtonPressed() {
  should_show_expanded_menu_ = !should_show_expanded_menu_;
  if (!should_show_expanded_menu_) {
    snap_group_expanded_menu_widget_.reset();
    snap_group_expanded_menu_view_ = nullptr;
    return;
  }

  if (!snap_group_expanded_menu_widget_) {
    snap_group_expanded_menu_widget_ = std::make_unique<views::Widget>();
    snap_group_expanded_menu_widget_->Init(CreateWidgetInitParams(
        split_view_controller_->root_window(), "SnapGroupExpandedMenuWidget"));
    SnapGroupController* snap_group_controller = SnapGroupController::Get();
    SnapGroup* snap_group = snap_group_controller->GetSnapGroupForGivenWindow(
        split_view_controller_->primary_window());
    CHECK(snap_group);
    snap_group_expanded_menu_view_ =
        snap_group_expanded_menu_widget_->SetContentsView(
            std::make_unique<SnapGroupExpandedMenuView>(snap_group));
  }
  snap_group_expanded_menu_widget_->Show();
  MaybeUpdateExpandedMenuWidgetBounds();
}

void SplitViewDividerView::MaybeUpdateExpandedMenuWidgetBounds() {
  CHECK(snap_group_expanded_menu_widget_);
  const auto kebab_button_bounds = kebab_button_->GetBoundsInScreen();
  gfx::Rect divider_bounds_in_screen =
      split_view_controller_->split_view_divider()->GetDividerBoundsInScreen(
          /*is_dragging=*/false);
  const gfx::Rect expanded_menu_bounds(
      divider_bounds_in_screen.x() + kSplitviewDividerShortSideLength / 2 -
          kExpandedMenuRoundedCornerRadius,
      kebab_button_bounds.y() - kExpandedMenuHeight -
          kDistanceBetweenKebabButtonAndExpandedMenu,
      kExpandedMenuRoundedCornerRadius * 2, kExpandedMenuHeight);
  divider_bounds_in_screen.ClampToCenteredSize(
      gfx::Size(kExpandedMenuRoundedCornerRadius * 2, kExpandedMenuHeight));
  snap_group_expanded_menu_widget_->SetBounds(expanded_menu_bounds);
}

BEGIN_METADATA(SplitViewDividerView, views::View)
END_METADATA

}  // namespace ash
