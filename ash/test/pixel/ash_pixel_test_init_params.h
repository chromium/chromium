// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_TEST_PIXEL_ASH_PIXEL_TEST_INIT_PARAMS_H_
#define ASH_TEST_PIXEL_ASH_PIXEL_TEST_INIT_PARAMS_H_

namespace ash::pixel_test {

// Lists the wallpaper types supported during the pixel test setup.
enum class WallpaperInitType {
  // Initializes a regular wallpaper.
  kRegular,

  // Initializes a policy wallpaper.
  kPolicy
};

// The params that control the pixel test setup.
struct InitParams {
  InitParams();
  InitParams(InitParams&&);
  InitParams& operator=(InitParams&&);
  ~InitParams();

  WallpaperInitType wallpaper_init_type = WallpaperInitType::kRegular;

  // If true, the system UI layout follows the right-to-left fashion.
  bool under_rtl = false;
};

}  // namespace ash::pixel_test

#endif  // ASH_TEST_PIXEL_ASH_PIXEL_TEST_INIT_PARAMS_H_
