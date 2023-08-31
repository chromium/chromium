// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wallpaper/views/wallpaper_view.h"

#include "ash/public/cpp/shell_window_ids.h"
#include "ash/public/cpp/window_animation_types.h"
#include "ash/root_window_controller.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/wallpaper/views/wallpaper_widget_controller.h"
#include "ash/wallpaper/wallpaper_controller_impl.h"
#include "ash/wallpaper/wallpaper_drag_drop_delegate.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "cc/paint/render_surface_filters.h"
#include "ui/aura/window.h"
#include "ui/base/dragdrop/drag_drop_types.h"
#include "ui/base/dragdrop/mojom/drag_drop_types.mojom.h"
#include "ui/compositor/layer.h"
#include "ui/display/display.h"
#include "ui/display/manager/display_manager.h"
#include "ui/display/manager/managed_display_info.h"
#include "ui/display/screen.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/size_conversions.h"
#include "ui/gfx/geometry/transform.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"
#include "ui/wm/core/window_animations.h"

namespace ash {

namespace {

// Returns the delegate, owned by the `WallpaperControllerImpl`, for
// drag-and-drop events over the wallpaper. May be `nullptr` if drag-and-drop
// related features are disabled.
WallpaperDragDropDelegate* GetDragDropDelegate() {
  auto* controller = Shell::Get()->wallpaper_controller();
  return controller ? controller->GetDragDropDelegate() : nullptr;
}

// A view that controls the child view's layer so that the layer always has the
// same size as the display's original, un-scaled size in DIP. The layer is then
// transformed to fit to the virtual screen size when laid-out. This is to avoid
// scaling the image at painting time, then scaling it back to the screen size
// in the compositor.
class WallpaperWidgetDelegate : public views::WidgetDelegateView {
 public:
  explicit WallpaperWidgetDelegate(views::View* view) {
    SetCanMaximize(true);
    SetCanFullscreen(true);
    AddChildView(view);
    view->SetPaintToLayer();
  }

  WallpaperWidgetDelegate(const WallpaperWidgetDelegate&) = delete;
  WallpaperWidgetDelegate& operator=(const WallpaperWidgetDelegate&) = delete;

  // Overrides views::View.
  void Layout() override {
    aura::Window* window = GetWidget()->GetNativeWindow();
    // Keep |this| at the bottom since there may be other windows on top of the
    // wallpaper view such as an overview mode shield.
    window->parent()->StackChildAtBottom(window);
    display::Display display =
        display::Screen::GetScreen()->GetDisplayNearestWindow(window);

    for (auto* child : children()) {
      child->SetBounds(0, 0, display.size().width(), display.size().height());
      gfx::Transform transform;
      // Apply RTL transform explicitly becacuse Views layer code
      // doesn't handle RTL.  crbug.com/458753.
      transform.Translate(-child->GetMirroredX(), 0);
      child->SetTransform(transform);
    }
  }
};

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// WallpaperView, public:

WallpaperView::WallpaperView(float blur_sigma) : blur_sigma_(blur_sigma) {
  set_context_menu_controller(this);
}

WallpaperView::~WallpaperView() = default;

void WallpaperView::ClearCachedImage() {
  small_image_.reset();
}

void WallpaperView::SetLockShieldEnabled(bool enabled) {
  if (enabled == !!shield_view_) {
    return;
  }

  if (enabled) {
    DCHECK(!shield_view_);
    shield_view_ = new views::View();
    parent()->AddChildViewAt(shield_view_.get(), 0);
    shield_view_->SetPaintToLayer(ui::LAYER_SOLID_COLOR);
    shield_view_->layer()->SetColor(SK_ColorBLACK);
    shield_view_->layer()->SetName("WallpaperViewShield");
    shield_view_->SetBoundsRect(parent()->GetLocalBounds());
  } else {
    DCHECK(shield_view_);
    parent()->RemoveChildViewT(shield_view_.get());
    shield_view_ = nullptr;
  }
}

const char* WallpaperView::GetClassName() const {
  return "WallpaperView";
}

bool WallpaperView::OnMousePressed(const ui::MouseEvent& event) {
  return true;
}

void WallpaperView::OnBoundsChanged(const gfx::Rect& previous_bounds) {
  if (shield_view_) {
    shield_view_->SetBoundsRect(parent()->GetLocalBounds());
  }
}

bool WallpaperView::AreDropTypesRequired() {
  return true;
}

bool WallpaperView::CanDrop(const ui::OSExchangeData& data) {
  if (auto* drag_drop_delegate = GetDragDropDelegate()) {
    return drag_drop_delegate->CanDrop(data);
  }
  return false;
}

views::View::DropCallback WallpaperView::GetDropCallback(
    const ui::DropTargetEvent& event) {
  const gfx::Point location_in_screen =
      views::View::ConvertPointToScreen(this, event.location());
  return base::BindOnce(
      [](const gfx::Point& location_in_screen, const ui::DropTargetEvent& event,
         ui::mojom::DragOperation& output_drag_op,
         std::unique_ptr<ui::LayerTreeOwner> drag_image_layer_owner) {
        if (auto* drag_drop_delegate = GetDragDropDelegate()) {
          output_drag_op =
              drag_drop_delegate->OnDrop(event.data(), location_in_screen);
        }
      },
      location_in_screen);
}

bool WallpaperView::GetDropFormats(int* formats,
                                   std::set<ui::ClipboardFormatType>* types) {
  if (auto* drag_drop_delegate = GetDragDropDelegate()) {
    drag_drop_delegate->GetDropFormats(formats, types);
  }
  return *formats || types->size();
}

void WallpaperView::OnDragEntered(const ui::DropTargetEvent& event) {
  if (auto* drag_drop_delegate = GetDragDropDelegate()) {
    drag_drop_delegate->OnDragEntered(
        event.data(),
        views::View::ConvertPointToScreen(this, event.location()));
  }
}

int WallpaperView::OnDragUpdated(const ui::DropTargetEvent& event) {
  if (auto* drag_drop_delegate = GetDragDropDelegate()) {
    return drag_drop_delegate->OnDragUpdated(
        event.data(),
        views::View::ConvertPointToScreen(this, event.location()));
  }
  return ui::DragDropTypes::DRAG_NONE;
}

void WallpaperView::OnDragExited() {
  if (auto* drag_drop_delegate = GetDragDropDelegate()) {
    drag_drop_delegate->OnDragExited();
  }
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
    small_image_ = absl::make_optional(
        gfx::ImageSkia::CreateFrom1xBitmap(small_canvas.GetBitmap()));
  }

