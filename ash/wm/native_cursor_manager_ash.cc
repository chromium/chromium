// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/native_cursor_manager_ash.h"

#include <algorithm>
#include <optional>

#include "ash/constants/ash_constants.h"
#include "ash/display/cursor_window_controller.h"
#include "ash/display/window_tree_host_manager.h"
#include "ash/shell.h"
#include "base/check.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/aura/client/cursor_shape_client.h"
#include "ui/aura/env.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/cursor/cursor.h"
#include "ui/base/cursor/mojom/cursor_type.mojom-shared.h"
#include "ui/display/types/display_constants.h"
#include "ui/wm/core/native_cursor_manager_delegate.h"

namespace ash {

namespace {

constexpr float kMinInternalCursorSizeInMm = 4.0f;

void SetCursorOnAllRootWindows(gfx::NativeCursor cursor) {
  aura::Window::Windows root_windows = Shell::Get()->GetAllRootWindows();
  for (aura::Window::Windows::iterator iter = root_windows.begin();
       iter != root_windows.end(); ++iter)
    (*iter)->GetHost()->SetCursor(cursor);

  Shell::Get()
      ->window_tree_host_manager()
      ->cursor_window_controller()
      ->SetCursor(cursor);
}

void NotifyCursorVisibilityChange(bool visible) {
  aura::Window::Windows root_windows = Shell::Get()->GetAllRootWindows();
  for (aura::Window::Windows::iterator iter = root_windows.begin();
       iter != root_windows.end(); ++iter)
    (*iter)->GetHost()->OnCursorVisibilityChanged(visible);

  Shell::Get()
      ->window_tree_host_manager()
      ->cursor_window_controller()
      ->SetVisibility(visible);
}

void NotifyMouseEventsEnableStateChange(bool enabled) {
  aura::Window::Windows root_windows = Shell::Get()->GetAllRootWindows();
  for (aura::Window::Windows::iterator iter = root_windows.begin();
       iter != root_windows.end(); ++iter)
    (*iter)->GetHost()->dispatcher()->OnMouseEventsEnableStateChanged(enabled);
  // Mirror window never process events.
}

}  // namespace

NativeCursorManagerAsh::NativeCursorManagerAsh()
    : native_cursor_enabled_(true) {
  aura::client::SetCursorShapeClient(&cursor_loader_);
}

NativeCursorManagerAsh::~NativeCursorManagerAsh() {
  aura::client::SetCursorShapeClient(nullptr);
}

void NativeCursorManagerAsh::SetNativeCursorEnabled(bool enabled) {
  native_cursor_enabled_ = enabled;

  ::wm::CursorManager* cursor_manager = Shell::Get()->cursor_manager();
  SetCursor(cursor_manager->GetCursor(), cursor_manager);
}

// static
float NativeCursorManagerAsh::GetDefaultCursorScale(
    const display::Display& display) {
  const float scale = display.device_scale_factor();
  if (!display.IsInternal()) {
    return scale;
  }
  const float min_cursor_size_in_px = kMinInternalCursorSizeInMm *
                                      display.GetPixelsPerInchX() /
                                      display::kInchInMm;
  return std::max(scale, min_cursor_size_in_px / kMinLargeCursorSize);
}

display::Display::Rotation NativeCursorManagerAsh::GetRotation() const {
  return cursor_loader_.rotation();
}

void NativeCursorManagerAsh::SetDisplay(
    const display::Display& display,
    ::wm::NativeCursorManagerDelegate* delegate) {
  UpdateCursor(display, custom_cursor_size_, delegate);
}

void NativeCursorManagerAsh::SetCursor(
    gfx::NativeCursor cursor,
    ::wm::NativeCursorManagerDelegate* delegate) {
  if (native_cursor_enabled_) {
    cursor_loader_.SetPlatformCursor(&cursor);
  } else {
    gfx::NativeCursor invisible_cursor(ui::mojom::CursorType::kNone);
    cursor_loader_.SetPlatformCursor(&invisible_cursor);
    cursor.SetPlatformCursor(invisible_cursor.platform());
  }

  delegate->CommitCursor(cursor);

  if (delegate->IsCursorVisible())
    SetCursorOnAllRootWindows(cursor);
}

void NativeCursorManagerAsh::SetCustomCursorSize(
    std::optional<int> custom_cursor_size,
    ::wm::NativeCursorManagerDelegate* delegate) {
  UpdateCursor(display_, custom_cursor_size, delegate);
}

void NativeCursorManagerAsh::SetVisibility(
    bool visible,
    ::wm::NativeCursorManagerDelegate* delegate) {
  delegate->CommitVisibility(visible);

  if (visible) {
    SetCursor(delegate->GetCursor(), delegate);
  } else {
    gfx::NativeCursor invisible_cursor(ui::mojom::CursorType::kNone);
    cursor_loader_.SetPlatformCursor(&invisible_cursor);
    SetCursorOnAllRootWindows(invisible_cursor);
  }

  NotifyCursorVisibilityChange(visible);
}

void NativeCursorManagerAsh::SetMouseEventsEnabled(
    bool enabled,
    ::wm::NativeCursorManagerDelegate* delegate) {
  delegate->CommitMouseEventsEnabled(enabled);

  if (enabled)
    aura::Env::GetInstance()->SetLastMouseLocation(disabled_cursor_location_);
  else
    disabled_cursor_location_ = aura::Env::GetInstance()->last_mouse_location();

  SetVisibility(delegate->IsCursorVisible(), delegate);
  NotifyMouseEventsEnableStateChange(enabled);
}

void NativeCursorManagerAsh::SetCursorColor(
    SkColor color,
    ::wm::NativeCursorManagerDelegate* delegate) {
  cursor_loader_.SetColor(color);
  delegate->CommitCursorColor(color);

  // Sets the cursor to reflect the color change immediately.
  if (delegate->IsCursorVisible()) {
    SetCursor(delegate->GetCursor(), delegate);
  }

  Shell::Get()
      ->window_tree_host_manager()
      ->cursor_window_controller()
      ->SetCursorColor(color);
}

void NativeCursorManagerAsh::UpdateCursor(
    const display::Display& display,
    std::optional<int> custom_cursor_size,
    ::wm::NativeCursorManagerDelegate* delegate) {
  const bool custom_size_changed = custom_cursor_size != custom_cursor_size_;
  // If a custom cursor size is set, use it as is. Otherwise, use a scale that
  // keeps the cursor at least 4mm tall on internal displays.
  const float scale = custom_cursor_size ? display.device_scale_factor()
                                         : GetDefaultCursorScale(display);
  const bool loader_changed =
      cursor_loader_.UpdateRotationAndScale(display.panel_rotation(), scale);
  const bool display_changed =
      display != display_ ||
      display.panel_rotation() != display_.panel_rotation() ||
      display.GetPixelsPerInchX() != display_.GetPixelsPerInchX() ||
      display.GetPixelsPerInchY() != display_.GetPixelsPerInchY() ||
      (display.is_valid() && !custom_size_changed && loader_changed);
  if (!display_changed && !custom_size_changed) {
    return;
  }
  display_ = display;
  custom_cursor_size_ = custom_cursor_size;

  if (custom_size_changed) {
    cursor_loader_.SetCustomCursorSize(custom_cursor_size_);
    delegate->CommitCustomCursorSize(custom_cursor_size_);
  }
  if (loader_changed || (custom_size_changed && delegate->IsCursorVisible())) {
    SetCursor(delegate->GetCursor(), delegate);
  }

  auto* cursor_window_controller =
      Shell::Get()->window_tree_host_manager()->cursor_window_controller();
  if (custom_size_changed) {
    cursor_window_controller->SetCustomCursorSize(custom_cursor_size_);
  }
  if (display_changed && display_.is_valid()) {
    cursor_window_controller->SetDisplay(display_);
    Shell::Get()->UpdateCursorCompositingEnabled();
  }
}

}  // namespace ash
