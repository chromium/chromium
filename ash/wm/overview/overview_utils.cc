// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/overview/overview_utils.h"

#include <utility>

#include "ash/accessibility/accessibility_controller.h"
#include "ash/constants/ash_features.h"
#include "ash/public/cpp/shelf_config.h"
#include "ash/public/cpp/window_properties.h"
#include "ash/root_window_controller.h"
#include "ash/screen_util.h"
#include "ash/shelf/shelf.h"
#include "ash/shelf/shelf_layout_manager.h"
#include "ash/shell.h"
#include "ash/wm/mru_window_tracker.h"
#include "ash/wm/overview/cleanup_animation_observer.h"
#include "ash/wm/overview/delayed_animation_observer_impl.h"
#include "ash/wm/overview/overview_constants.h"
#include "ash/wm/overview/overview_controller.h"
#include "ash/wm/overview/overview_grid.h"
#include "ash/wm/overview/overview_item.h"
#include "ash/wm/overview/overview_session.h"
#include "ash/wm/overview/scoped_overview_animation_settings.h"
#include "ash/wm/snap_group/snap_group_controller.h"
#include "ash/wm/splitview/split_view_controller.h"
#include "ash/wm/splitview/split_view_overview_session.h"
#include "ash/wm/splitview/split_view_types.h"
#include "ash/wm/splitview/split_view_utils.h"
#include "ash/wm/window_state.h"
#include "ash/wm/window_transient_descendant_iterator.h"
#include "ash/wm/window_util.h"
#include "ash/wm/wm_event.h"
#include "ash/wm/work_area_insets.h"
#include "chromeos/ui/frame/caption_buttons/snap_controller.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/window.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/compositor/layer.h"
#include "ui/display/screen.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/transform_util.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"
#include "ui/wm/core/coordinate_conversion.h"
#include "ui/wm/core/scoped_animation_disabler.h"
#include "ui/wm/core/window_animations.h"

namespace ash {

constexpr int kBirchBarHotseatSpacing = 10;

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
    return split_view_controller->CanKeepCurrentSnapRatio(window);
  return WindowState::Get(window)->IsMaximizedOrFullscreenOrPinned();
}

void FadeInAndTransformWidgetToOverview(views::Widget* widget,
                                        const gfx::Transform& target_transform,
                                        OverviewAnimationType animation_type,
                                        bool observe) {
  aura::Window* window = widget->GetNativeWindow();
  auto* window_layer = window->layer();
  const bool animate_opacity = window_layer->GetTargetOpacity() != 1;
  const bool animate_transform =
      window_layer->GetTargetTransform() != target_transform;
  if (!animate_opacity && !animate_transform) {
    return;
  }

  // Fade in the widget from its current opacity.
  ScopedOverviewAnimationSettings scoped_overview_animation_settings(
      animation_type, window);
  if (animate_opacity) {
    window_layer->SetOpacity(1.0f);
  }

  if (animate_transform) {
    window_layer->SetTransform(target_transform);
  }

  if (observe) {
    auto enter_observer = std::make_unique<EnterAnimationObserver>();
    scoped_overview_animation_settings.AddObserver(enter_observer.get());
    OverviewController::Get()->AddEnterAnimationObserver(
        std::move(enter_observer));
  }
}

void FadeInWidgetToOverview(views::Widget* widget,
                            OverviewAnimationType animation_type,
                            bool observe) {
  FadeInAndTransformWidgetToOverview(widget,
                                     widget->GetLayer()->GetTargetTransform(),
                                     animation_type, observe);
}

void PrepareWidgetForShutdownAnimation(views::Widget* widget) {
  // The widget should no longer process events at this point.
  widget->SetVisibilityChangedAnimationsEnabled(false);
  widget->widget_delegate()->SetCanActivate(false);
  widget->GetNativeWindow()->SetEventTargetingPolicy(
      aura::EventTargetingPolicy::kNone);
  widget->GetContentsView()->SetCanProcessEventsWithinSubtree(false);
  widget->GetFocusManager()->set_shortcut_handling_suspended(true);
}

