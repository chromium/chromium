// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wallpaper/wallpaper_view.h"

#include "ash/public/cpp/window_animation_types.h"
#include "ash/root_window_controller.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/wallpaper/wallpaper_controller_impl.h"
#include "ash/wallpaper/wallpaper_widget_controller.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "cc/paint/render_surface_filters.h"
#include "ui/aura/window.h"
#include "ui/display/display.h"
#include "ui/display/manager/display_manager.h"
#include "ui/display/manager/managed_display_info.h"
#include "ui/display/screen.h"
#include "ui/gfx/canvas.h"
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
  LayerControlView(views::View* view, bool needs_shield) {
    if (needs_shield) {
      auto* shield = new views::View();
      AddChildView(shield);
      shield->SetPaintToLayer(ui::LAYER_SOLID_COLOR);
      shield->layer()->SetColor(SK_ColorBLACK);
      shield->layer()->set_name("WallpaperViewShield");
    }
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

    for (auto* child : children()) {
      // views::View* child = children().front();
      child->SetBounds(0, 0, display.size().width(), display.size().height());
      gfx::Transform transform;
      // Apply RTL transform explicitly becacuse Views layer code
      // doesn't handle RTL.  crbug.com/458753.
      transform.Translate(-child->GetMirroredX(), 0);
      child->SetTransform(transform);
    }
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(LayerControlView);
};

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// WallpaperView, public:

WallpaperView::WallpaperView(int blur, float opacity)
    : repaint_blur_(blur), repaint_opacity_(opacity) {
  set_context_menu_controller(this);
}

WallpaperView::~WallpaperView() = default;

void WallpaperView::RepaintBlurAndOpacity(int repaint_blur,
                                          float repaint_opacity) {
  if (repaint_blur_ == repaint_blur && repaint_opacity_ == repaint_opacity)
    return;

  repaint_blur_ = repaint_blur;
  repaint_opacity_ = repaint_opacity;
  SchedulePaint();
}

const char* WallpaperView::GetClassName() const {
  return "WallpaperView";
}

bool WallpaperView::OnMousePressed(const ui::MouseEvent& event) {
  return true;
}

void WallpaperView::ShowContextMenuForViewImpl(views::View* source,
                                               const gfx::Point& point,
                                               ui::MenuSourceType source_type) {
  Shell::Get()->ShowContextMenu(point, source_type);
}

