// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/display/cursor_window_controller.h"

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
#include "ui/wm/core/cursor_util.h"

namespace ash {

namespace {

const int kMinLargeCursorSize = 25;
const int kMaxLargeCursorSize = 64;
const int kMaxExtraLargeCursorSize = 128;

SkBitmap GetColorAdjustedBitmap(const gfx::ImageSkiaRep& image_rep,
                                SkColor cursor_color) {
  const SkBitmap& bitmap = image_rep.GetBitmap();
  // Recolor the black and greyscale parts of the image based on
  // cursor_color_. Do not recolor pure white or tinted portions of the image,
  // this ensures we do not impact the colored portions of cursors or the
  // transition between the colored portion and white outline.
  // TODO(crbug.com/1085442): Programmatically find a way to recolor the white
  // parts in order to draw a black outline, but without impacting cursors
  // like noDrop which contained tinted portions. Or, add new assets with
  // black and white inverted for easier re-coloring.
  SkBitmap recolored;
  recolored.allocN32Pixels(bitmap.width(), bitmap.height());
  recolored.eraseARGB(0, 0, 0, 0);
  SkCanvas canvas(recolored);
  canvas.drawImage(bitmap.asImage(), 0, 0);
  color_utils::HSL cursor_hsl;
  color_utils::SkColorToHSL(cursor_color, &cursor_hsl);
  for (int y = 0; y < bitmap.height(); ++y) {
    for (int x = 0; x < bitmap.width(); ++x) {
      const SkColor color = bitmap.getColor(x, y);
      // If the alpha is lower than 1, it's transparent, skip it.
      if (SkColorGetA(color) < 1) {
        continue;
      }
      // Convert to HSL: We want to change the hue and saturation, and
      // map the lightness from 0-100 to cursor_hsl.l-100. This means that
      // things which were black (l=0) become the cursor color lightness, and
      // things which were white (l=100) stay white.
      color_utils::HSL hsl;
      color_utils::SkColorToHSL(color, &hsl);
      // If it has color, do not change it.
      if (hsl.s > 0.01) {
        continue;
      }
      color_utils::HSL result;
      result.h = cursor_hsl.h;
      result.s = cursor_hsl.s;
      result.l = hsl.l * (1 - cursor_hsl.l) + cursor_hsl.l;
      SkPaint paint;
      paint.setColor(color_utils::HSLToSkColor(result, SkColorGetA(color)));
      canvas.drawRect(SkRect::MakeXYWH(x, y, 1, 1), paint);
    }
  }
  return recolored;
}

std::vector<gfx::ImageSkia> GetCursorImages(ui::CursorSize cursor_size,
                                            ui::mojom::CursorType type,
                                            float scale_factor,
                                            gfx::Point& out_hotspot) {
  std::vector<gfx::ImageSkia> images;
  int resource_id;
  if (!wm::GetCursorDataFor(cursor_size, type, scale_factor, &resource_id,
                            &out_hotspot)) {
    return images;
  }
  gfx::ImageSkia* image =
      ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(resource_id);
  const int frame_height = image->height();
  // Assume frame width equals to frame height.
  const int frame_width = frame_height;
  const int total_width = image->width();
  const int frame_count = total_width / frame_width;

  if (frame_count == 1) {
    images.push_back(*image);
  } else if (frame_count > 1) {
    // For animated cursor, the input image is a sesquence of frames which
    // needs to be cut into a list of images.
    images.resize(frame_count);
    for (int frame = 0; frame < frame_count; ++frame) {
      const int x_offset = frame_width * frame;
      gfx::ImageSkia cropped = gfx::ImageSkiaOperations::CreateTiledImage(
          *image, x_offset, 0, frame_width, frame_height);
      images[frame] = std::move(cropped);
    }
  }
  return images;
}

// The ImageSkiaSource that translate the color of the cursor.
class CursorImageSource : public gfx::ImageSkiaSource {
 public:
  CursorImageSource(gfx::ImageSkia& image_skia, SkColor cursor_color)
      : image_skia_(image_skia), cursor_color_(cursor_color) {
    DCHECK_NE(cursor_color_, kDefaultCursorColor);
  }
  CursorImageSource(const CursorImageSource&) = delete;
  CursorImageSource& operator=(const CursorImageSource&) = delete;
  ~CursorImageSource() override = default;

