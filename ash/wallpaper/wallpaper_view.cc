// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wallpaper/wallpaper_view.h"

#include "ash/public/cpp/login_constants.h"
#include "ash/public/cpp/wallpaper_types.h"
#include "ash/public/cpp/window_animation_types.h"
#include "ash/root_window_controller.h"
#include "ash/session/session_controller.h"
#include "ash/shell.h"
#include "ash/wallpaper/wallpaper_controller.h"
#include "ash/wallpaper/wallpaper_widget_controller.h"
#include "ash/wm/overview/overview_utils.h"
#include "ash/wm/overview/window_selector_controller.h"
#include "ui/aura/window.h"
#include "ui/display/display.h"
#include "ui/display/manager/display_manager.h"
#include "ui/display/manager/managed_display_info.h"
#include "ui/display/screen.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_analysis.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/geometry/safe_integer_conversions.h"
#include "ui/gfx/geometry/size_conversions.h"
#include "ui/gfx/transform.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/core/window_animations.h"

namespace ash {
namespace {

// A view that controls the child view's layer so that the layer always has the
// same size as the display's original, un-scaled size in DIP. The layer is then
// transformed to fit to the virtual screen size when laid-out. This is to avoid
// scaling the image at painting time, then scaling it back to the screen size
// in the compositor.
class LayerControlView : public views::View {
 public:
  explicit LayerControlView(views::View* view) {
    AddChildView(view);
    view->SetPaintToLayer();
  }

  // Overrides views::View.
  void Layout() override {
    aura::Window* window = GetWidget()->GetNativeWindow();
    // Keep |this| at the bottom since there may be other windows on top of the
    // wallpaper view such as an overview mode shield.
    window->parent()->StackChildAtBottom(window);
    display::Display display =
        display::Screen::GetScreen()->GetDisplayNearestWindow(window);

    display::ManagedDisplayInfo info =
        Shell::Get()->display_manager()->GetDisplayInfo(display.id());

    DCHECK_EQ(1, child_count());
    views::View* child = child_at(0);
    child->SetBounds(0, 0, display.size().width(), display.size().height());
    gfx::Transform transform;
    // Apply RTL transform explicitly becacuse Views layer code
    // doesn't handle RTL.  crbug.com/458753.
    transform.Translate(-child->GetMirroredX(), 0);
    child->SetTransform(transform);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(LayerControlView);
};

// Returns the color used to dim the wallpaper.
SkColor GetWallpaperDarkenColor() {
  SkColor darken_color =
      Shell::Get()->wallpaper_controller()->GetProminentColor(
          color_utils::ColorProfile(color_utils::LumaRange::DARK,
                                    color_utils::SaturationRange::MUTED));
  if (darken_color == kInvalidWallpaperColor)
    darken_color = login_constants::kDefaultBaseColor;

  darken_color = color_utils::GetResultingPaintColor(
      SkColorSetA(login_constants::kDefaultBaseColor,
                  login_constants::kTranslucentColorDarkenAlpha),
      SkColorSetA(darken_color, 0xFF));
  return SkColorSetA(darken_color, login_constants::kTranslucentAlpha);
}

}  // namespace

// This event handler receives events in the pre-target phase and takes care of
// the following:
//   - Disabling overview mode on touch release.
//   - Disabling overview mode on mouse release.
class PreEventDispatchHandler : public ui::EventHandler {
 public:
  PreEventDispatchHandler() = default;
  ~PreEventDispatchHandler() override = default;

 private:
  // ui::EventHandler:
  void OnMouseEvent(ui::MouseEvent* event) override {
    if (event->type() == ui::ET_MOUSE_RELEASED)
      HandleClickOrTap(event);
  }

  void OnGestureEvent(ui::GestureEvent* event) override {
    if (event->type() == ui::ET_GESTURE_TAP)
      HandleClickOrTap(event);
  }

  void HandleClickOrTap(ui::Event* event) {
    CHECK_EQ(ui::EP_PRETARGET, event->phase());
    WindowSelectorController* controller =
        Shell::Get()->window_selector_controller();
    if (!controller->IsSelecting())
      return;
    // Events that happen while app list is sliding out during overview should
    // be ignored to prevent overview from disappearing out from under the user.
    if (!IsSlidingOutOverviewFromShelf())
      controller->ToggleOverview();
    event->StopPropagation();
  }

