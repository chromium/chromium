// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WALLPAPER_WALLPAPER_UTILS_WALLPAPER_RESIZER_OBSERVER_H_
#define ASH_WALLPAPER_WALLPAPER_UTILS_WALLPAPER_RESIZER_OBSERVER_H_

namespace ash {

class WallpaperResizerObserver {
 public:
  // Invoked when the wallpaper is resized.
  virtual void OnWallpaperResized() = 0;

 protected:
  virtual ~WallpaperResizerObserver() {}
};

}  // namespace ash

#endif  // ASH_WALLPAPER_WALLPAPER_UTILS_WALLPAPER_RESIZER_OBSERVER_H_
