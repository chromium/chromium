// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/display/cursor_window_controller.h"

#include "ash/components/cursor/cursor_view.h"
#include "ash/display/display_color_manager.h"
#include "ash/display/mirror_window_controller.h"
#include "ash/display/window_tree_host_manager.h"
#include "ash/magnifier/magnification_controller.h"
#include "ash/public/cpp/ash_constants.h"
#include "ash/public/cpp/ash_pref_names.h"
#include "ash/public/cpp/ash_switches.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/root_window_controller.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/window_factory.h"
#include "base/command_line.h"
#include "base/metrics/histogram_macros.h"
#include "components/prefs/pref_service.h"
#include "ui/aura/env.h"
#include "ui/aura/window_delegate.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/base/cursor/cursors_aura.h"
#include "ui/base/hit_test.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/compositor/dip_util.h"
#include "ui/compositor/paint_recorder.h"
#include "ui/display/display.h"
#include "ui/display/manager/display_manager.h"
#include "ui/display/screen.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/dip_util.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/views/widget/widget.h"

namespace ash {
namespace {

const int kMinLargeCursorSize = 25;
const int kMaxLargeCursorSize = 64;

}  // namespace

class CursorWindowDelegate : public aura::WindowDelegate {
 public:
  CursorWindowDelegate() = default;
  ~CursorWindowDelegate() override = default;

  // aura::WindowDelegate overrides:
  gfx::Size GetMinimumSize() const override { return size_; }
  gfx::Size GetMaximumSize() const override { return size_; }
  void OnBoundsChanged(const gfx::Rect& old_bounds,
                       const gfx::Rect& new_bounds) override {}
  gfx::NativeCursor GetCursor(const gfx::Point& point) override {
    return gfx::kNullCursor;
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
    recorder.canvas()->DrawImageInt(cursor_image_, 0, 0);
  }
  void OnDeviceScaleFactorChanged(float old_device_scale_factor,
                                  float new_device_scale_factor) override {}
  void OnWindowDestroying(aura::Window* window) override {}
  void OnWindowDestroyed(aura::Window* window) override {}
  void OnWindowTargetVisibilityChanged(bool visible) override {}
  bool HasHitTestMask() const override { return false; }
  void GetHitTestMask(SkPath* mask) const override {}

  // Sets the cursor image for the |display|'s scale factor.
  void SetCursorImage(const gfx::Size& size, const gfx::ImageSkia& image) {
    size_ = size;
    cursor_image_ = image;
  }

  const gfx::Size& size() const { return size_; }
  const gfx::ImageSkia& cursor_image() const { return cursor_image_; }

 private:
  gfx::ImageSkia cursor_image_;
  gfx::Size size_;

