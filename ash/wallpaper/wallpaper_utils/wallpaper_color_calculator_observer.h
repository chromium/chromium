// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WALLPAPER_WALLPAPER_UTILS_WALLPAPER_COLOR_CALCULATOR_OBSERVER_H_
#define ASH_WALLPAPER_WALLPAPER_UTILS_WALLPAPER_COLOR_CALCULATOR_OBSERVER_H_

namespace ash {

// Observer for the WallpaperColorCalculator.
class WallpaperColorCalculatorObserver {
 public:
  // Notified when a color calculation completes.
  virtual void OnColorCalculationComplete() = 0;

 protected:
  virtual ~WallpaperColorCalculatorObserver() {}
};

}  // namespace ash

#endif  // ASH_WALLPAPER_WALLPAPER_UTILS_WALLPAPER_COLOR_CALCULATOR_OBSERVER_H_