  // gfx::ImageSkiaSource:
  gfx::ImageSkiaRep GetImageForScale(float scale) override {
    const gfx::ImageSkiaRep& rep = image_skia_.GetRepresentation(scale);
    return gfx::ImageSkiaRep(GetColorAdjustedBitmap(rep, cursor_color_),
                             rep.scale());
  }

 private:
  gfx::ImageSkia image_skia_;
  SkColor cursor_color_;
};

}  // namespace

class CursorWindowDelegate : public aura::WindowDelegate {
 public:
  CursorWindowDelegate() = default;

  CursorWindowDelegate(const CursorWindowDelegate&) = delete;
  CursorWindowDelegate& operator=(const CursorWindowDelegate&) = delete;

  ~CursorWindowDelegate() override = default;

  // aura::WindowDelegate overrides:
  gfx::Size GetMinimumSize() const override { return size_; }
  gfx::Size GetMaximumSize() const override { return size_; }
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
    : delegate_(new CursorWindowDelegate()),
      is_fast_ink_enabled_(features::IsFastInkForSoftwareCursorEnabled()) {}

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
      std::min(large_cursor_size_in_dip,
               ::features::IsAccessibilityExtraLargeCursorEnabled()
                   ? kMaxExtraLargeCursorSize
                   : kMaxLargeCursorSize);
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

  if (cursor_color_ != kDefaultCursorColor)
    return true;

  PrefService* prefs = shell->session_controller()->GetActivePrefService();
  if (!prefs) {
    // The active pref service can be null early in startup.
    return false;
  }

  if (prefs->GetBoolean(prefs::kNightLightEnabled)) {
    // All or some displays don't support setting a CRTC matrix, which means
    // Night Light is using the composited color matrix, and hence software
    // cursor should be used.
    // TODO(afakhry): Instead of switching to the composited cursor on all
    // displays if any of them don't support a CRTC matrix, we should provide
    // the functionality to turn on the composited cursor on a per-display basis
    // (i.e. use it only on the displays that don't support CRTC matrices).
    const DisplayColorManager::DisplayCtmSupport displays_ctm_support =
        shell->display_color_manager()->displays_ctm_support();
    UMA_HISTOGRAM_ENUMERATION("Ash.NightLight.DisplayCrtcCtmSupport",
                              displays_ctm_support);
    if (displays_ctm_support != DisplayColorManager::DisplayCtmSupport::kAll)
      return true;
  }

  return prefs->GetBoolean(prefs::kAccessibilityLargeCursorEnabled) ||
         prefs->GetBoolean(prefs::kAccessibilityHighContrastEnabled) ||
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
    display::Screen* screen = display::Screen::GetScreen();
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
  if (!is_cursor_compositing_enabled_)
    return;

  // TODO(oshima): Do not update the composition cursor when crossing
  // display in unified desktop mode for now. crbug.com/517222.
  if (Shell::Get()->display_manager()->IsInUnifiedMode() &&
      display.id() != display::kUnifiedDisplayId) {
    return;
  }

