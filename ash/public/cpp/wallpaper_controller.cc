// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/wallpaper_controller.h"

namespace ash {

// static
WallpaperController* WallpaperController::Get() {
  return g_instance_;
}

// static
WallpaperController* WallpaperController::g_instance_ = nullptr;

}  // namespace ash
