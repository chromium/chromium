// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/display/cursor_window_controller.h"

#include <optional>

#include "ash/accessibility/magnifier/fullscreen_magnifier_controller.h"
#include "ash/capture_mode/capture_mode_camera_controller.h"
#include "ash/capture_mode/capture_mode_controller.h"
#include "ash/capture_mode/capture_mode_session.h"
#include "ash/constants/ash_constants.h"
#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/constants/ash_switches.h"
#include "ash/curtain/security_curtain_controller.h"
#include "ash/display/display_color_manager.h"
#include "ash/fast_ink/cursor/cursor_view.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/root_window_controller.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "base/command_line.h"
#include "base/metrics/histogram_macros.h"
#include "components/prefs/pref_service.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/accessibility/accessibility_features.h"
#include "ui/aura/env.h"
#include "ui/aura/window.h"
#include "ui/aura/window_delegate.h"
#include "ui/base/hit_test.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/resource/resource_scale_factor.h"
#include "ui/compositor/paint_recorder.h"
#include "ui/display/display.h"
#include "ui/display/manager/display_manager.h"
#include "ui/display/screen.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/dip_util.h"
#include "ui/gfx/geometry/point_conversions.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/gfx/image/image_skia_source.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_utils.h"
#include "ui/wm/core/coordinate_conversion.h"
#include "ui/wm/core/cursor_util.h"

namespace ash {

namespace {

std::vector<gfx::ImageSkia> GetCursorImages(
    ui::CursorSize cursor_size,
    ui::mojom::CursorType type,
    int target_cursor_size_in_dip,
    float dsf,
    SkColor cursor_color,
    gfx::Point* out_hotspot_in_physical_pixels) {
  std::vector<gfx::ImageSkia> images;
  // Rotation is handled in viz (for aura::Window based cursor)
  // or fast ink canvas (for fast ink based cursor), so don't do any
  // rotation here.
  std::optional<ui::CursorData> cursor_data = wm::GetCursorData(
      type, dsf,
      cursor_size == ui::CursorSize::kLarge
          ? std::make_optional(target_cursor_size_in_dip * dsf)
          : std::nullopt,
      display::Display::ROTATE_0, cursor_color);
  if (!cursor_data) {
    return images;
  }
  *out_hotspot_in_physical_pixels = cursor_data->hotspot;
  for (const auto& bitmap : cursor_data->bitmaps) {
    images.push_back(gfx::ImageSkia::CreateFromBitmap(bitmap, dsf));
  }
  return images;
}

}  // namespace

class CursorWindowDelegate : public aura::WindowDelegate {
 public:
  CursorWindowDelegate() = default;

  CursorWindowDelegate(const CursorWindowDelegate&) = delete;
  CursorWindowDelegate& operator=(const CursorWindowDelegate&) = delete;

  ~CursorWindowDelegate() override = default;

  // aura::WindowDelegate overrides:
  gfx::Size GetMinimumSize() const override { return size_; }
  std::optional<gfx::Size> GetMaximumSize() const override {
    if (size_.IsZero()) {
      return std::nullopt;
    }
    return size_;
  }
  void OnBoundsChanged(const gfx::Rect& old_bounds,
                       const gfx::Rect& new_bounds) override {}
  gfx::NativeCursor GetCursor(const gfx::Point& point) override {
    return gfx::NativeCursor{};
  }
  int GetNonClientComponent(const gfx::Point& point) const override {
    return HTNOWHERE;
  }
  bool ShouldDescendIntoChildForEventHandling(
      aura::Window* child,
      const gfx::Point& location) override {
    return false;
  }
  bool CanFocus() override { return false; }
  void OnCaptureLost() override {}
  void OnPaint(const ui::PaintContext& context) override {
    // No need to cache the output here, the CursorWindow is not invalidated.
    ui::PaintRecorder recorder(context, size_);
    recorder.canvas()->DrawImageInt(cursor_images_[paint_image_index_], 0, 0);
  }
  void OnDeviceScaleFactorChanged(float old_device_scale_factor,
                                  float new_device_scale_factor) override {}
  void OnWindowDestroying(aura::Window* window) override {}
  void OnWindowDestroyed(aura::Window* window) override {}
  void OnWindowTargetVisibilityChanged(bool visible) override {}
  bool HasHitTestMask() const override { return false; }
  void GetHitTestMask(SkPath* mask) const override {}

