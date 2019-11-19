// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/customization/customization_wallpaper_util.h"

#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/location.h"
#include "base/sequenced_task_runner.h"
#include "base/task/post_task.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/customization/customization_document.h"
#include "chrome/browser/chromeos/login/users/avatar/user_image_loader.h"
#include "chrome/browser/ui/ash/wallpaper_controller_client.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/user_manager/user_image/user_image.h"
#include "content/public/browser/browser_thread.h"
#include "ui/gfx/codec/jpeg_codec.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "url/gurl.h"

namespace chromeos {

namespace {

// File path suffixes of resized wallpapers.
constexpr char kSmallWallpaperSuffix[] = "_small";
constexpr char kLargeWallpaperSuffix[] = "_large";

// Returns true if saving the resized |image| to |file_path| succeeded.
bool SaveResizedWallpaper(const gfx::ImageSkia& image,
                          const gfx::Size& size,
                          const base::FilePath& file_path) {
  gfx::ImageSkia resized_image = gfx::ImageSkiaOperations::CreateResizedImage(
      image, skia::ImageOperations::RESIZE_LANCZOS3, size);
  scoped_refptr<base::RefCountedBytes> image_data = new base::RefCountedBytes();
  gfx::JPEGCodec::Encode(*resized_image.bitmap(), 90 /*quality=*/,
                         &image_data->data());
  size_t written_bytes = base::WriteFile(
      file_path, image_data->front_as<const char>(), image_data->size());
  return written_bytes == image_data->size();
}

// Returns true if both file paths exist.
bool CheckCustomizedWallpaperFilesExist(
    const base::FilePath& resized_small_path,
    const base::FilePath& resized_large_path) {
  return base::PathExists(resized_small_path) &&
         base::PathExists(resized_large_path);
}

// Resizes and saves the customized default wallpapers.
bool ResizeAndSaveCustomizedDefaultWallpaper(
    gfx::ImageSkia image,
    const base::FilePath& resized_small_path,
    const base::FilePath& resized_large_path) {
  return SaveResizedWallpaper(image,
                              gfx::Size(ash::kSmallWallpaperMaxWidth,
                                        ash::kSmallWallpaperMaxHeight),
                              resized_small_path) &&
         SaveResizedWallpaper(image,
                              gfx::Size(ash::kLargeWallpaperMaxWidth,
                                        ash::kLargeWallpaperMaxHeight),
                              resized_large_path);
}

// Checks the result of |ResizeAndSaveCustomizedDefaultWallpaper| and sends
// the paths to apply the wallpapers.
void OnCustomizedDefaultWallpaperResizedAndSaved(
    const GURL& wallpaper_url,
    const base::FilePath& resized_small_path,
    const base::FilePath& resized_large_path,
    bool success) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!success) {
    LOG(WARNING) << "Failed to save resized customized default wallpaper";
    return;
  }

  g_browser_process->local_state()->SetString(
      prefs::kCustomizationDefaultWallpaperURL, wallpaper_url.spec());
  WallpaperControllerClient::Get()->SetCustomizedDefaultWallpaperPaths(
      resized_small_path, resized_large_path);
  VLOG(1) << "Customized default wallpaper applied.";
}

// Initiates resizing and saving the customized default wallpapers if decoding
// is successful.
void OnCustomizedDefaultWallpaperDecoded(
    const GURL& wallpaper_url,
    const base::FilePath& resized_small_path,
    const base::FilePath& resized_large_path,
    std::unique_ptr<user_manager::UserImage> wallpaper) {
  // Empty image indicates decode failure.
  if (wallpaper->image().isNull()) {
    LOG(WARNING) << "Failed to decode customized wallpaper.";
    return;
  }

  wallpaper->image().EnsureRepsForSupportedScales();

  scoped_refptr<base::SequencedTaskRunner> task_runner =
      base::CreateSequencedTaskRunner(
          {base::ThreadPool(), base::MayBlock(),
           base::TaskPriority::USER_BLOCKING,
           base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN});
  base::PostTaskAndReplyWithResult(
      task_runner.get(), FROM_HERE,
      base::Bind(&ResizeAndSaveCustomizedDefaultWallpaper,
                 wallpaper->image().DeepCopy(), resized_small_path,
                 resized_large_path),
      base::Bind(&OnCustomizedDefaultWallpaperResizedAndSaved, wallpaper_url,
                 resized_small_path, resized_large_path));
}

