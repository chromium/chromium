// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_NATIVE_CURSOR_MANAGER_ASH_H_
#define ASH_WM_NATIVE_CURSOR_MANAGER_ASH_H_

#include "ash/ash_export.h"
#include "ui/display/display.h"
#include "ui/wm/core/cursor_loader.h"
#include "ui/wm/core/native_cursor_manager.h"

namespace ash {

// This does the ash-specific setting of cursor details like cursor
// visibility. It communicates back with the CursorManager through the
// NativeCursorManagerDelegate interface, which receives messages about what
// changes were acted on.
class ASH_EXPORT NativeCursorManagerAsh : public ::wm::NativeCursorManager {
 public:
  NativeCursorManagerAsh();

  NativeCursorManagerAsh(const NativeCursorManagerAsh&) = delete;
  NativeCursorManagerAsh& operator=(const NativeCursorManagerAsh&) = delete;

  ~NativeCursorManagerAsh() override;

  // Toggle native cursor enabled/disabled.
  // The native cursor is enabled by default. When disabled, we hide the native
  // cursor regardless of visibility state, and let CursorWindowController draw
  // the cursor.
  void SetNativeCursorEnabled(bool enabled);

  // Returns the rotation of the currently loaded cursor.
  display::Display::Rotation GetRotation() const;

  // Overridden from ::wm::NativeCursorManager:
  void SetDisplay(const display::Display& display,
                  ::wm::NativeCursorManagerDelegate* delegate) override;
  void SetCursor(gfx::NativeCursor cursor,
                 ::wm::NativeCursorManagerDelegate* delegate) override;
  void SetVisibility(bool visible,
                     ::wm::NativeCursorManagerDelegate* delegate) override;
  void SetCursorSize(ui::CursorSize cursor_size,
                     ::wm::NativeCursorManagerDelegate* delegate) override;
  void SetMouseEventsEnabled(
      bool enabled,
      ::wm::NativeCursorManagerDelegate* delegate) override;

 private:
  friend class CursorManagerTestApi;

  // The cursor location where the cursor was disabled.
  gfx::Point disabled_cursor_location_;

  bool native_cursor_enabled_;

  wm::CursorLoader cursor_loader_{/*use_platform_cursors=*/false};
};

}  // namespace ash

#endif  // ASH_WM_NATIVE_CURSOR_MANAGER_ASH_H_
