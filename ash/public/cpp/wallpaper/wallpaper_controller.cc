// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/wallpaper/wallpaper_controller.h"

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

}  // namespace ash
