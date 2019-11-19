// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_WALLPAPER_CONTROLLER_OBSERVER_H_
#define ASH_PUBLIC_CPP_WALLPAPER_CONTROLLER_OBSERVER_H_

#include "ash/public/cpp/ash_public_export.h"
#include "base/macros.h"

namespace ash {

// Used to listen for wallpaper state changes.
class ASH_PUBLIC_EXPORT WallpaperControllerObserver {
 public:
  WallpaperControllerObserver();

  // Invoked when the wallpaper is about to change.
  virtual void OnWallpaperChanging() {}

  // Invoked when the wallpaper is changed.
  virtual void OnWallpaperChanged() {}

  // Invoked when the colors extracted from the current wallpaper change.
  virtual void OnWallpaperColorsChanged() {}

  // Invoked when the blur state of the wallpaper changes.
  // TODO(crbug.com/875128): Remove this after web-ui login code is completely
  // removed.
  virtual void OnWallpaperBlurChanged() {}

  // Invoked when the wallpaper preview mode starts.
  virtual void OnWallpaperPreviewStarted() {}

  // Invoked when the wallpaper preview mode ends.
  virtual void OnWallpaperPreviewEnded() {}

  // Invoked when the first wallpaper is set. The first wallpaper is the one
  // shown right after boot splash screen or after a session restart.
  virtual void OnFirstWallpaperShown() {}

 protected:
  virtual ~WallpaperControllerObserver();

 private:
  DISALLOW_COPY_AND_ASSIGN(WallpaperControllerObserver);
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_WALLPAPER_CONTROLLER_OBSERVER_H_