  display_ = display;
  aura::Window* root_window = Shell::GetRootWindowForDisplayId(display.id());
  if (!root_window)
    return;

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

void CursorWindowController::UpdateLocation() {
  if (!cursor_window_)
    return;
  gfx::Point point = aura::Env::GetInstance()->last_mouse_location();
  point.Offset(-bounds_in_screen_.x(), -bounds_in_screen_.y());
  point.Offset(-hot_point_.x(), -hot_point_.y());
  gfx::Rect bounds = cursor_window_->bounds();
  bounds.set_origin(point);
  cursor_window_->SetBounds(bounds);
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

gfx::Rect CursorWindowController::GetBoundsForTest() const {
  return cursor_window_->GetBoundsInScreen();
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

  if (is_fast_ink_enabled_) {
    UpdateCursorView();
  } else {
    delegate_->SetCursorWindow(nullptr);
    // Reusing the window does not work when the display is disconnected.
    // Just creates a new one instead. crbug.com/384218.
    cursor_window_ = std::make_unique<aura::Window>(delegate_.get());
    cursor_window_->SetTransparent(true);
    cursor_window_->Init(ui::LAYER_TEXTURED);
    cursor_window_->SetEventTargetingPolicy(aura::EventTargetingPolicy::kNone);
    cursor_window_->set_owned_by_parent(false);
    delegate_->SetCursorWindow(cursor_window_.get());
    // Call UpdateCursorImage() to figure out |cursor_window_|'s desired size.
    UpdateCursorImage();
    container->AddChild(cursor_window_.get());
  }
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

  float cursor_scale;
  std::vector<gfx::ImageSkia> images;
  gfx::Point hot_point_in_physical_pixels;
  if (cursor_.type() == ui::mojom::CursorType::kCustom) {
    SkBitmap bitmap = cursor_.custom_bitmap();
    gfx::Point hotspot = cursor_.custom_hotspot();
    if (bitmap.isNull()) {
      return;
    }
    cursor_scale = cursor_.image_scale_factor();

    // Custom cursor's bitmap is already rotated. Revert the rotation because
    // software cursor's rotation is handled by viz.
    const display::Display::Rotation inverted_rotation =
        static_cast<display::Display::Rotation>(
            (4 - static_cast<int>(display_.rotation())) % 4);
    wm::ScaleAndRotateCursorBitmapAndHotpoint(1.0f, inverted_rotation, &bitmap,
                                              &hotspot);
    images.push_back(gfx::ImageSkia::CreateFromBitmap(bitmap, cursor_scale));
    hot_point_in_physical_pixels = hotspot;
  } else {
    // Do not use the device scale factor, as the cursor will be scaled
    // by compositor. HW cursor will not be scaled by display zoom, so the
    // physical size will be inconsistent.
    cursor_scale = ui::GetScaleForResourceScaleFactor(
        ui::GetSupportedResourceScaleFactorForRescale(
            display_.device_scale_factor()));

    images = GetCursorImages(cursor_size_, cursor_.type(), cursor_scale,
                             hot_point_in_physical_pixels);
    if (images.empty()) {
      return;
    }
  }
  // Use `gfx::ToFlooredPoint` as `ImageSkiaRep::GetWidth` is implemented as
  // `return static_cast<int>(pixel_width() / scale());`.
  hot_point_ = gfx::ToFlooredPoint(
      gfx::ConvertPointToDips(hot_point_in_physical_pixels, cursor_scale));

  // Rescale cursor size. This is used with the combination of accessibility
  // large cursor. We don't need to care about the case where cursor
  // compositing is disabled as we always use cursor compositing if
  // accessibility large cursor is enabled.
  if (cursor_size_ == ui::CursorSize::kLarge &&
      large_cursor_size_in_dip_ != images[0].size().width()) {
    const float rescale = static_cast<float>(large_cursor_size_in_dip_) /
                          static_cast<float>(images[0].size().width());
    hot_point_ = gfx::ScaleToCeiledPoint(hot_point_, rescale);
    for (size_t i = 0; i < images.size(); ++i) {
      images[i] = gfx::ImageSkiaOperations::CreateResizedImage(
          images[i], skia::ImageOperations::ResizeMethod::RESIZE_BEST,
          gfx::ScaleToCeiledSize(images[i].size(), rescale));
    }
  }

  if (cursor_color_ != kDefaultCursorColor) {
    for (size_t i = 0; i < images.size(); ++i) {
      images[i] = gfx::ImageSkia(
          std::make_unique<CursorImageSource>(images[i], cursor_color_),
          images[i].size());
    }
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
  // Return if the container's size is not updated yet.
  if (container_->GetBoundsInRootWindow().size() != bounds_in_screen_.size()) {
    return;
  }

  cursor_view_widget_ = CursorView::Create(
      aura::Env::GetInstance()->last_mouse_location(), container_);
  UpdateCursorImage();
}

const gfx::ImageSkia& CursorWindowController::GetCursorImageForTest() const {
  return delegate_->cursor_images()[0];
}

}  // namespace ash