  // Updates cursor animation. If cursor_images_ has more than 1 image,
  // start the timer to schedule frame painting.
  void UpdateAnimation() {
    paint_image_index_ = 0;
    if (!cursor_window_) {
      if (animated_cursor_timer_.IsRunning()) {
        animated_cursor_timer_.Stop();
      }
      return;
    }
    if (cursor_images_.size() == 1) {
      animated_cursor_timer_.Stop();
    } else if (cursor_images_.size() > 1) {
      animated_cursor_timer_.Start(
          FROM_HERE, base::Milliseconds(16),
          base::BindRepeating(&CursorWindowDelegate::AdvanceFrame,
                              base::Unretained(this)));
    }
  }

  // Schedules to paint the next frame.
  void AdvanceFrame() {
    paint_image_index_ = (paint_image_index_ + 1) % cursor_images_.size();
    cursor_window_->SchedulePaintInRect(gfx::Rect(size_));
  }

  // Sets the cursor image for the |display|'s scale factor.
  void SetCursorImage(const gfx::Size& size,
                      const std::vector<gfx::ImageSkia>& images) {
    size_ = size;
    cursor_images_ = images;
    UpdateAnimation();
  }

  void SetCursorWindow(aura::Window* window) {
    cursor_window_ = window;
    UpdateAnimation();
  }

  const gfx::Size& size() const { return size_; }
  const std::vector<gfx::ImageSkia>& cursor_images() const {
    return cursor_images_;
  }

 private:
  std::vector<gfx::ImageSkia> cursor_images_;
  int paint_image_index_ = 0;
  raw_ptr<aura::Window> cursor_window_;
  base::RepeatingTimer animated_cursor_timer_;
  gfx::Size size_;
};

CursorWindowController::CursorWindowController()
    : delegate_(new CursorWindowDelegate()) {}

CursorWindowController::~CursorWindowController() {
  SetContainer(NULL);
}

void CursorWindowController::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void CursorWindowController::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void CursorWindowController::SetLargeCursorSizeInDip(
    int large_cursor_size_in_dip) {
  large_cursor_size_in_dip =
      std::min(large_cursor_size_in_dip, kMaxLargeCursorSize);
  large_cursor_size_in_dip =
      std::max(large_cursor_size_in_dip, kMinLargeCursorSize);

  if (large_cursor_size_in_dip_ == large_cursor_size_in_dip)
    return;

  large_cursor_size_in_dip_ = large_cursor_size_in_dip;

  if (display_.is_valid())
    UpdateCursorImage();
}

void CursorWindowController::SetCursorColor(SkColor cursor_color) {
  if (cursor_color_ == cursor_color)
    return;

  cursor_color_ = cursor_color;

  // Reset cursor lottie animation cache when new color needs to be applied.
  wm::ClearCursorAnimationCache();

  if (display_.is_valid())
    UpdateCursorImage();
}

bool CursorWindowController::ShouldEnableCursorCompositing() {
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kForceShowCursor)) {
    return false;
  }

  auto* controller = CaptureModeController::Get();
  auto* session = controller->capture_mode_session();
  if (session && session->is_drag_in_progress()) {
    // To ensure the cursor is aligned with the dragged region.
    return true;
  }

  if (controller->camera_controller()->is_drag_in_progress()) {
    // To ensure the cursor is aligned with the dragged camera preview.
    return true;
  }

  Shell* shell = Shell::Get();
  if (shell->security_curtain_controller().IsEnabled()) {
    return true;
  }

  // During startup, we may not have a preference service yet. We need to check
  // display manager state first so that we don't accidentally ignore it while
  // early outing when there isn't a PrefService yet.
  display::DisplayManager* display_manager = shell->display_manager();
  if ((display_manager->IsInSoftwareMirrorMode()) ||
      display_manager->IsInUnifiedMode() ||
      display_manager->screen_capture_is_active()) {
    return true;
  }

  if (shell->fullscreen_magnifier_controller()->IsEnabled())
    return true;

  PrefService* prefs = shell->session_controller()->GetActivePrefService();
  if (!prefs) {
    // The active pref service can be null early in startup.
    return false;
  }

  if (prefs->GetBoolean(prefs::kNightLightEnabled)) {
    // If the current display doesn't support setting a CRTC matrix,
    // Night Light will be using the composited color matrix, and hence software
    // cursor should be used.
    if (!shell->display_color_manager()->HasColorCorrectionMatrix(
            display_.id())) {
      return true;
    }
  }

  if (prefs->GetBoolean(prefs::kAccessibilityLargeCursorEnabled)) {
    // If current display's cursor plane size (i.e. maximum cursor size) is
    // larger than the large cursor size, then large cursor can be drawn
    // using hardware plane. Otherwise, software compositing is needed
    // for cursor.
    if (gfx::ConvertSizeToDips(display_.maximum_cursor_size(),
                               display_.device_scale_factor())
            .height() < large_cursor_size_in_dip_) {
      return true;
    }
  }

  return prefs->GetBoolean(prefs::kAccessibilityHighContrastEnabled) ||
         prefs->GetBoolean(prefs::kDockedMagnifierEnabled);
}

