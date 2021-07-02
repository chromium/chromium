// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/wallpaper/wallpaper_controller.h"

#include "base/strings/string_number_conversions.h"
#include "ui/display/screen.h"

namespace ash {

// static
WallpaperController* WallpaperController::Get() {
  return g_instance_;
}

// static
WallpaperController* WallpaperController::g_instance_ = nullptr;

// static
std::string WallpaperController::GetBackdropWallpaperSuffix() {
  // TODO(b/186807814) handle different display resolutions better.
  // FIFE url is used for Backdrop wallpapers and the desired image size should
  // be specified. Currently we are using two times the display size. This is
  // determined by trial and error and is subject to change.
  gfx::Size display_size =
      display::Screen::GetScreen()->GetPrimaryDisplay().size();
  return "=w" + base::NumberToString(
                    2 * std::max(display_size.width(), display_size.height()));
}

}  // namespace ash
