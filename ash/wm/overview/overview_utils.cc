// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/overview/overview_utils.h"

#include <utility>

#include "ash/home_screen/home_launcher_gesture_handler.h"
#include "ash/home_screen/home_screen_controller.h"
#include "ash/public/cpp/ash_features.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/public/cpp/window_properties.h"
#include "ash/scoped_animation_disabler.h"
#include "ash/screen_util.h"
#include "ash/shell.h"
#include "ash/wm/overview/cleanup_animation_observer.h"
#include "ash/wm/overview/delayed_animation_observer_impl.h"
#include "ash/wm/overview/overview_controller.h"
#include "ash/wm/overview/overview_grid.h"
#include "ash/wm/overview/scoped_overview_animation_settings.h"
#include "ash/wm/splitview/split_view_controller.h"
#include "ash/wm/splitview/split_view_utils.h"
#include "ash/wm/window_state.h"
#include "ash/wm/window_transient_descendant_iterator.h"
#include "ash/wm/wm_event.h"
#include "base/no_destructor.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/window.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/scoped_canvas.h"
#include "ui/gfx/transform_util.h"
#include "ui/views/background.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/core/coordinate_conversion.h"
#include "ui/wm/core/window_animations.h"

namespace ash {

namespace {

// The transform applied to an overview item when animating to or from the home
// launcher.
const gfx::Transform& GetShiftTransform() {
  static const base::NoDestructor<gfx::Transform> matrix(1, 0, 0, 1, 0, -100);
  return *matrix;
}

}  // namespace

bool CanCoverAvailableWorkspace(aura::Window* window) {
  if (SplitViewController::Get(window)->InSplitViewMode())
    return CanSnapInSplitview(window);
  return WindowState::Get(window)->IsMaximizedOrFullscreenOrPinned();
}

void FadeInWidgetAndMaybeSlideOnEnter(views::Widget* widget,
                                      OverviewAnimationType animation_type,
                                      bool slide,
                                      bool observe) {
  aura::Window* window = widget->GetNativeWindow();
  if (window->layer()->GetTargetOpacity() == 1.f && !slide)
    return;

  gfx::Transform original_transform = window->transform();
  if (slide) {
    // Translate the window up before sliding down to |original_transform|.
    gfx::Transform new_transform = original_transform;
    new_transform.ConcatTransform(GetShiftTransform());
    if (window->layer()->GetTargetOpacity() == 1.f &&
        window->layer()->GetTargetTransform() == new_transform) {
      return;
    }
    window->SetTransform(new_transform);
  }
  window->layer()->SetOpacity(0.0f);
  ScopedOverviewAnimationSettings scoped_overview_animation_settings(
      animation_type, window);
  window->layer()->SetOpacity(1.0f);
  if (slide)
    window->SetTransform(original_transform);

  if (observe) {
    auto enter_observer = std::make_unique<EnterAnimationObserver>();
    scoped_overview_animation_settings.AddObserver(enter_observer.get());
    Shell::Get()->overview_controller()->AddEnterAnimationObserver(
        std::move(enter_observer));
  }
}

void FadeOutWidgetAndMaybeSlideOnExit(std::unique_ptr<views::Widget> widget,
                                      OverviewAnimationType animation_type,
                                      bool slide) {
  // The overview controller may be nullptr on shutdown.
  OverviewController* controller = Shell::Get()->overview_controller();
  if (!controller) {
    widget->SetOpacity(0.f);
    return;
  }

  widget->SetOpacity(1.f);
  // Fade out the widget. This animation continues past the lifetime of overview
  // mode items.
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

  if (slide) {
    gfx::Transform new_transform = widget_ptr->GetNativeWindow()->transform();
    new_transform.ConcatTransform(GetShiftTransform());
    widget_ptr->GetNativeWindow()->SetTransform(new_transform);
  }
}

void ImmediatelyCloseWidgetOnExit(std::unique_ptr<views::Widget> widget) {
  widget->GetNativeWindow()->SetProperty(aura::client::kAnimationsDisabledKey,
                                         true);
  widget->Close();
  widget.reset();
}

WindowTransientDescendantIteratorRange GetVisibleTransientTreeIterator(
    aura::Window* window) {
  auto hide_predicate = [](aura::Window* window) {
    return window->GetProperty(kHideInOverviewKey);
  };
  return GetTransientTreeIterator(window, base::BindRepeating(hide_predicate));
}

gfx::RectF GetTransformedBounds(aura::Window* transformed_window,
                                int top_inset) {
  gfx::RectF bounds;
  for (auto* window : GetVisibleTransientTreeIterator(transformed_window)) {
    // Ignore other window types when computing bounding box of overview target
    // item.
    if (window != transformed_window &&
        window->type() != aura::client::WINDOW_TYPE_NORMAL) {
      continue;
    }
    gfx::RectF window_bounds(window->GetTargetBounds());
    gfx::Transform new_transform =
        TransformAboutPivot(gfx::ToRoundedPoint(window_bounds.origin()),
                            window->layer()->GetTargetTransform());
    new_transform.TransformRect(&window_bounds);

    // The preview title is shown above the preview window. Hide the window
    // header for apps or browser windows with no tabs (web apps) to avoid
    // showing both the window header and the preview title.
    if (top_inset > 0) {
      gfx::RectF header_bounds(window_bounds);
      header_bounds.set_height(top_inset);
      new_transform.TransformRect(&header_bounds);
      window_bounds.Inset(0, header_bounds.height(), 0, 0);
    }
    ::wm::TranslateRectToScreen(window->parent(), &window_bounds);
    bounds.Union(window_bounds);
  }
  return bounds;
}

gfx::RectF GetTargetBoundsInScreen(aura::Window* window) {
  gfx::RectF bounds;
  for (auto* window_iter : GetVisibleTransientTreeIterator(window)) {
    // Ignore other window types when computing bounding box of overview target
    // item.
    if (window_iter != window &&
        window_iter->type() != aura::client::WINDOW_TYPE_NORMAL) {
      continue;
    }
    gfx::RectF target_bounds(window_iter->GetTargetBounds());
    ::wm::TranslateRectToScreen(window_iter->parent(), &target_bounds);
    bounds.Union(target_bounds);
  }
  return bounds;
}

void SetTransform(aura::Window* window, const gfx::Transform& transform) {
  gfx::PointF target_origin(GetTargetBoundsInScreen(window).origin());
  for (auto* window_iter : GetVisibleTransientTreeIterator(window)) {
    aura::Window* parent_window = window_iter->parent();
    gfx::RectF original_bounds(window_iter->GetTargetBounds());
    ::wm::TranslateRectToScreen(parent_window, &original_bounds);
    gfx::Transform new_transform =
        TransformAboutPivot(gfx::Point(target_origin.x() - original_bounds.x(),
                                       target_origin.y() - original_bounds.y()),
                            transform);
    window_iter->SetTransform(new_transform);
  }
}

bool IsSlidingOutOverviewFromShelf() {
  if (!Shell::Get()->overview_controller()->InOverviewSession())
    return false;

  if (Shell::Get()
          ->home_screen_controller()
          ->home_launcher_gesture_handler()
          ->mode() == HomeLauncherGestureHandler::Mode::kSlideUpToShow) {
    return true;
  }

  return false;
}

void MaximizeIfSnapped(aura::Window* window) {
  auto* window_state = WindowState::Get(window);
  if (window_state && window_state->IsSnapped()) {
    ScopedAnimationDisabler disabler(window);
    WMEvent event(WM_EVENT_MAXIMIZE);
    window_state->OnWMEvent(&event);
  }
}

// Get the grid bounds if a window is snapped in splitview, or what they will be
// when snapped based on |target_root| and |indicator_state|.
gfx::Rect GetGridBoundsInScreenForSplitview(
    aura::Window* target_root,
    base::Optional<SplitViewDragIndicators::WindowDraggingState>
        window_dragging_state) {
  auto* split_view_controller = SplitViewController::Get(target_root);
  auto state = split_view_controller->state();

  // If we are in splitview mode already just use the given state, otherwise
  // convert |window_dragging_state| to a split view state.
  if (!split_view_controller->InSplitViewMode() && window_dragging_state) {
    switch (*window_dragging_state) {
      case SplitViewDragIndicators::WindowDraggingState::kToSnapLeft:
        state = SplitViewController::State::kLeftSnapped;
        break;
      case SplitViewDragIndicators::WindowDraggingState::kToSnapRight:
        state = SplitViewController::State::kRightSnapped;
        break;
      default:
        break;
    }
  }

  switch (state) {
    case SplitViewController::State::kLeftSnapped:
      return split_view_controller->GetSnappedWindowBoundsInScreen(
          SplitViewController::RIGHT, /*window_for_minimum_size=*/nullptr);
    case SplitViewController::State::kRightSnapped:
      return split_view_controller->GetSnappedWindowBoundsInScreen(
          SplitViewController::LEFT, /*window_for_minimum_size=*/nullptr);
    default:
      return screen_util::
          GetDisplayWorkAreaBoundsInScreenForActiveDeskContainer(target_root);
  }
}

base::Optional<gfx::RectF> GetSplitviewBoundsMaintainingAspectRatio(
    aura::Window* window) {
  if (!ShouldAllowSplitView())
    return base::nullopt;
  auto* overview_session =
      Shell::Get()->overview_controller()->overview_session();
  DCHECK(overview_session);
  aura::Window* root_window = Shell::GetPrimaryRootWindow();
  DCHECK(overview_session->GetGridWithRootWindow(root_window)
             ->split_view_drag_indicators());
  // TODO(sammiequon): This does not work for drag from top as they have
  // different drag indicators object as regular overview.
  auto window_dragging_state =
      overview_session->GetGridWithRootWindow(root_window)
          ->split_view_drag_indicators()
          ->current_window_dragging_state();
  if (!SplitViewController::Get(root_window)->InSplitViewMode() &&
      SplitViewDragIndicators::GetSnapPosition(window_dragging_state) ==
          SplitViewController::NONE) {
    return base::nullopt;
  }

  return base::make_optional(gfx::RectF(GetGridBoundsInScreenForSplitview(
      root_window, base::make_optional(window_dragging_state))));
}

bool ShouldUseTabletModeGridLayout() {
  return base::FeatureList::IsEnabled(features::kNewOverviewLayout) &&
         Shell::Get()->tablet_mode_controller()->InTabletMode();
}

}  // namespace ash