void FadeOutWidgetFromOverview(std::unique_ptr<views::Widget> widget,
                               OverviewAnimationType animation_type) {
  PrepareWidgetForShutdownAnimation(widget.get());

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

OverviewItemFillMode GetOverviewItemFillMode(const gfx::Size& size) {
  if (size.width() > size.height() * kExtremeWindowRatioThreshold) {
    return OverviewItemFillMode::kLetterBoxed;
  }

  if (size.height() > size.width() * kExtremeWindowRatioThreshold) {
    return OverviewItemFillMode::kPillarBoxed;
  }

  return OverviewItemFillMode::kNormal;
}

OverviewItemFillMode GetOverviewItemFillModeForWindow(aura::Window* window) {
  SnapGroupController* snap_group_controller = SnapGroupController::Get();
  if (!snap_group_controller ||
      !snap_group_controller->GetSnapGroupForGivenWindow(window)) {
    return GetOverviewItemFillMode(window->bounds().size());
  }

  return OverviewItemFillMode::kNormal;
}

void MaximizeIfSnapped(aura::Window* window) {
  auto* window_state = WindowState::Get(window);
  if (window_state && window_state->IsSnapped()) {
    wm::ScopedAnimationDisabler disabler(window);
    WMEvent event(WM_EVENT_MAXIMIZE);
    window_state->OnWMEvent(&event);
  }
}

gfx::Rect GetGridBoundsInScreen(aura::Window* target_root) {
  return GetGridBoundsInScreen(target_root,
                               /*window_dragging_state=*/std::nullopt,
                               /*account_for_hotseat=*/true);
}

gfx::Rect GetGridBoundsInScreen(
    aura::Window* target_root,
    std::optional<SplitViewDragIndicators::WindowDraggingState>
        window_dragging_state,
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

  const gfx::Rect work_area =
      WorkAreaInsets::ForWindow(target_root)->ComputeStableWorkArea();
  gfx::Rect bounds = work_area;
  std::optional<SnapPosition> opposite_position;

  // We should show partial overview for the following use cases:
  // 1. In tablet split view mode with one window snapped.
  // 2. In clamshell `SplitViewOverviewSession`.
  if (auto* split_view_overview_session =
          RootWindowController::ForWindow(target_root)
              ->split_view_overview_session()) {
    aura::Window* snapped_window = split_view_overview_session->window();
    gfx::Rect target_bounds_in_screen(snapped_window->GetTargetBounds());
    WindowState* window_state = WindowState::Get(snapped_window);
    CHECK(window_state->IsSnapped());
    opposite_position = window_state->GetStateType() ==
                                chromeos::WindowStateType::kPrimarySnapped
                            ? SnapPosition::kSecondary
                            : SnapPosition::kPrimary;
    wm::ConvertRectToScreen(target_root, &target_bounds_in_screen);
    bounds.Subtract(target_bounds_in_screen);
  } else {
    const bool account_for_divider_width =
        display::Screen::GetScreen()->InTabletMode();
    switch (state) {
      case SplitViewController::State::kPrimarySnapped:
        bounds = split_view_controller->GetSnappedWindowBoundsInScreen(
            SnapPosition::kSecondary,
            /*window_for_minimum_size=*/nullptr, chromeos::kDefaultSnapRatio,
            account_for_divider_width);
        opposite_position = SnapPosition::kSecondary;
        break;
      case SplitViewController::State::kSecondarySnapped:
        bounds = split_view_controller->GetSnappedWindowBoundsInScreen(
            SnapPosition::kPrimary,
            /*window_for_minimum_size=*/nullptr, chromeos::kDefaultSnapRatio,
            account_for_divider_width);
        opposite_position = SnapPosition::kPrimary;
        break;
      case SplitViewController::State::kNoSnap:
        break;
      case SplitViewController::State::kBothSnapped:
        // When this function is called, SplitViewController should have
        // already handled the state change.
        NOTREACHED();
    }
  }

  // Hotseat overlaps the work area / split view bounds when extended, but in
  // some cases we don't want its bounds in our calculations. When the forest
  // features are enabled, we want to exclude the hotseat bounds when it is
  // shown in home launcher.
  if (account_for_hotseat && display::Screen::GetScreen()->InTabletMode()) {
    const HotseatState hotseat_state =
        Shelf::ForWindow(target_root)->shelf_layout_manager()->hotseat_state();

    const bool hotseat_extended = hotseat_state == HotseatState::kExtended;
    const bool show_home_launcher =
        hotseat_state == HotseatState::kShownHomeLauncher;

    const bool forest_enabled = features::IsForestFeatureEnabled();

    const int hotseat_bottom_inset =
        ShelfConfig::Get()->GetHotseatSize(HotseatDensity::kNormal) +
        ShelfConfig::Get()->hotseat_bottom_padding();

    if (!forest_enabled && hotseat_extended) {
      bounds.Inset(gfx::Insets::TLBR(0, 0, hotseat_bottom_inset, 0));
    } else if (forest_enabled && show_home_launcher) {
      // If the home launcher is shown, add some extra spacing between the birch
      // bar and the hotseat. Subtract the in app shelf size since it is already
      // factored in the work area calculations.
      bounds.Inset(
          gfx::Insets::TLBR(0, 0,
                            hotseat_bottom_inset + kBirchBarHotseatSpacing -
                                ShelfConfig::Get()->in_app_shelf_size(),
                            0));
    }
  }

  // Clamp the bounds of the overview grid such that it doesn't go below
  // `kOverviewGridClampThresholdRatio` of the work area length.
  const bool horizontal = IsLayoutHorizontal(target_root);
  const int min_length = (horizontal ? work_area.width() : work_area.height()) *
                         kOverviewGridClampThresholdRatio;
  const int current_length = horizontal ? bounds.width() : bounds.height();

  if (current_length > min_length)
    return bounds;

  // Clamp bounds' corresponding length to the minimum length.
  if (horizontal)
    bounds.set_width(min_length);
  else
    bounds.set_height(min_length);

  // These changes below are crucial for better visualization and help
  // preventing crashes when dragging the snapped window towards the edge. In
  // this case, the overview components will become too small to allow any
  // gradient painting on desk bar or applying shadows. Please ensure to go
  // through the bounds update below when one window is snapped in overview both
  // in clamshell and tablet mode. See the regression behavior in
  // http://b/324478757.
  if (opposite_position &&
      IsPhysicallyLeftOrTop(*opposite_position, target_root)) {
    // If we are shifting to the left or top we need to update the origin as
    // well.
    const int offset = min_length - current_length;
    bounds.Offset(horizontal ? gfx::Vector2d(-offset, 0)
                             : gfx::Vector2d(0, -offset));
  }

  return bounds;
}

std::optional<gfx::RectF> GetSplitviewBoundsMaintainingAspectRatio() {
  if (!ShouldAllowSplitView()) {
    return std::nullopt;
  }
  if (!display::Screen::GetScreen()->InTabletMode()) {
    return std::nullopt;
  }
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
          SnapPosition::kNone) {
    return std::nullopt;
  }

  // The hotseat bounds do not affect splitview after a window is snapped, so
  // the aspect ratio should reflect it and not worry about the hotseat.
  return gfx::RectF(GetGridBoundsInScreen(root_window, window_dragging_state,
                                          /*account_for_hotseat=*/false));
}

bool ShouldUseTabletModeGridLayout() {
  return display::Screen::GetScreen()->InTabletMode();
}

gfx::Rect ToStableSizeRoundedRect(const gfx::RectF& rect) {
  return gfx::Rect(gfx::ToRoundedPoint(rect.origin()),
                   gfx::ToRoundedSize(rect.size()));
}

bool IsEligibleForDraggingToSnapInOverview(OverviewItemBase* item) {
  return (item->GetWindows().size() == 1u) && ShouldAllowSplitView();
}

void SetWindowsVisibleDuringItemDragging(const aura::Window::Windows& windows,
                                         bool visible,
                                         bool animate) {
  float new_opacity = visible ? 1.f : 0.f;
  for (aura::Window* window : windows) {
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

ui::ImageModel CreateIconForMenuItem(const gfx::VectorIcon& icon) {
  constexpr ui::ColorId kMenuIconColorId = cros_tokens::kCrosSysOnSurface;
  constexpr int kMenuIconSize = 20;
  return ui::ImageModel::FromVectorIcon(icon, kMenuIconColorId, kMenuIconSize);
}

}  // namespace ash