void CursorWindowController::SetCursorCompositingEnabled(bool enabled) {
  if (is_cursor_compositing_enabled_ == enabled)
    return;

  is_cursor_compositing_enabled_ = enabled;
  if (display_.is_valid())
    UpdateCursorImage();
  UpdateContainer();
  for (auto& obs : observers_)
    obs.OnCursorCompositingStateChanged(is_cursor_compositing_enabled_);
}

void CursorWindowController::UpdateContainer() {
  if (is_cursor_compositing_enabled_) {
    display::Screen* screen = display::Screen::Get();
    display::Display display =
        screen->GetDisplayNearestPoint(screen->GetCursorScreenPoint());
    DCHECK(display.is_valid());
    if (display.is_valid())
      SetDisplay(display);
  } else {
    SetContainer(nullptr);
  }
  // Updates the hot point based on the current display.
  UpdateCursorImage();
}

void CursorWindowController::SetDisplay(const display::Display& display) {
  // TODO(oshima): Do not update the composition cursor when crossing
  // display in unified desktop mode for now. crbug.com/517222.
  if (Shell::Get()->display_manager()->IsInUnifiedMode() &&
      display.id() != display::kUnifiedDisplayId) {
    return;
  }

  display_ = display;

  Shell* shell = Shell::Get();
  auto& managed_display_info =
      shell->display_manager()->GetDisplayInfo(display_.id());
  max_update_rate_ms_ =
      (1000.f / managed_display_info.refresh_rate()) * 2.f / 3.f;

  // Make sure next mouse event will update the cursor.
  last_updated_.reset();

  aura::Window* root_window = Shell::GetRootWindowForDisplayId(display.id());
  if (!root_window)
    return;

  if (!is_cursor_compositing_enabled_) {
    return;
  }

  SetContainer(RootWindowController::ForWindow(root_window)
                   ->GetContainer(kShellWindowId_MouseCursorContainer));
  SetBoundsInScreenAndRotation(display.bounds(), display.rotation());
  // Updates the hot point based on the current display.
  UpdateCursorImage();
}

void CursorWindowController::OnDockedMagnifierResizingStateChanged(
    bool is_active) {
  if (!container_) {
    return;
  }
  const int container_id = is_active ? kShellWindowId_DockedMagnifierContainer
                                     : kShellWindowId_MouseCursorContainer;
  SetContainer(
      RootWindowController::ForWindow(container_)->GetContainer(container_id));
}

void CursorWindowController::OnFullscreenMagnifierEnabled(bool enabled) {
  UpdateCursorMode();
}

void CursorWindowController::UpdateLocation(bool throttle) {
  // Throttle cursor updates.
  if (throttle) {
    auto now = base::TimeTicks::Now();
    if (last_updated_ &&
        (now - *last_updated_).InMilliseconds() < max_update_rate_ms_) {
      return;
    }
    last_updated_ = now;
  }

  if (cursor_view_widget_) {
    gfx::Point cursor_location =
        aura::Env::GetInstance()->last_mouse_location();
    aura::Window* root_window = views::GetRootWindow(cursor_view_widget_.get());
    // Convert cursor location point in screen coordinate to root window
    // coordinate.
    wm::ConvertPointFromScreen(root_window, &cursor_location);
    static_cast<CursorView*>(cursor_view_widget_->GetContentsView())
        ->SetLocation(cursor_location);
    return;
  }
  if (cursor_window_) {
    gfx::Point point = aura::Env::GetInstance()->last_mouse_location();
    // Calculate the new origin.
    // new_origin.x() + bounds_in_screen_.x() + hot_point_.x() = x value of
    // the mouse location in the screen coordinates.
    point.Offset(-bounds_in_screen_.x(), -bounds_in_screen_.y());
    point.Offset(-hot_point_.x(), -hot_point_.y());
    gfx::Rect bounds = cursor_window_->bounds();
    bounds.set_origin(point);
    cursor_window_->SetBounds(bounds);
    return;
  }
}

