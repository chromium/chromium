// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WALLPAPER_WALLPAPER_UTILS_WALLPAPER_FILE_UTILS_H_
#define ASH_WALLPAPER_WALLPAPER_UTILS_WALLPAPER_FILE_UTILS_H_

#include <string>

#include "ash/ash_export.h"
#include "ash/public/cpp/wallpaper/wallpaper_types.h"

namespace base {
class FilePath;
}  // namespace base

namespace gfx {
class ImageSkia;
class Size;
}  // namespace gfx

namespace ash {

// Directory names of custom wallpapers.
inline constexpr char kSmallWallpaperSubDir[] = "small";
inline constexpr char kLargeWallpaperSubDir[] = "large";
inline constexpr char kOriginalWallpaperSubDir[] = "original";

// Saves the wallpaper `image` to disk at the given `path`, encoded as jpg.
// Returns true if successfully saved and false otherwise. This function is not
// specific to any one WallpaperType.
//
// The `image` may be resized first according to the `preferred_size`
// depending on the `layout` specified.
//
// `image_metadata` is optional. Sets the value if it is required to store the
// XMP metadata for the image. `image_metadata`, if not empty, should be
// constructed as XMP format (like XML standard format). Otherwise,
// `image_metadata` is empty by default, and no metadata is saved for the image.
//
// This performs synchronous blocking file operations and must be called from a
// thread or sequence that allows it.
ASH_EXPORT bool ResizeAndSaveWallpaper(
    const gfx::ImageSkia& image,
    const base::FilePath& path,
    WallpaperLayout layout,
    gfx::Size preferred_size,
    const std::string& image_metadata = std::string());

// Create `directory` and any missing parent directories if it does not exist.
// Does nothing if `directory` already exists. Logs error message for any
// failures. Functionally equivalent to `mkdir -p`.
ASH_EXPORT void CreateDirectoryAndLogError(const base::FilePath& directory);

}  // namespace ash

#endif  // ASH_WALLPAPER_WALLPAPER_UTILS_WALLPAPER_FILE_UTILS_H_
