// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/overview/overview_utils.h"

#include <utility>

#include "ash/accessibility/accessibility_controller_impl.h"
#include "ash/public/cpp/shelf_config.h"
#include "ash/public/cpp/window_properties.h"
#include "ash/root_window_controller.h"
#include "ash/scoped_animation_disabler.h"
#include "ash/screen_util.h"
#include "ash/shelf/shelf.h"
#include "ash/shelf/shelf_layout_manager.h"
#include "ash/shell.h"
#include "ash/wm/mru_window_tracker.h"
#include "ash/wm/overview/cleanup_animation_observer.h"
#include "ash/wm/overview/delayed_animation_observer_impl.h"
#include "ash/wm/overview/overview_controller.h"
#include "ash/wm/overview/overview_focus_cycler.h"
#include "ash/wm/overview/overview_grid.h"
#include "ash/wm/overview/overview_item.h"
#include "ash/wm/overview/overview_session.h"
#include "ash/wm/overview/scoped_overview_animation_settings.h"
#include "ash/wm/splitview/split_view_controller.h"
#include "ash/wm/splitview/split_view_overview_session.h"
#include "ash/wm/splitview/split_view_utils.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "ash/wm/window_state.h"
#include "ash/wm/window_transient_descendant_iterator.h"
#include "ash/wm/window_util.h"
#include "ash/wm/wm_event.h"
#include "ash/wm/work_area_insets.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/window.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/transform_util.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"
#include "ui/wm/core/coordinate_conversion.h"
#include "ui/wm/core/window_animations.h"

