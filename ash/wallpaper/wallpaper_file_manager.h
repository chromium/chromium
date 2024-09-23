// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WALLPAPER_WALLPAPER_FILE_MANAGER_H_
#define ASH_WALLPAPER_WALLPAPER_FILE_MANAGER_H_

#include <string>

#include "ash/ash_export.h"
#include "ash/public/cpp/wallpaper/wallpaper_controller.h"
#include "ash/public/cpp/wallpaper/wallpaper_types.h"
#include "ash/wallpaper/wallpaper_utils/wallpaper_resolution.h"
#include "base/files/file_path.h"
#include "base/functional/callback_forward.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/ref_counted_memory.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "ui/gfx/image/image_skia.h"
#include "url/gurl.h"

namespace base {
class SequencedTaskRunner;
}  // namespace base

namespace ash {

// Returns the path of the online wallpaper corresponding to `resolution` with
// the base path `wallpaper_dir`.
//
// This method is thread safe.
base::FilePath GetOnlineWallpaperFilePath(const base::FilePath& wallpaper_dir,
                                          const GURL& url,
                                          WallpaperResolution resolution);

// Handles loading wallpaper from disk and saving wallpaper images into disk.
class ASH_EXPORT WallpaperFileManager {
 public:
  WallpaperFileManager();
  WallpaperFileManager(const WallpaperFileManager&) = delete;
  WallpaperFileManager& operator=(const WallpaperFileManager&) = delete;

  ~WallpaperFileManager();

  using LoadWallpaperCallback = base::OnceCallback<void(const gfx::ImageSkia&)>;
  // Loads a previously saved Online or Google Photos wallpaper from
  // `wallpaper_dir` and returns it as a gfx::ImageSkia to the caller. The
  // `callback` is run when the image has been loaded. A null gfx::ImageSkia
  // instance may be returned if loading the wallpaper failed; this usually
  // means the requested Online or Google Photos wallpaper does not exist on
  // disc.
  void LoadWallpaper(WallpaperType type,
                     const base::FilePath& wallpaper_dir,
                     const std::string& location,
                     LoadWallpaperCallback callback);

  using LoadPreviewImageCallback =
      base::OnceCallback<void(scoped_refptr<base::RefCountedMemory>)>;
  // Loads a previously saved image from `wallpaper_dir` and returns it as a
  // scoped_refptr<base::RefCountedMemory> to be served as the preview of an
  // online wallpaper caller. The caller specifies which the asset to load
  // through `preview_url` (the url is used as a persistent file identifier).
  // The `callback` is run when the image has been loaded. A nullptr may be
  // returned if loading the image failed; this usually means the preview image
  // does not exist on disc.
  void LoadOnlineWallpaperPreview(const base::FilePath& wallpaper_dir,
                                  const GURL& url,
                                  LoadPreviewImageCallback callback);

  using SaveWallpaperCallback = base::OnceCallback<void(const base::FilePath&)>;
  // Saves the wallpaper to disk then pass its original file path to the caller
  // if it is saved successfully.
  // The `callback` is run after the wallpaper is saved. The purpose of
  // the callback is to continue saving the wallpaper to DriveFS that is only
  // applicable to custom wallpapers.
  void SaveWallpaperToDisk(
      WallpaperType type,
      const base::FilePath& wallpaper_dir,
      const std::string& file_name,
      WallpaperLayout layout,
      const gfx::ImageSkia& image,
      SaveWallpaperCallback callback = base::DoNothing(),
      const std::string& wallpaper_files_id = std::string());

 private:
  void LoadFromDisk(LoadWallpaperCallback callback,
                    const base::FilePath& file_path);

  scoped_refptr<base::SequencedTaskRunner> blocking_task_runner_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<WallpaperFileManager> weak_factory_{this};
};

}  // namespace ash

#endif  // ASH_WALLPAPER_WALLPAPER_FILE_MANAGER_H_
