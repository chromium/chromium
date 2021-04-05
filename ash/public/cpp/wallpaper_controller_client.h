// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_WALLPAPER_CONTROLLER_CLIENT_H_
#define ASH_PUBLIC_CPP_WALLPAPER_CONTROLLER_CLIENT_H_

#include "ash/public/cpp/ash_public_export.h"

class AccountId;

namespace ash {

// Used by ash to control a Chrome client of the WallpaperController.
class ASH_PUBLIC_EXPORT WallpaperControllerClient {
 public:
  // Opens the wallpaper picker window.
  virtual void OpenWallpaperPicker() = 0;

  // Closes the app side of the wallpaper preview (top header bar) if it is
  // currently open.
  virtual void MaybeClosePreviewWallpaper() = 0;

  // Sets the default wallpaper and removes the file for the previous wallpaper.
  virtual void SetDefaultWallpaper(const AccountId& account_id,
                                   bool show_wallpaper) = 0;
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_WALLPAPER_CONTROLLER_CLIENT_H_
