// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_HOST_ASH_WINDOW_TREE_HOST_H_
#define ASH_HOST_ASH_WINDOW_TREE_HOST_H_

#include <memory>

#include "ash/ash_export.h"
#include "ui/display/display.h"

namespace aura {
class WindowTreeHost;
}

namespace gfx {
class Insets;
class Rect;
}

namespace ui {
class LocatedEvent;
}

namespace ash {
struct AshWindowTreeHostInitParams;
class RootWindowTransformer;

class ASH_EXPORT AshWindowTreeHost {
 public:
  AshWindowTreeHost();
  virtual ~AshWindowTreeHost();

  static std::unique_ptr<AshWindowTreeHost> Create(
      const AshWindowTreeHostInitParams& init_params);

  // Confines the cursor to the bounds of the root window. This should do
  // nothing if allow_confine_cursor() returns false.
  virtual void ConfineCursorToRootWindow() = 0;

  // Clips the cursor to the given |bounds_in_root|.
  virtual void ConfineCursorToBoundsInRoot(const gfx::Rect& bounds_in_root) = 0;

  // Returns the last used bounds to confine the mouse cursor in the host's
  // window's pixels.
  virtual gfx::Rect GetLastCursorConfineBoundsInPixels() const = 0;

  virtual void SetRootWindowTransformer(
      std::unique_ptr<RootWindowTransformer> transformer) = 0;

  virtual gfx::Insets GetHostInsets() const = 0;

  virtual aura::WindowTreeHost* AsWindowTreeHost() = 0;

  // Stop listening for events in preparation for shutdown.
  virtual void PrepareForShutdown() {}

  virtual void RegisterMirroringHost(AshWindowTreeHost* mirroring_ash_host) {}

  virtual void SetCursorConfig(const display::Display& display,
                               display::Display::Rotation rotation) = 0;
  virtual void ClearCursorConfig() = 0;

 protected:
  // Returns true if cursor confinement should be allowed. For development
  // builds this will return false, for ease of switching between windows,
  // unless --ash-constrain-pointer-to-root is provided. This is always true on
  // a Chrome OS device.
  bool allow_confine_cursor() const { return allow_confine_cursor_; }

  // Translates the native mouse location into screen coordinates.
  void TranslateLocatedEvent(ui::LocatedEvent* event);

 private:
  const bool allow_confine_cursor_;
};

}  // namespace ash

#endif  // ASH_HOST_ASH_WINDOW_TREE_HOST_H_
