// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/wallpaper/wallpaper_controller.h"

#include "base/strings/string_number_conversions.h"
#include "ui/display/screen.h"

namespace ash {

namespace {
WallpaperController* g_instance = nullptr;
}  // namespace

WallpaperController::WallpaperController() {
  DCHECK_EQ(nullptr, g_instance);
  g_instance = this;
}

WallpaperController::~WallpaperController() {
  DCHECK_EQ(this, g_instance);
  g_instance = nullptr;
}

// static
WallpaperController* WallpaperController::Get() {
  return g_instance;
}

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
