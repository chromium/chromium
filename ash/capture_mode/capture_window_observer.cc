// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/capture_mode/capture_window_observer.h"

#include "ash/capture_mode/capture_mode_controller.h"
#include "ash/capture_mode/capture_mode_session.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/public/cpp/window_finder.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/shell.h"
#include "ui/base/cursor/cursor_factory.h"
#include "ui/base/cursor/cursor_util.h"
#include "ui/display/screen.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/wm/core/window_util.h"
#include "ui/wm/public/activation_client.h"

namespace ash {

CaptureWindowObserver::CaptureWindowObserver(
    CaptureModeSession* capture_mode_session,
    CaptureModeType type)
    : capture_type_(type),
      original_cursor_(Shell::Get()->cursor_manager()->GetCursor()),
      capture_mode_session_(capture_mode_session) {
  Shell::Get()->activation_client()->AddObserver(this);
}

CaptureWindowObserver::~CaptureWindowObserver() {
  auto* shell = Shell::Get();
  shell->activation_client()->RemoveObserver(this);
  StopObserving();
  ::wm::CursorManager* cursor_manager = shell->cursor_manager();
  if (is_cursor_locked_) {
    cursor_manager->UnlockCursor();
    cursor_manager->SetCursor(original_cursor_);
    is_cursor_locked_ = false;
  }
}

void CaptureWindowObserver::UpdateSelectedWindowAtPosition(
    const gfx::Point& location_in_screen) {
  UpdateSelectedWindowAtPosition(location_in_screen, /*ignore_windows=*/{});
}

void CaptureWindowObserver::OnCaptureTypeChanged(CaptureModeType new_type) {
  capture_type_ = new_type;
  UpdateMouseCursor();
}

void CaptureWindowObserver::OnWindowBoundsChanged(
    aura::Window* window,
    const gfx::Rect& old_bounds,
    const gfx::Rect& new_bounds,
    ui::PropertyChangeReason reason) {
  DCHECK_EQ(window, window_);
  RepaintCaptureRegion();
}

void CaptureWindowObserver::OnWindowVisibilityChanging(aura::Window* window,
                                                       bool visible) {
  DCHECK_EQ(window, window_);
  DCHECK(!visible);
  StopObserving();
  UpdateSelectedWindowAtPosition(location_in_screen_,
                                 /*ignore_windows=*/{window});
}

void CaptureWindowObserver::OnWindowDestroying(aura::Window* window) {
  DCHECK_EQ(window, window_);
  StopObserving();
  UpdateSelectedWindowAtPosition(location_in_screen_,
                                 /*ignore_windows=*/{window});
}

void CaptureWindowObserver::OnWindowActivated(ActivationReason reason,
                                              aura::Window* gained_active,
                                              aura::Window* lost_active) {
  // If another window is activated on top of the current selected window, we
  // may change the selected window to the activated window if it's under the
  // current event location. If there is no selected window at the moment, we
  // also want to check if new activated window should be focused.
  UpdateSelectedWindowAtPosition(location_in_screen_, /*ignore_windows=*/{});
}

void CaptureWindowObserver::StartObserving(aura::Window* window) {
  window_ = window;
  window_->AddObserver(this);
}

void CaptureWindowObserver::StopObserving() {
  if (window_) {
    window_->RemoveObserver(this);
    window_ = nullptr;
  }
}

void CaptureWindowObserver::UpdateSelectedWindowAtPosition(
    const gfx::Point& location_in_screen,
    const std::set<aura::Window*>& ignore_windows) {
  location_in_screen_ = location_in_screen;
  // Find the toplevel window under the mouse/touch position.
  aura::Window* window =
      GetTopmostWindowAtPoint(location_in_screen_, ignore_windows);
  if (window_ == window)
    return;

  // Don't capture wallpaper window.
  if (window && window->parent() &&
      window->parent()->id() == kShellWindowId_WallpaperContainer) {
    window = nullptr;
  }

  // Stop observing the current selected window if there is one.
  aura::Window* previous_selected_window = window_;
  StopObserving();
  if (window)
    StartObserving(window);
  RepaintCaptureRegion();

  // Change mouse cursor depending on capture type and capture window if
  // applicable.
  const bool should_update_cursor =
      !previous_selected_window != !window_ &&
      Shell::Get()->cursor_manager()->IsCursorVisible();
  if (should_update_cursor)
    UpdateMouseCursor();
}

void CaptureWindowObserver::RepaintCaptureRegion() {
  ui::Layer* layer = capture_mode_session_->layer();
  layer->SchedulePaint(layer->bounds());
}

void CaptureWindowObserver::UpdateMouseCursor() {
  ::wm::CursorManager* cursor_manager = Shell::Get()->cursor_manager();
  if (window_) {
    // Change the mouse cursor to a capture icon or a recording icon.
    ui::Cursor cursor(ui::mojom::CursorType::kCustom);
    const display::Display display =
        display::Screen::GetScreen()->GetDisplayNearestWindow(window_);
    const float device_scale_factor = display.device_scale_factor();
    // TODO: Adjust the icon color after spec is updated.
    const gfx::ImageSkia icon = gfx::CreateVectorIcon(
        capture_type_ == CaptureModeType::kImage ? kCaptureModeImageIcon
                                                 : kCaptureModeVideoIcon,
        SK_ColorBLACK);
    SkBitmap bitmap = *icon.bitmap();
    gfx::Point hotspot(bitmap.width() / 2, bitmap.height() / 2);
    ui::ScaleAndRotateCursorBitmapAndHotpoint(
        device_scale_factor, display.panel_rotation(), &bitmap, &hotspot);
    auto* cursor_factory = ui::CursorFactory::GetInstance();
    ui::PlatformCursor platform_cursor =
        cursor_factory->CreateImageCursor(bitmap, hotspot);
    cursor.SetPlatformCursor(platform_cursor);
    cursor.set_custom_bitmap(bitmap);
    cursor.set_custom_hotspot(hotspot);
    cursor_factory->UnrefImageCursor(platform_cursor);

    // Unlock the cursor first so that it can be changed.
    if (is_cursor_locked_)
      cursor_manager->UnlockCursor();
    cursor_manager->SetCursor(cursor);
    cursor_manager->LockCursor();
    is_cursor_locked_ = true;
  } else {
    // Revert back to its previous mouse cursor setting.
    if (is_cursor_locked_) {
      cursor_manager->UnlockCursor();
      is_cursor_locked_ = false;
    }
    cursor_manager->SetCursor(original_cursor_);
  }
}

}  // namespace ash
