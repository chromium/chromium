// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_DISPLAY_SCREEN_ASH_H_
#define ASH_DISPLAY_SCREEN_ASH_H_

#include <stdint.h>
#include <memory>

#include "ash/ash_export.h"
#include "chromeos/dbus/power/power_manager_client.h"
#include "ui/display/screen.h"

namespace display {
class DisplayManager;
class DisplayObserver;
}

namespace gfx {
class Rect;
}

namespace ash {

// Aura implementation of display::Screen. Implemented here to avoid circular
// dependencies.
class ASH_EXPORT ScreenAsh : public display::Screen,
                             public chromeos::PowerManagerClient::Observer {
 public:
  ScreenAsh();

  ScreenAsh(const ScreenAsh&) = delete;
  ScreenAsh& operator=(const ScreenAsh&) = delete;

  ~ScreenAsh() override;

  // display::Screen overrides:
  gfx::Point GetCursorScreenPoint() override;
  bool IsWindowUnderCursor(gfx::NativeWindow window) override;
  gfx::NativeWindow GetWindowAtScreenPoint(const gfx::Point& point) override;
  gfx::NativeWindow GetLocalProcessWindowAtPoint(
      const gfx::Point& point,
      const std::set<gfx::NativeWindow>& ignore) override;
  int GetNumDisplays() const override;
  const std::vector<display::Display>& GetAllDisplays() const override;
  display::Display GetDisplayNearestWindow(
      gfx::NativeWindow window) const override;
  display::Display GetDisplayNearestPoint(
      const gfx::Point& point) const override;
  display::Display GetDisplayMatching(
      const gfx::Rect& match_rect) const override;
  display::Display GetPrimaryDisplay() const override;
  void SetDisplayForNewWindows(int64_t display_id) override;
  void AddObserver(display::DisplayObserver* observer) override;
  void RemoveObserver(display::DisplayObserver* observer) override;
  display::TabletState GetTabletState() const override;

  // chromeos::PowerManagerClient::Observer overrides:
  void ScreenBrightnessChanged(
      const power_manager::BacklightBrightnessChange& change) override;

  // CreateDisplayManager with a ScreenAsh instance.
  static std::unique_ptr<display::DisplayManager> CreateDisplayManager();

  // Create a screen instance to be used during shutdown.
  static void CreateScreenForShutdown();

  // Test helpers may need to clean up the ScreenForShutdown between tests.
  static void DeleteScreenForShutdown();
};

}  // namespace ash

#endif  // ASH_DISPLAY_SCREEN_ASH_H_