void CursorWindowController::SetCursor(gfx::NativeCursor cursor) {
  if (cursor_ == cursor)
    return;
  cursor_ = cursor;
  UpdateCursorImage();
  UpdateCursorVisibility();
}

void CursorWindowController::SetCursorSize(ui::CursorSize cursor_size) {
  cursor_size_ = cursor_size;
  UpdateCursorImage();
}

void CursorWindowController::SetVisibility(bool visible) {
  visible_ = visible;
  UpdateCursorVisibility();
}

void CursorWindowController::OnWindowBoundsChanged(
    aura::Window* window,
    const gfx::Rect& old_bounds,
    const gfx::Rect& new_bounds,
    ui::PropertyChangeReason reason) {
  DCHECK_EQ(container_, window);

  if (cursor_view_widget_) {
    UpdateCursorView();
  }
}

void CursorWindowController::OnWindowDestroying(aura::Window* window) {
  DCHECK_EQ(container_, window);

  scoped_container_observer_.Reset();
}

const aura::Window* CursorWindowController::GetContainerForTest() const {
  return container_;
}

SkColor CursorWindowController::GetCursorColorForTest() const {
  return cursor_color_;
}

gfx::Rect CursorWindowController::GetCursorBoundsInScreenForTest() const {
  if (cursor_view_widget_) {
    gfx::Rect cursor_rect =
        static_cast<CursorView*>(cursor_view_widget_->GetContentsView())
            ->get_cursor_rect_for_test();  // IN-TEST
    // Convert cursor rect in root window to screen coordinate.
    wm::ConvertRectToScreen(views::GetRootWindow(cursor_view_widget_.get()),
                            &cursor_rect);
    return cursor_rect;
  }
  return cursor_window_ ? cursor_window_->GetBoundsInScreen() : gfx::Rect();
}

const aura::Window* CursorWindowController::GetCursorHostWindowForTest() const {
  if (cursor_view_widget_) {
    return cursor_view_widget_->GetNativeWindow();
  }
  return cursor_window_ ? cursor_window_.get() : nullptr;
}

void CursorWindowController::SetContainer(aura::Window* container) {
  if (container_ == container) {
    return;
  }

  scoped_container_observer_.Reset();

  container_ = container;
  if (!container) {
    delegate_->SetCursorWindow(nullptr);
    cursor_window_.reset();
    cursor_view_widget_.reset();
    return;
  }

  scoped_container_observer_.Observe(container_);

  bounds_in_screen_ = display_.bounds();
  rotation_ = display_.rotation();

  UpdateCursorMode();
  UpdateCursorVisibility();
  UpdateLocation();
}

void CursorWindowController::SetBoundsInScreenAndRotation(
    const gfx::Rect& bounds,
    display::Display::Rotation rotation) {
  if (bounds == bounds_in_screen_ && rotation == rotation_) {
    return;
  }
  bounds_in_screen_ = bounds;
  rotation_ = rotation;
  if (cursor_view_widget_) {
    UpdateCursorView();
  }
  UpdateLocation();
}