// If |both_sizes_exist| is false or the url doesn't match the current value,
// initiates image decoding, otherwise directly sends the paths.
void SetCustomizedDefaultWallpaperAfterCheck(
    const GURL& wallpaper_url,
    const base::FilePath& file_path,
    const base::FilePath& resized_small_path,
    const base::FilePath& resized_large_path,
    bool both_sizes_exist) {
  const std::string current_url = g_browser_process->local_state()->GetString(
      prefs::kCustomizationDefaultWallpaperURL);
  if (both_sizes_exist && current_url == wallpaper_url.spec()) {
    WallpaperControllerClient::Get()->SetCustomizedDefaultWallpaperPaths(
        resized_small_path, resized_small_path);
  } else {
    // Either resized images do not exist or cached version is incorrect.
    // Need to start decoding again.
    scoped_refptr<base::SequencedTaskRunner> task_runner =
        base::CreateSequencedTaskRunner(
            {base::ThreadPool(), base::MayBlock(),
             base::TaskPriority::USER_BLOCKING,
             base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN});
    user_image_loader::StartWithFilePath(
        task_runner, file_path, ImageDecoder::ROBUST_JPEG_CODEC,
        0,  // Do not crop.
        base::Bind(&OnCustomizedDefaultWallpaperDecoded, wallpaper_url,
                   resized_small_path, resized_large_path));
  }
}

}  // namespace

namespace customization_wallpaper_util {

void StartSettingCustomizedDefaultWallpaper(const GURL& wallpaper_url,
                                            const base::FilePath& file_path) {
  // Should fail if this ever happens in tests.
  DCHECK(wallpaper_url.is_valid());
  if (!wallpaper_url.is_valid()) {
    if (!wallpaper_url.is_empty()) {
      LOG(WARNING) << "Invalid Customized Wallpaper URL '"
                   << wallpaper_url.spec() << "'";
    }
    return;
  }

  std::string downloaded_file_name = file_path.BaseName().value();
  base::FilePath resized_small_path;
  base::FilePath resized_large_path;
  if (!GetCustomizedDefaultWallpaperPaths(&resized_small_path,
                                          &resized_large_path)) {
    return;
  }

  scoped_refptr<base::SequencedTaskRunner> task_runner =
      base::CreateSequencedTaskRunner(
          {base::ThreadPool(), base::MayBlock(),
           base::TaskPriority::USER_BLOCKING,
           base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN});
  base::PostTaskAndReplyWithResult(
      task_runner.get(), FROM_HERE,
      base::Bind(&CheckCustomizedWallpaperFilesExist, resized_small_path,
                 resized_large_path),
      base::Bind(&SetCustomizedDefaultWallpaperAfterCheck, wallpaper_url,
                 file_path, resized_small_path, resized_large_path));
}

bool GetCustomizedDefaultWallpaperPaths(base::FilePath* small_path_out,
                                        base::FilePath* large_path_out) {
  const base::FilePath default_downloaded_file_name =
      ServicesCustomizationDocument::GetCustomizedWallpaperDownloadedFileName();
  const base::FilePath default_cache_dir =
      ServicesCustomizationDocument::GetCustomizedWallpaperCacheDir();
  if (default_downloaded_file_name.empty() || default_cache_dir.empty()) {
    LOG(ERROR) << "Unable to get customized default wallpaper paths.";
    return false;
  }
  const std::string file_name = default_downloaded_file_name.BaseName().value();
  *small_path_out = default_cache_dir.Append(file_name + kSmallWallpaperSuffix);
  *large_path_out = default_cache_dir.Append(file_name + kLargeWallpaperSuffix);
  return true;
}

bool ShouldUseCustomizedDefaultWallpaper() {
  PrefService* pref_service = g_browser_process->local_state();
  return !pref_service->FindPreference(prefs::kCustomizationDefaultWallpaperURL)
              ->IsDefaultValue();
}

}  // namespace customization_wallpaper_util
}  // namespace chromeos
