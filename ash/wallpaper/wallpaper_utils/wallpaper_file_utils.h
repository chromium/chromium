// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WALLPAPER_WALLPAPER_UTILS_WALLPAPER_FILE_UTILS_H_
#define ASH_WALLPAPER_WALLPAPER_UTILS_WALLPAPER_FILE_UTILS_H_

#include "ash/ash_export.h"
#include "ash/public/cpp/wallpaper/wallpaper_types.h"

namespace base {
class FilePath;
}  // namespace base

namespace gfx {
class ImageSkia;
}  // namespace gfx

namespace ash {

// Directory names of custom wallpapers.
constexpr char kSmallWallpaperSubDir[] = "small";
constexpr char kLargeWallpaperSubDir[] = "large";
constexpr char kOriginalWallpaperSubDir[] = "original";

// Saves the wallpaper |image| to disc at the given file |path|. Returns true
// if successfully saved and false otherwise. This function is not specific to
// any one WallpaperType.
//
// The |image| might get resized first according to the |preferred_width| and
// |preferred_height| depending on the |layout| specified. Afterwards, it gets
// encoded and written to disc.
//
// Note this performs synchronous blocking file operations and must be called
// from a thread or sequence that allows it.
ASH_EXPORT bool ResizeAndSaveWallpaper(const gfx::ImageSkia& image,
                                       const base::FilePath& path,
                                       WallpaperLayout layout,
                                       int preferred_width,
                                       int preferred_height);
}  // namespace ash

#endif  // ASH_WALLPAPER_WALLPAPER_UTILS_WALLPAPER_FILE_UTILS_H_