void WallpaperView::DrawWallpaper(const gfx::ImageSkia& wallpaper,
                                  const gfx::Rect& src,
                                  const gfx::Rect& dst,
                                  const cc::PaintFlags& flags,
                                  gfx::Canvas* canvas) {
  // The amount we downsample the original image by before applying filters to
  // improve performance.
  constexpr float quality = 0.3f;
  gfx::Rect quality_adjusted_rect = gfx::ScaleToEnclosingRect(dst, quality);
  // Draw the wallpaper to a cached image the first time it is drawn or if the
  // size has changed.
  if (!small_image_ || small_image_->size() != quality_adjusted_rect.size()) {
    gfx::Canvas small_canvas(quality_adjusted_rect.size(),
                             /*image_scale=*/1.f,
                             /*is_opaque=*/false);
    small_canvas.DrawImageInt(wallpaper, src.x(), src.y(), src.width(),
                              src.height(), 0, 0, quality_adjusted_rect.width(),
                              quality_adjusted_rect.height(), true);
    small_image_ = base::make_optional(
        gfx::ImageSkia::CreateFrom1xBitmap(small_canvas.GetBitmap()));
  }

  if (repaint_blur_ == 0 && repaint_opacity_ == 1.f) {
    canvas->DrawImageInt(wallpaper, src.x(), src.y(), src.width(), src.height(),
                         dst.x(), dst.y(), dst.width(), dst.height(),
                         /*filter=*/true, flags);
    return;
  }
  bool will_not_fill = width() > dst.width() || height() > dst.height();
  // When not filling the view, we paint the small_image_ directly to the
  // canvas.
  float blur = will_not_fill ? repaint_blur_ : repaint_blur_ * quality;

  // Create the blur and brightness filter to apply to the downsampled image.
  cc::FilterOperations operations;
  // In tablet mode, the wallpaper already has a color filter applied in
  // |OnPaint| so we don't need to darken here.
  // TODO(crbug.com/944152): Merge this with the color filter in
  // WallpaperBaseView.
  if (!Shell::Get()->tablet_mode_controller()->InTabletMode()) {
    operations.Append(
        cc::FilterOperation::CreateBrightnessFilter(repaint_opacity_));
  }

  operations.Append(cc::FilterOperation::CreateBlurFilter(
      blur, SkBlurImageFilter::kClamp_TileMode));
  sk_sp<cc::PaintFilter> filter = cc::RenderSurfaceFilters::BuildImageFilter(
      operations, gfx::SizeF(dst.size()), gfx::Vector2dF());

  // If the wallpaper can't fill the desktop, paint it directly to the
  // canvas so that it can blend the image with the rest of background
  // correctly.
  if (blur > 0 && will_not_fill) {
    cc::PaintFlags filter_flags(flags);
    filter_flags.setImageFilter(filter);
    canvas->DrawImageInt(*small_image_, 0, 0, small_image_->width(),
                         small_image_->height(), dst.x(), dst.y(), dst.width(),
                         dst.height(),
                         /*filter=*/true, filter_flags);
    return;
  }
  cc::PaintFlags filter_flags;
  filter_flags.setImageFilter(filter);

  gfx::Canvas filtered_canvas(small_image_->size(),
                              /*image_scale=*/1.f,
                              /*is_opaque=*/false);
  filtered_canvas.sk_canvas()->saveLayer(nullptr, &filter_flags);
  filtered_canvas.DrawImageInt(
      *small_image_, 0, 0, small_image_->width(), small_image_->height(), 0, 0,
      small_image_->width(), small_image_->height(), true);
  filtered_canvas.sk_canvas()->restore();

  // Draw the downsampled and filtered image onto |canvas|. Draw a inseted
  // version of the image to avoid drawing a blackish border caused by the blur
  // filter. This is what we do on the login screen as well.
  gfx::ImageSkia filtered_wallpaper =
      gfx::ImageSkia::CreateFrom1xBitmap(filtered_canvas.GetBitmap());
  canvas->DrawImageInt(filtered_wallpaper, blur, blur,
                       small_image_->width() - 2 * blur,
                       small_image_->height() - 2 * blur, dst.x(), dst.y(),
                       dst.width(), dst.height(),
                       /*filter=*/true, flags);
}

views::Widget* CreateWallpaperWidget(aura::Window* root_window,
                                     int container_id,
                                     int blur,
                                     float opacity,
                                     WallpaperView** out_wallpaper_view) {
  auto* controller = Shell::Get()->wallpaper_controller();

  views::Widget* wallpaper_widget = new views::Widget;
  views::Widget::InitParams params(
      views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  params.name = "WallpaperViewWidget";
  params.layer_type = ui::LAYER_NOT_DRAWN;
  if (controller->GetWallpaper().isNull())
    params.opacity = views::Widget::InitParams::TRANSLUCENT_WINDOW;
  params.parent = root_window->GetChildById(container_id);
  wallpaper_widget->Init(std::move(params));
  // Owned by views.
  WallpaperView* wallpaper_view = new WallpaperView(blur, opacity);
  wallpaper_widget->SetContentsView(new LayerControlView(
      wallpaper_view,
      Shell::Get()->session_controller()->IsUserSessionBlocked()));
  *out_wallpaper_view = wallpaper_view;
  int animation_type =
      controller->ShouldShowInitialAnimation()
          ? WINDOW_VISIBILITY_ANIMATION_TYPE_BRIGHTNESS_GRAYSCALE
          : wm::WINDOW_VISIBILITY_ANIMATION_TYPE_FADE;
  aura::Window* wallpaper_window = wallpaper_widget->GetNativeWindow();
  ::wm::SetWindowVisibilityAnimationType(wallpaper_window, animation_type);

  // Enable wallpaper transition for the following cases:
  // 1. Initial wallpaper animation after device boot.
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
