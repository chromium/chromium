// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_NATIVE_CURSOR_MANAGER_ASH_H_
#define ASH_WM_NATIVE_CURSOR_MANAGER_ASH_H_

#include "ash/ash_export.h"
#include "base/macros.h"
#include "ui/display/display.h"
#include "ui/wm/core/native_cursor_manager.h"

namespace ui {
class ImageCursors;
}

namespace ash {

// This does the ash-specific setting of cursor details like cursor
// visibility. It communicates back with the CursorManager through the
// NativeCursorManagerDelegate interface, which receives messages about what
// changes were acted on.
class ASH_EXPORT NativeCursorManagerAsh : public ::wm::NativeCursorManager {
 public:
  NativeCursorManagerAsh();
  ~NativeCursorManagerAsh() override;

  // Toggle native cursor enabled/disabled.
  // The native cursor is enabled by default. When disabled, we hide the native
  // cursor regardless of visibility state, and let CursorWindowManager draw
  // the cursor.
  void SetNativeCursorEnabled(bool enabled);

  // Returns the scale and rotation of the currently loaded cursor.
  float GetScale() const;
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

  std::unique_ptr<ui::ImageCursors> image_cursors_;

  DISALLOW_COPY_AND_ASSIGN(NativeCursorManagerAsh);
};

}  // namespace ash

#endif  // ASH_WM_NATIVE_CURSOR_MANAGER_ASH_H_