  DISALLOW_COPY_AND_ASSIGN(CursorWindowDelegate);
};

CursorWindowController::CursorWindowController()
    : delegate_(new CursorWindowDelegate()),
      is_cursor_motion_blur_enabled_(
          base::CommandLine::ForCurrentProcess()->HasSwitch(
              switches::kAshEnableCursorMotionBlur)) {}

CursorWindowController::~CursorWindowController() {
  SetContainer(NULL);
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

bool CursorWindowController::ShouldEnableCursorCompositing() {
  if (is_cursor_motion_blur_enabled_)
    return true;

  // During startup, we may not have a preference service yet. We need to check
  // display manager state first so that we don't accidentally ignore it while
  // early outing when there isn't a PrefService yet.
  Shell* shell = Shell::Get();
  display::DisplayManager* display_manager = shell->display_manager();
  if ((display_manager->IsInSoftwareMirrorMode()) ||
      display_manager->IsInUnifiedMode() ||
      display_manager->screen_capture_is_active()) {
    return true;
  }

  if (shell->magnification_controller()->IsEnabled())
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
  if (is_cursor_compositing_enabled_ != enabled) {
    is_cursor_compositing_enabled_ = enabled;
    if (display_.is_valid())
      UpdateCursorImage();
    UpdateContainer();
  }
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

void CursorWindowController::SetContainer(aura::Window* container) {
  if (container_ == container)
    return;
  container_ = container;
  if (!container) {
    cursor_window_.reset();
    cursor_view_.reset();
    return;
  }

  bounds_in_screen_ = display_.bounds();
  rotation_ = display_.rotation();

  if (is_cursor_motion_blur_enabled_) {
    UpdateCursorView();
  } else {
    // Reusing the window does not work when the display is disconnected.
    // Just creates a new one instead. crbug.com/384218.
    cursor_window_ = window_factory::NewWindow(delegate_.get());
    cursor_window_->SetTransparent(true);
    cursor_window_->Init(ui::LAYER_TEXTURED);
    cursor_window_->SetEventTargetingPolicy(aura::EventTargetingPolicy::kNone);
    cursor_window_->set_owned_by_parent(false);
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
  if (bounds == bounds_in_screen_ && rotation == rotation_)
    return;
  bounds_in_screen_ = bounds;
  rotation_ = rotation;
  if (cursor_view_)
    UpdateCursorView();
  UpdateLocation();
}

void CursorWindowController::UpdateCursorImage() {
  if (!is_cursor_compositing_enabled_)
    return;

  // Use the original device scale factor instead of the display's, which
  // might have been adjusted for the UI scale.
  const float original_scale = Shell::Get()
                                   ->display_manager()
                                   ->GetDisplayInfo(display_.id())
                                   .device_scale_factor();
  // And use the nearest resource scale factor.
  float cursor_scale =
      ui::GetScaleForScaleFactor(ui::GetSupportedScaleFactor(original_scale));

  gfx::ImageSkia image;
  if (cursor_.native_type() == ui::CursorType::kCustom) {
    SkBitmap bitmap = cursor_.GetBitmap();
    if (bitmap.isNull())
      return;
    image = gfx::ImageSkia::CreateFrom1xBitmap(bitmap);
    hot_point_ = cursor_.GetHotspot();
  } else {
    int resource_id;
    if (!ui::GetCursorDataFor(cursor_size_, cursor_.native_type(), cursor_scale,
                              &resource_id, &hot_point_)) {
      return;
    }
    image =
        *ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(resource_id);
  }

  gfx::ImageSkia resized = image;

  // Rescale cursor size. This is used with the combination of accessibility
  // large cursor. We don't need to care about the case where cursor
  // compositing is disabled as we always use cursor compositing if
  // accessibility large cursor is enabled.
  if (cursor_size_ == ui::CursorSize::kLarge &&
      large_cursor_size_in_dip_ != image.size().width()) {
    float rescale = static_cast<float>(large_cursor_size_in_dip_) /
                    static_cast<float>(image.size().width());
    resized = gfx::ImageSkiaOperations::CreateResizedImage(
        image, skia::ImageOperations::ResizeMethod::RESIZE_GOOD,
        gfx::ScaleToCeiledSize(image.size(), rescale));
    hot_point_ = gfx::ScaleToCeiledPoint(hot_point_, rescale);
  }

  const gfx::ImageSkiaRep& image_rep = resized.GetRepresentation(cursor_scale);
  delegate_->SetCursorImage(
      resized.size(),
      gfx::ImageSkia(gfx::ImageSkiaRep(image_rep.GetBitmap(), cursor_scale)));
  hot_point_ = gfx::ConvertPointToDIP(cursor_scale, hot_point_);

  if (cursor_view_) {
    cursor_view_->SetCursorImage(delegate_->cursor_image(), delegate_->size(),
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
  bool visible = (visible_ && cursor_.native_type() != ui::CursorType::kNone);
  if (visible) {
    if (cursor_view_)
      cursor_view_->GetWidget()->Show();
    if (cursor_window_)
      cursor_window_->Show();
  } else {
    if (cursor_view_)
      cursor_view_->GetWidget()->Hide();
    if (cursor_window_)
      cursor_window_->Hide();
  }
}

void CursorWindowController::UpdateCursorView() {
  cursor_view_.reset(new cursor::CursorView(
      container_, aura::Env::GetInstance()->last_mouse_location(),
      is_cursor_motion_blur_enabled_));
  UpdateCursorImage();
}

const gfx::ImageSkia& CursorWindowController::GetCursorImageForTest() const {
  return delegate_->cursor_image();
}

}  // namespace ash
