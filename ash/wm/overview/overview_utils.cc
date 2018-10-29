// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/overview/overview_utils.h"

#include <utility>

#include "ash/app_list/app_list_controller_impl.h"
#include "ash/app_list/home_launcher_gesture_handler.h"
#include "ash/public/cpp/ash_features.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/shell.h"
#include "ash/wm/overview/cleanup_animation_observer.h"
#include "ash/wm/overview/scoped_overview_animation_settings.h"
#include "ash/wm/overview/window_selector_controller.h"
#include "ash/wm/splitview/split_view_controller.h"
#include "ash/wm/window_state.h"
#include "ash/wm/window_transient_descendant_iterator.h"
#include "base/no_destructor.h"
#include "third_party/skia/include/pathops/SkPathOps.h"
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

// The transform applied to a window selector item when animating to or from the
// home launcher.
const gfx::Transform& GetShiftTransform() {
  static const base::NoDestructor<gfx::Transform> matrix(1, 0, 0, 1, 0, -100);
  return *matrix;
}

// BackgroundWith1PxBorder renders a solid background color, with a one pixel
// border with rounded corners. This accounts for the scaling of the canvas, so
// that the border is 1 pixel thick regardless of display scaling.
class BackgroundWith1PxBorder : public views::Background {
 public:
  BackgroundWith1PxBorder(SkColor background,
                          SkColor border_color,
                          int border_thickness,
                          int corner_radius)
      : border_color_(border_color),
        border_thickness_(border_thickness),
        corner_radius_(corner_radius) {
    SetNativeControlColor(background);
  }

  // views::Background:
  void Paint(gfx::Canvas* canvas, views::View* view) const override {
    gfx::RectF border_rect_f(view->GetContentsBounds());

    gfx::ScopedCanvas scoped_canvas(canvas);
    const float scale = canvas->UndoDeviceScaleFactor();
    border_rect_f.Inset(border_thickness_, border_thickness_);
    border_rect_f = gfx::ScaleRect(border_rect_f, scale);

    SkPath path;
    const SkScalar scaled_corner_radius =
        SkIntToScalar(gfx::ToCeiledInt(corner_radius_ * scale));
    path.addRoundRect(gfx::RectFToSkRect(border_rect_f), scaled_corner_radius,
                      scaled_corner_radius);

    cc::PaintFlags flags;
    flags.setStyle(cc::PaintFlags::kStroke_Style);
    flags.setStrokeWidth(1);
    flags.setAntiAlias(true);

    SkPath stroke_path;
    flags.getFillPath(path, &stroke_path);

    SkPath fill_path;
    Op(path, stroke_path, kDifference_SkPathOp, &fill_path);
    flags.setStyle(cc::PaintFlags::kFill_Style);
    flags.setColor(get_color());
    canvas->sk_canvas()->drawPath(fill_path, flags);

    if (border_thickness_ > 0) {
      flags.setColor(border_color_);
      canvas->sk_canvas()->drawPath(stroke_path, flags);
    }
  }

 private:
  // Color for the one pixel border.
  const SkColor border_color_;

  // Thickness of border inset.
  const int border_thickness_;

  // Corner radius of the inside edge of the roundrect border stroke.
  const int corner_radius_;

  DISALLOW_COPY_AND_ASSIGN(BackgroundWith1PxBorder);
};

}  // namespace

bool CanCoverAvailableWorkspace(aura::Window* window) {
  SplitViewController* split_view_controller =
      Shell::Get()->split_view_controller();
  if (split_view_controller->IsSplitViewModeActive())
    return split_view_controller->CanSnap(window);
  return wm::GetWindowState(window)->IsMaximizedOrFullscreenOrPinned();
}

bool IsOverviewSwipeToCloseEnabled() {
  return base::FeatureList::IsEnabled(features::kOverviewSwipeToClose);
}

void FadeInWidgetAndMaybeSlideOnEnter(views::Widget* widget,
                                      OverviewAnimationType animation_type,
                                      bool slide) {
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
}

void FadeOutWidgetAndMaybeSlideOnExit(std::unique_ptr<views::Widget> widget,
                                      OverviewAnimationType animation_type,
                                      bool slide) {
  // The window selector controller may be nullptr on shutdown.
  WindowSelectorController* controller =
      Shell::Get()->window_selector_controller();
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
  // window selector controller which has longer lifetime so that animations can
  // continue even after the overview mode is shut down.
  views::Widget* widget_ptr = widget.get();
  auto observer = std::make_unique<CleanupAnimationObserver>(std::move(widget));
  animation_settings.AddObserver(observer.get());
  controller->AddDelayedAnimationObserver(std::move(observer));
  widget_ptr->SetOpacity(0.f);
  if (slide) {
    gfx::Transform new_transform = widget_ptr->GetNativeWindow()->transform();
    new_transform.ConcatTransform(GetShiftTransform());
    widget_ptr->GetNativeWindow()->SetTransform(new_transform);
  }
}

