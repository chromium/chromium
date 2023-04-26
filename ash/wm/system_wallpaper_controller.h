// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_SYSTEM_WALLPAPER_CONTROLLER_H_
#define ASH_WM_SYSTEM_WALLPAPER_CONTROLLER_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/aura/window_observer.h"

namespace ui {
class Layer;
}

namespace ash {

// SystemWallpaperController manages a ui::Layer that's stacked at the bottom
// of an aura::RootWindow's children.  It exists solely to obscure portions of
// the root layer that aren't covered by any other layers (e.g. before the
// wallpaper image is loaded at startup, or when we scale down all of the other
// layers as part of a power-button or window-management animation).
// It should never be transformed or restacked.
class SystemWallpaperController : public aura::WindowObserver {
 public:
  SystemWallpaperController(aura::Window* root_window, SkColor color);

  SystemWallpaperController(const SystemWallpaperController&) = delete;
  SystemWallpaperController& operator=(const SystemWallpaperController&) =
      delete;

  ~SystemWallpaperController() override;

  // aura::WindowObserver:
  void OnWindowBoundsChanged(aura::Window* root,
                             const gfx::Rect& old_bounds,
                             const gfx::Rect& new_bounds,
                             ui::PropertyChangeReason reason) override;

 private:
  raw_ptr<aura::Window, ExperimentalAsh> root_window_;  // not owned

  std::unique_ptr<ui::Layer> layer_;
};

}  // namespace ash

#endif  // ASH_WM_SYSTEM_WALLPAPER_CONTROLLER_H_