  DISALLOW_COPY_AND_ASSIGN(PreEventDispatchHandler);
};

////////////////////////////////////////////////////////////////////////////////
// WallpaperView, public:

WallpaperView::WallpaperView()
    : pre_dispatch_handler_(new PreEventDispatchHandler()) {
  set_context_menu_controller(this);
  AddPreTargetHandler(pre_dispatch_handler_.get());
}

WallpaperView::~WallpaperView() {
  RemovePreTargetHandler(pre_dispatch_handler_.get());
}

////////////////////////////////////////////////////////////////////////////////
// WallpaperView, views::View overrides:

void WallpaperView::OnPaint(gfx::Canvas* canvas) {
  // Scale the image while maintaining the aspect ratio, cropping as necessary
  // to fill the wallpaper. Ideally the image should be larger than the largest
  // display supported, if not we will scale and center it if the layout is
  // WALLPAPER_LAYOUT_CENTER_CROPPED.
  WallpaperController* controller = Shell::Get()->wallpaper_controller();
  gfx::ImageSkia wallpaper = controller->GetWallpaper();
  WallpaperLayout layout = controller->GetWallpaperLayout();

  // Wallpapers with png format could be partially transparent. Fill the canvas
  // with black to make it opaque before painting the wallpaper.
  canvas->FillRect(GetLocalBounds(), SK_ColorBLACK);

  if (wallpaper.isNull())
    return;

  cc::PaintFlags flags;
  if (controller->ShouldApplyDimming()) {
    flags.setColorFilter(SkColorFilter::MakeModeFilter(
        GetWallpaperDarkenColor(), SkBlendMode::kDarken));
  }

  switch (layout) {
    case WALLPAPER_LAYOUT_CENTER_CROPPED: {
      // The dimension with the smallest ratio must be cropped, the other one
      // is preserved. Both are set in gfx::Size cropped_size.
      double horizontal_ratio =
          static_cast<double>(width()) / static_cast<double>(wallpaper.width());
      double vertical_ratio = static_cast<double>(height()) /
                              static_cast<double>(wallpaper.height());

      gfx::Size cropped_size;
      if (vertical_ratio > horizontal_ratio) {
        cropped_size = gfx::Size(
            gfx::ToFlooredInt(static_cast<double>(width()) / vertical_ratio),
            wallpaper.height());
      } else {
        cropped_size = gfx::Size(
            wallpaper.width(), gfx::ToFlooredInt(static_cast<double>(height()) /
                                                 horizontal_ratio));
      }

      gfx::Rect wallpaper_cropped_rect(0, 0, wallpaper.width(),
                                       wallpaper.height());
      wallpaper_cropped_rect.ClampToCenteredSize(cropped_size);
      canvas->DrawImageInt(
          wallpaper, wallpaper_cropped_rect.x(), wallpaper_cropped_rect.y(),
          wallpaper_cropped_rect.width(), wallpaper_cropped_rect.height(), 0, 0,
          width(), height(), true, flags);
      break;
    }
    case WALLPAPER_LAYOUT_TILE: {
      canvas->TileImageInt(wallpaper, 0, 0, 0, 0, width(), height(), 1.0f,
                           SkShader::kRepeat_TileMode,
                           SkShader::kRepeat_TileMode, &flags);
      break;
    }
    case WALLPAPER_LAYOUT_STRETCH: {
      // This is generally not recommended as it may show artifacts.
      canvas->DrawImageInt(wallpaper, 0, 0, wallpaper.width(),
                           wallpaper.height(), 0, 0, width(), height(), true,
                           flags);
      break;
    }
    case WALLPAPER_LAYOUT_CENTER: {
      float image_scale = canvas->image_scale();
      gfx::Rect wallpaper_rect(0, 0, wallpaper.width() / image_scale,
                               wallpaper.height() / image_scale);
      // Simply centered and not scaled (but may be clipped).
      canvas->DrawImageInt(
          wallpaper, 0, 0, wallpaper.width(), wallpaper.height(),
          (width() - wallpaper_rect.width()) / 2,
          (height() - wallpaper_rect.height()) / 2, wallpaper_rect.width(),
          wallpaper_rect.height(), true, flags);
      break;
    }
    default: {
      NOTREACHED();
      break;
    }
  }
}

bool WallpaperView::OnMousePressed(const ui::MouseEvent& event) {
  return true;
}

void WallpaperView::ShowContextMenuForView(views::View* source,
                                           const gfx::Point& point,
                                           ui::MenuSourceType source_type) {
  Shell::Get()->ShowContextMenu(point, source_type);
}

views::Widget* CreateWallpaperWidget(aura::Window* root_window,
                                     int container_id) {
  WallpaperController* controller = Shell::Get()->wallpaper_controller();

  views::Widget* wallpaper_widget = new views::Widget;
  views::Widget::InitParams params(
      views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  params.name = "WallpaperView";
  if (controller->GetWallpaper().isNull())
    params.opacity = views::Widget::InitParams::TRANSLUCENT_WINDOW;
  params.parent = root_window->GetChildById(container_id);
  wallpaper_widget->Init(params);
  wallpaper_widget->SetContentsView(new LayerControlView(new WallpaperView()));
  int animation_type =
      controller->ShouldShowInitialAnimation()
          ? wm::WINDOW_VISIBILITY_ANIMATION_TYPE_BRIGHTNESS_GRAYSCALE
          : ::wm::WINDOW_VISIBILITY_ANIMATION_TYPE_FADE;
  aura::Window* wallpaper_window = wallpaper_widget->GetNativeWindow();
  ::wm::SetWindowVisibilityAnimationType(wallpaper_window, animation_type);

  // Enable wallpaper transition for the following cases:
  // 1. Initial(OOBE) wallpaper animation.
  // 2. Wallpaper fades in from a non empty background.
  // 3. From an empty background, chrome transit to a logged in user session.
  // 4. From an empty background, guest user logged in.
  if (controller->ShouldShowInitialAnimation() ||
      RootWindowController::ForWindow(root_window)
          ->wallpaper_widget_controller()
          ->IsAnimating() ||
      Shell::Get()->session_controller()->NumberOfLoggedInUsers()) {
    ::wm::SetWindowVisibilityAnimationTransition(wallpaper_window,
                                                 ::wm::ANIMATE_SHOW);
    base::TimeDelta animation_duration = controller->animation_duration();
    if (!animation_duration.is_zero()) {
      ::wm::SetWindowVisibilityAnimationDuration(wallpaper_window,
                                                 animation_duration);
    }
  } else {
    // Disable animation if transition to login screen from an empty background.
    ::wm::SetWindowVisibilityAnimationTransition(wallpaper_window,
                                                 ::wm::ANIMATE_NONE);
  }

  aura::Window* container = root_window->GetChildById(container_id);
  wallpaper_widget->SetBounds(container->bounds());

  return wallpaper_widget;
}

}  // namespace ash