std::unique_ptr<views::Widget> CreateBackgroundWidget(aura::Window* root_window,
                                                      ui::LayerType layer_type,
                                                      SkColor background_color,
                                                      int border_thickness,
                                                      int border_radius,
                                                      SkColor border_color,
                                                      float initial_opacity,
                                                      aura::Window* parent,
                                                      bool stack_on_top) {
  std::unique_ptr<views::Widget> widget = std::make_unique<views::Widget>();
  views::Widget::InitParams params;
  params.type = views::Widget::InitParams::TYPE_POPUP;
  params.keep_on_top = false;
  params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params.opacity = views::Widget::InitParams::TRANSLUCENT_WINDOW;
  params.layer_type = layer_type;
  params.accept_events = false;
  widget->set_focus_on_creation(false);
  // Parenting in kShellWindowId_WallpaperContainer allows proper layering of
  // the shield and selection widgets. Since that container is created with
  // USE_LOCAL_COORDINATES BoundsInScreenBehavior local bounds in |root_window_|
  // need to be provided.
  params.parent =
      parent ? parent
             : root_window->GetChildById(kShellWindowId_WallpaperContainer);
  widget->Init(params);
  aura::Window* widget_window = widget->GetNativeWindow();
  // Disable the "bounce in" animation when showing the window.
  ::wm::SetWindowVisibilityAnimationTransition(widget_window,
                                               ::wm::ANIMATE_NONE);
  // The background widget should not activate the shelf when passing under it.
  wm::GetWindowState(widget_window)->set_ignored_by_shelf(true);
  if (params.layer_type == ui::LAYER_SOLID_COLOR) {
    widget_window->layer()->SetColor(background_color);
  } else if (params.layer_type == ui::LAYER_TEXTURED) {
    views::View* content_view = new views::View();
    content_view->SetBackground(std::make_unique<BackgroundWith1PxBorder>(
        background_color, border_color, border_thickness, border_radius));
    widget->SetContentsView(content_view);
  }

  if (stack_on_top)
    widget_window->parent()->StackChildAtTop(widget_window);
  else
    widget_window->parent()->StackChildAtBottom(widget_window);

  widget->Show();
  widget_window->layer()->SetOpacity(initial_opacity);
  return widget;
}

gfx::Rect GetTransformedBounds(aura::Window* transformed_window,
                               int top_inset) {
  gfx::Rect bounds;
  for (auto* window : wm::GetTransientTreeIterator(transformed_window)) {
    // Ignore other window types when computing bounding box of window
    // selector target item.
    if (window != transformed_window &&
        (window->type() != aura::client::WINDOW_TYPE_NORMAL &&
         window->type() != aura::client::WINDOW_TYPE_PANEL)) {
      continue;
    }
    gfx::RectF window_bounds(window->GetTargetBounds());
    gfx::Transform new_transform =
        TransformAboutPivot(gfx::Point(window_bounds.x(), window_bounds.y()),
                            window->layer()->GetTargetTransform());
    new_transform.TransformRect(&window_bounds);

    // The preview title is shown above the preview window. Hide the window
    // header for apps or browser windows with no tabs (web apps) to avoid
    // showing both the window header and the preview title.
    if (top_inset > 0) {
      gfx::RectF header_bounds(window_bounds);
      header_bounds.set_height(top_inset);
      new_transform.TransformRect(&header_bounds);
      window_bounds.Inset(0, gfx::ToCeiledInt(header_bounds.height()), 0, 0);
    }
    gfx::Rect enclosing_bounds = ToEnclosingRect(window_bounds);
    ::wm::ConvertRectToScreen(window->parent(), &enclosing_bounds);
    bounds.Union(enclosing_bounds);
  }
  return bounds;
}

gfx::Rect GetTargetBoundsInScreen(aura::Window* window) {
  gfx::Rect bounds;
  for (auto* window_iter : wm::GetTransientTreeIterator(window)) {
    // Ignore other window types when computing bounding box of window
    // selector target item.
    if (window_iter != window &&
        window_iter->type() != aura::client::WINDOW_TYPE_NORMAL &&
        window_iter->type() != aura::client::WINDOW_TYPE_PANEL) {
      continue;
    }
    gfx::Rect target_bounds = window_iter->GetTargetBounds();
    ::wm::ConvertRectToScreen(window_iter->parent(), &target_bounds);
    bounds.Union(target_bounds);
  }
  return bounds;
}

void SetTransform(aura::Window* window, const gfx::Transform& transform) {
  gfx::Point target_origin(GetTargetBoundsInScreen(window).origin());
  for (auto* window_iter : wm::GetTransientTreeIterator(window)) {
    aura::Window* parent_window = window_iter->parent();
    gfx::Rect original_bounds(window_iter->GetTargetBounds());
    ::wm::ConvertRectToScreen(parent_window, &original_bounds);
    gfx::Transform new_transform =
        TransformAboutPivot(gfx::Point(target_origin.x() - original_bounds.x(),
                                       target_origin.y() - original_bounds.y()),
                            transform);
    window_iter->SetTransform(new_transform);
  }
}

bool IsSlidingOutOverviewFromShelf() {
  if (!Shell::Get()->window_selector_controller()->IsSelecting())
    return false;

  HomeLauncherGestureHandler* home_launcher_gesture_handler =
      Shell::Get()->app_list_controller()->home_launcher_gesture_handler();
  if (home_launcher_gesture_handler &&
      home_launcher_gesture_handler->mode() ==
          HomeLauncherGestureHandler::Mode::kSlideUpToShow) {
    return true;
  }

  return false;
}

}  // namespace ash