  if (blur_sigma_ == wallpaper_constants::kClear) {
    canvas->DrawImageInt(wallpaper, src.x(), src.y(), src.width(), src.height(),
                         dst.x(), dst.y(), dst.width(), dst.height(),
                         /*filter=*/true, flags);
    return;
  }
  bool will_not_fill = width() > dst.width() || height() > dst.height();
  // When not filling the view, we paint the small_image_ directly to the
  // canvas.
  float blur = will_not_fill ? blur_sigma_ : blur_sigma_ * quality;

  // Create the blur and brightness filter to apply to the downsampled image.
  cc::FilterOperations operations;
  operations.Append(
      cc::FilterOperation::CreateBlurFilter(blur, SkTileMode::kClamp));
  sk_sp<cc::PaintFilter> filter =
      cc::RenderSurfaceFilters::BuildImageFilter(operations);

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
  filtered_canvas.sk_canvas()->saveLayer(filter_flags);
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

std::unique_ptr<views::Widget> CreateWallpaperWidget(
    aura::Window* root_window,
    float blur_sigma,
    bool locked,
    WallpaperView** out_wallpaper_view) {
  int container_id = locked ? kShellWindowId_LockScreenWallpaperContainer
                            : kShellWindowId_WallpaperContainer;
  auto* controller = Shell::Get()->wallpaper_controller();

  auto wallpaper_widget = std::make_unique<views::Widget>();
  views::Widget::InitParams params(
      views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  params.name = "WallpaperViewWidget";
  params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params.layer_type = ui::LAYER_NOT_DRAWN;
  params.parent = root_window->GetChildById(container_id);
  WallpaperView* wallpaper_view = new WallpaperView(blur_sigma);
  params.delegate = new WallpaperWidgetDelegate(wallpaper_view);

  wallpaper_widget->Init(std::move(params));
  // Owned by views.
  *out_wallpaper_view = wallpaper_view;
  int animation_type =
      controller->ShouldShowInitialAnimation()
          ? static_cast<int>(
                WINDOW_VISIBILITY_ANIMATION_TYPE_BRIGHTNESS_GRAYSCALE)
          : static_cast<int>(wm::WINDOW_VISIBILITY_ANIMATION_TYPE_FADE);
  aura::Window* wallpaper_window = wallpaper_widget->GetNativeWindow();
  ::wm::SetWindowVisibilityAnimationType(wallpaper_window, animation_type);

  // Enable wallpaper transition for the following cases:
  // 1. Initial wallpaper animation after device boot.
  // 2. Wallpaper fades in from a non empty background.
  // 3. From an empty background, chrome transit to a logged in user session.
  // 4. From an empty background, guest user logged in.
  // except for the lock state.
  if (!locked &&
      (controller->ShouldShowInitialAnimation() ||
       RootWindowController::ForWindow(root_window)
           ->wallpaper_widget_controller()
           ->IsAnimating() ||
       Shell::Get()->session_controller()->NumberOfLoggedInUsers())) {
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

  return wallpaper_widget;
}

}  // namespace ash