void CursorWindowController::UpdateCursorImage() {
  if (!is_cursor_compositing_enabled_) {
    return;
  }

  std::vector<gfx::ImageSkia> images;
  gfx::Point hot_point_in_physical_pixels;

  if (cursor_.type() == ui::mojom::CursorType::kCustom) {
    // Custom cursor.
    SkBitmap bitmap = cursor_.custom_bitmap();
    gfx::Point hotspot = cursor_.custom_hotspot();
    if (bitmap.isNull()) {
      return;
    }
    float cursor_scale = cursor_.image_scale_factor();

    // Custom cursor's bitmap is already rotated. Revert the rotation because
    // software cursor's rotation is handled by viz.
    // TODO(b/320398214): Custom cursor's scaling and rotation
    // should be handled in ash.
    const display::Display::Rotation inverted_rotation =
        static_cast<display::Display::Rotation>(
            (4 - static_cast<int>(display_.rotation())) % 4);
    wm::ScaleAndRotateCursorBitmapAndHotpoint(1.0f, inverted_rotation, &bitmap,
                                              &hotspot);
    images.push_back(gfx::ImageSkia::CreateFromBitmap(
        wm::GetColorAdjustedBitmap(bitmap, cursor_color_), cursor_scale));
    hot_point_in_physical_pixels = hotspot;

    // Use `gfx::ToFlooredPoint` as `ImageSkiaRep::GetWidth` is implemented as
    // `return static_cast<int>(pixel_width() / scale());`.
    hot_point_ = gfx::ToFlooredPoint(
        gfx::ConvertPointToDips(hot_point_in_physical_pixels, cursor_scale));

    // Rescale cursor size. This is used with the combination of accessibility
    // large cursor. We don't need to care about the case where cursor
    // compositing is disabled as we always use cursor compositing if
    // accessibility large cursor is enabled.
    if (cursor_size_ == ui::CursorSize::kLarge &&
        large_cursor_size_in_dip_ != images[0].size().height()) {
      const float rescale = static_cast<float>(large_cursor_size_in_dip_) /
                            static_cast<float>(images[0].size().height());
      hot_point_ = gfx::ScaleToCeiledPoint(hot_point_, rescale);
      for (size_t i = 0; i < images.size(); ++i) {
        images[i] = gfx::ImageSkiaOperations::CreateResizedImage(
            images[i], skia::ImageOperations::ResizeMethod::RESIZE_BEST,
            gfx::ScaleToCeiledSize(images[i].size(), rescale));
      }
    }
  } else {
    // Standard cursor.
    const float dsf = display_.device_scale_factor();

    images =
        GetCursorImages(cursor_size_, cursor_.type(), large_cursor_size_in_dip_,
                        dsf, cursor_color_, &hot_point_in_physical_pixels);
    if (images.empty()) {
      return;
    }
    hot_point_ = gfx::ToFlooredPoint(
        gfx::ConvertPointToDips(hot_point_in_physical_pixels, dsf));
  }

  delegate_->SetCursorImage(images[0].size(), images);

  if (cursor_view_widget_) {
    static_cast<CursorView*>(cursor_view_widget_->GetContentsView())
        ->SetCursorImages(delegate_->cursor_images(), delegate_->size(),
                          hot_point_);
  }
  if (cursor_window_) {
    cursor_window_->SetBounds(gfx::Rect(delegate_->size()));
    cursor_window_->SchedulePaintInRect(
        gfx::Rect(cursor_window_->bounds().size()));
  }
  UpdateLocation();
}

void CursorWindowController::UpdateCursorVisibility() {
  bool visible = (visible_ && cursor_.type() != ui::mojom::CursorType::kNone);
  if (visible) {
    if (cursor_view_widget_)
      cursor_view_widget_->Show();
    if (cursor_window_)
      cursor_window_->Show();
  } else {
    if (cursor_view_widget_)
      cursor_view_widget_->Hide();
    if (cursor_window_)
      cursor_window_->Hide();
  }
}

void CursorWindowController::UpdateCursorView() {
  // Return if there is existing |cursor_view_widget_| and the container's size
  // is not updated yet.
  if (cursor_view_widget_ &&
      container_->GetBoundsInRootWindow().size() != bounds_in_screen_.size()) {
    return;
  }

  cursor_view_widget_ = CursorView::Create(
      aura::Env::GetInstance()->last_mouse_location(), container_);
  UpdateCursorImage();
}

void CursorWindowController::UpdateCursorWindow() {
  delegate_->SetCursorWindow(nullptr);
  // Reusing the window does not work when the display is disconnected.
  // Just creates a new one instead. crbug.com/384218.
  cursor_window_ = std::make_unique<aura::Window>(delegate_.get());
  cursor_window_->Init(ui::LAYER_TEXTURED);
  cursor_window_->SetTransparent(true);
  cursor_window_->SetEventTargetingPolicy(aura::EventTargetingPolicy::kNone);
  cursor_window_->set_owned_by_parent(false);
  delegate_->SetCursorWindow(cursor_window_.get());
  // Call UpdateCursorImage() to figure out |cursor_window_|'s desired size.
  UpdateCursorImage();
  container_->AddChild(cursor_window_.get());
}

const gfx::ImageSkia& CursorWindowController::GetCursorImageForTest() const {
  return delegate_->cursor_images()[0];
}

bool CursorWindowController::ShouldUseFastInk() const {
  return features::IsFastInkForSoftwareCursorEnabled() &&
         !Shell::Get()->fullscreen_magnifier_controller()->IsEnabled();
}

void CursorWindowController::UpdateCursorMode() {
  if (!container_) {
    return;
  }

  if (ShouldUseFastInk()) {
    delegate_->SetCursorWindow(nullptr);
    cursor_window_.reset();
    UpdateCursorView();
  } else {
    cursor_view_widget_.reset();
    UpdateCursorWindow();
  }
}

}  // namespace ash