namespace ash {

bool IsInOverviewSession() {
  OverviewController* overview_controller = OverviewController::Get();
  return overview_controller && overview_controller->InOverviewSession();
}

OverviewSession* GetOverviewSession() {
  OverviewController* overview_controller = OverviewController::Get();
  return overview_controller && overview_controller->InOverviewSession()
             ? overview_controller->overview_session()
             : nullptr;
}

bool CanCoverAvailableWorkspace(aura::Window* window) {
  if (!window) {
    return false;
  }
  SplitViewController* split_view_controller = SplitViewController::Get(window);
  if (split_view_controller->InSplitViewMode())
    return split_view_controller->CanSnapWindow(window);
  return WindowState::Get(window)->IsMaximizedOrFullscreenOrPinned();
}

void FadeInWidgetToOverview(views::Widget* widget,
                            OverviewAnimationType animation_type,
                            bool observe) {
  aura::Window* window = widget->GetNativeWindow();
  if (window->layer()->GetTargetOpacity() == 1.f)
    return;

  // Fade in the widget from its current opacity.
  ScopedOverviewAnimationSettings scoped_overview_animation_settings(
      animation_type, window);
  window->layer()->SetOpacity(1.0f);

  if (observe) {
    auto enter_observer = std::make_unique<EnterAnimationObserver>();
    scoped_overview_animation_settings.AddObserver(enter_observer.get());
    OverviewController::Get()->AddEnterAnimationObserver(
        std::move(enter_observer));
  }
}

void FadeOutWidgetFromOverview(std::unique_ptr<views::Widget> widget,
                               OverviewAnimationType animation_type) {
  // Make it so the widget is no longer activatable, since it will be deleted
  // when the animation is complete.
  widget->widget_delegate()->SetCanActivate(false);

  // The overview controller may be nullptr on shutdown.
  OverviewController* controller = OverviewController::Get();
  if (!controller) {
    widget->SetOpacity(0.f);
    return;
  }

  // Fade out the widget from its current opacity. This animation continues past
  // the lifetime of overview mode items.
  ScopedOverviewAnimationSettings animation_settings(animation_type,
                                                     widget->GetNativeWindow());
  // CleanupAnimationObserver will delete itself (and the widget) when the
  // opacity animation is complete. Ownership over the observer is passed to the
  // overview controller which has longer lifetime so that animations can
  // continue even after the overview mode is shut down.
  views::Widget* widget_ptr = widget.get();
  auto observer = std::make_unique<CleanupAnimationObserver>(std::move(widget));
  animation_settings.AddObserver(observer.get());
  controller->AddExitAnimationObserver(std::move(observer));
  widget_ptr->SetOpacity(0.f);
}

void ImmediatelyCloseWidgetOnExit(std::unique_ptr<views::Widget> widget) {
  widget->GetNativeWindow()->SetProperty(aura::client::kAnimationsDisabledKey,
                                         true);
  widget->Close();
  widget.reset();
}

gfx::RectF GetUnionScreenBoundsForWindow(aura::Window* window) {
  gfx::RectF bounds;
  for (auto* window_iter :
       window_util::GetVisibleTransientTreeIterator(window)) {
    // Ignore other window types when computing bounding box of overview target
    // item.
    if (window_iter != window &&
        window_iter->GetType() != aura::client::WINDOW_TYPE_NORMAL) {
      continue;
    }
    gfx::RectF target_bounds(window_iter->GetTargetBounds());
    wm::TranslateRectToScreen(window_iter->parent(), &target_bounds);
    bounds.Union(target_bounds);
  }

  return bounds;
}

void SetTransform(aura::Window* window, const gfx::Transform& transform) {
  const gfx::PointF target_origin(
      GetUnionScreenBoundsForWindow(window).origin());
  for (auto* window_iter :
       window_util::GetVisibleTransientTreeIterator(window)) {
    aura::Window* parent_window = window_iter->parent();
    gfx::RectF original_bounds(window_iter->GetTargetBounds());
    ::wm::TranslateRectToScreen(parent_window, &original_bounds);
    const gfx::Transform new_transform = TransformAboutPivot(
        gfx::PointF(target_origin.x() - original_bounds.x(),
                    target_origin.y() - original_bounds.y()),
        transform);
    window_iter->SetTransform(new_transform);
  }
}

void MaximizeIfSnapped(aura::Window* window) {
  auto* window_state = WindowState::Get(window);
  if (window_state && window_state->IsSnapped()) {
    ScopedAnimationDisabler disabler(window);
    WMEvent event(WM_EVENT_MAXIMIZE);
    window_state->OnWMEvent(&event);
  }
}

gfx::Rect GetGridBoundsInScreen(aura::Window* target_root) {
  return GetGridBoundsInScreen(target_root,
                               /*window_dragging_state=*/absl::nullopt,
                               /*divider_changed=*/false,
                               /*account_for_hotseat=*/true);
}

gfx::Rect GetGridBoundsInScreen(
    aura::Window* target_root,
    absl::optional<SplitViewDragIndicators::WindowDraggingState>
        window_dragging_state,
    bool divider_changed,
    bool account_for_hotseat) {
  auto* split_view_controller = SplitViewController::Get(target_root);
  SplitViewController::State state = split_view_controller->state();

  // If we are in splitview mode already just use the given state, otherwise
  // convert `window_dragging_state` to a split view state. Note this will
  // override `state` from `split_view_overview_session` if there is any.
  if (!split_view_controller->InSplitViewMode() && window_dragging_state) {
    switch (*window_dragging_state) {
      case SplitViewDragIndicators::WindowDraggingState::kToSnapPrimary:
        state = SplitViewController::State::kPrimarySnapped;
        break;
      case SplitViewDragIndicators::WindowDraggingState::kToSnapSecondary:
        state = SplitViewController::State::kSecondarySnapped;
        break;
      default:
        break;
    }
  }

  gfx::Rect bounds;
  gfx::Rect work_area =
      WorkAreaInsets::ForWindow(target_root)->ComputeStableWorkArea();
  absl::optional<SplitViewController::SnapPosition> opposite_position;

  // We should show partial overview for the following use cases:
  // 1. In tablet split view mode;
  // 2. On one window snapped in clamshell mode with feature flag `kSnapGroup`
  // is enabled and feature param `kAutomaticallyLockGroup` is true;
  // 3. On one window snapped in clamshell in overview session.

  // When `kFasterSplitScreenSetup` or `kSnapGroup` is enabled, we would only
  // reach here if overview is in session and there is no divider.
  // TODO(b/296935443): Consolidate split view bounds calculations.
  if (window_util::IsFasterSplitScreenOrSnapGroupArm1Enabled() &&
      !Shell::Get()->IsInTabletMode()) {
    bounds = work_area;
    if (auto* split_view_overview_session =
            RootWindowController::ForWindow(target_root)
                ->split_view_overview_session()) {
      gfx::Rect target_bounds_in_screen(
          split_view_overview_session->window()->GetTargetBounds());
      wm::ConvertRectToScreen(target_root, &target_bounds_in_screen);
      bounds.Subtract(target_bounds_in_screen);
    }
  } else {
    switch (state) {
      case SplitViewController::State::kPrimarySnapped:
        bounds = split_view_controller->GetSnappedWindowBoundsInScreen(
            SplitViewController::SnapPosition::kSecondary,
            /*window_for_minimum_size=*/nullptr);
        opposite_position = SplitViewController::SnapPosition::kSecondary;
        break;
      case SplitViewController::State::kSecondarySnapped:
        bounds = split_view_controller->GetSnappedWindowBoundsInScreen(
            SplitViewController::SnapPosition::kPrimary,
            /*window_for_minimum_size=*/nullptr);
        opposite_position = SplitViewController::SnapPosition::kPrimary;
        break;
      case SplitViewController::State::kNoSnap:
        bounds = work_area;
        break;
      case SplitViewController::State::kBothSnapped:
        // When this function is called, SplitViewController should have already
        // handled the state change.
        NOTREACHED();
    }
  }

  // Hotseat overlaps the work area / split view bounds when extended, but in
  // some cases we don't want its bounds in our calculations.
  if (account_for_hotseat &&
      Shell::Get()->tablet_mode_controller()->InTabletMode()) {
    Shelf* shelf = Shelf::ForWindow(target_root);
    const bool hotseat_extended =
        shelf->shelf_layout_manager()->hotseat_state() ==
        HotseatState::kExtended;
    // When a window is dragged from the top of the screen, overview gets
    // entered immediately but the window does not get deactivated right away so
    // the hotseat state does not get updated until the window gets dragged a
    // bit. In this case, determine whether the hotseat will be extended to
    // avoid doing a expensive double grid layout.
    auto* overview_session = OverviewController::Get()->overview_session();
    const bool hotseat_will_extend =
        overview_session && overview_session->ShouldEnterWithoutAnimations() &&
        !split_view_controller->InSplitViewMode();
    if (hotseat_extended || hotseat_will_extend) {
      // Use the default hotseat size here to avoid the possible re-layout
      // due to the update in HotseatWidget::is_forced_dense_.
      const int hotseat_bottom_inset =
          ShelfConfig::Get()->GetHotseatSize(
              /*density=*/HotseatDensity::kNormal) +
          ShelfConfig::Get()->hotseat_bottom_padding();

      bounds.Inset(gfx::Insets::TLBR(0, 0, hotseat_bottom_inset, 0));
    }
  }

  if (!divider_changed) {
    return bounds;
  }

  DCHECK(opposite_position);
  const bool horizontal = SplitViewController::IsLayoutHorizontal(target_root);
  const int min_length =
      (horizontal ? work_area.width() : work_area.height()) / 3;
  const int current_length = horizontal ? bounds.width() : bounds.height();

  if (current_length > min_length)
    return bounds;

  // Clamp bounds' length to the minimum length.
  if (horizontal)
    bounds.set_width(min_length);
  else
    bounds.set_height(min_length);

  if (SplitViewController::IsPhysicalLeftOrTop(*opposite_position,
                                               target_root)) {
    // If we are shifting to the left or top we need to update the origin as
    // well.
    const int offset = min_length - current_length;
    bounds.Offset(horizontal ? gfx::Vector2d(-offset, 0)
                             : gfx::Vector2d(0, -offset));
  }

  return bounds;
}

absl::optional<gfx::RectF> GetSplitviewBoundsMaintainingAspectRatio() {
  if (!ShouldAllowSplitView())
    return absl::nullopt;
  if (!Shell::Get()->tablet_mode_controller()->InTabletMode())
    return absl::nullopt;
  auto* overview_session = OverviewController::Get()->overview_session();
  DCHECK(overview_session);
  aura::Window* root_window = Shell::GetPrimaryRootWindow();
  DCHECK(overview_session->GetGridWithRootWindow(root_window)
             ->split_view_drag_indicators());
  auto window_dragging_state =
      overview_session->GetGridWithRootWindow(root_window)
          ->split_view_drag_indicators()
          ->current_window_dragging_state();
  if (!SplitViewController::Get(root_window)->InSplitViewMode() &&
      SplitViewDragIndicators::GetSnapPosition(window_dragging_state) ==
          SplitViewController::SnapPosition::kNone) {
    return absl::nullopt;
  }

  // The hotseat bounds do not affect splitview after a window is snapped, so
  // the aspect ratio should reflect it and not worry about the hotseat.
  return gfx::RectF(GetGridBoundsInScreen(root_window, window_dragging_state,
                                          /*divider_changed=*/false,
                                          /*account_for_hotseat=*/false));
}

bool ShouldUseTabletModeGridLayout() {
  return Shell::Get()->tablet_mode_controller()->InTabletMode();
}

gfx::Rect ToStableSizeRoundedRect(const gfx::RectF& rect) {
  return gfx::Rect(gfx::ToRoundedPoint(rect.origin()),
                   gfx::ToRoundedSize(rect.size()));
}

void MoveFocusToView(OverviewFocusableView* target_view) {
  auto* overview_session = OverviewController::Get()->overview_session();
  CHECK(overview_session);

  auto* focus_cycler = overview_session->focus_cycler();
  CHECK(focus_cycler);

  focus_cycler->MoveFocusToView(target_view);
}

void SetWindowsVisibleDuringItemDragging(const aura::Window::Windows& windows,
                                         bool visible,
                                         bool animate) {
  float new_opacity = visible ? 1.f : 0.f;
  for (auto* window : windows) {
    ui::Layer* layer = window->layer();
    if (layer->GetTargetOpacity() == new_opacity) {
      continue;
    }

    if (animate) {
      ScopedOverviewAnimationSettings settings(
          OVERVIEW_ANIMATION_OPACITY_ON_WINDOW_DRAG, window);
      layer->SetOpacity(new_opacity);
    } else {
      layer->SetOpacity(new_opacity);
    }
  }
}

}  // namespace ash
