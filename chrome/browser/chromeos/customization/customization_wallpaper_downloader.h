// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_CUSTOMIZATION_CUSTOMIZATION_WALLPAPER_DOWNLOADER_H_
#define CHROME_BROWSER_CHROMEOS_CUSTOMIZATION_CUSTOMIZATION_WALLPAPER_DOWNLOADER_H_

#include <stddef.h>

#include <string>

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "url/gurl.h"

namespace chromeos {

// Download customized wallpaper.
// Owner of this class must provide callback, which will be called on
// finished (either successful or failed) wallpaper download.
class CustomizationWallpaperDownloader {
 public:
  // - |url_context_getter| - Context to initialize net::URLFetcher.
  // - |wallpaper_url| - wallpaper URL to download.
  // - |wallpaper_dir| - directory, where wallpaper will be downloaded
  // (it will be created).
  // - |wallpaper_downloaded_file| - full path to local file to store downloaded
  // wallpaper file. File is downloaded to temporary location
  // |wallpaper_downloaded_file| + ".tmp", so directory must be writable.
  // After download is completed, temporary file will be renamed to
  // |wallpaper_downloaded_file|.
  CustomizationWallpaperDownloader(
      const GURL& wallpaper_url,
      const base::FilePath& wallpaper_dir,
      const base::FilePath& wallpaper_downloaded_file,
      base::Callback<void(bool success, const GURL&)>
          on_wallpaper_fetch_completed);

  ~CustomizationWallpaperDownloader();

  // Start download.
  void Start();

  // This is called in tests to modify (lower) retry delay.
  void set_retry_delay_for_testing(base::TimeDelta value) {
    retry_delay_ = value;
  }

  base::TimeDelta retry_current_delay_for_testing() const {
    return retry_current_delay_;
  }

 private:
  // Start new request.
  void StartRequest();

  // Schedules retry.
  void Retry();

  // This is called when the download has finished.
  void OnSimpleLoaderComplete(base::FilePath file_path);

  // Called on UI thread.
  void OnWallpaperDirectoryCreated(std::unique_ptr<bool> success);

  // Called on UI thread.
  void OnTemporaryFileRenamed(std::unique_ptr<bool> success);

  // This loader is used to download wallpaper file.
  std::unique_ptr<network::SimpleURLLoader> simple_loader_;

  // The wallpaper URL to fetch.
  const GURL wallpaper_url_;

  // Wallpaper directory (to be created).
  const base::FilePath wallpaper_dir_;

  // Full path to local file to save downloaded wallpaper.
  const base::FilePath wallpaper_downloaded_file_;

  // Full path to temporary file to fetch downloaded wallpper.
  const base::FilePath wallpaper_temporary_file_;

  // Pending retry.
  base::OneShotTimer request_scheduled_;

  // Number of download retries (first attempt is not counted as retry).
  size_t retries_;

  // Sleep between retry requests (increasing, see Retry() method for details).
  // Non-constant value for tests.
  base::TimeDelta retry_delay_;

  // Retry delay of the last attempt. For testing only.
  base::TimeDelta retry_current_delay_;

  // Callback supplied by caller.
  base::Callback<void(bool success, const GURL&)> on_wallpaper_fetch_completed_;

  base::WeakPtrFactory<CustomizationWallpaperDownloader> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(CustomizationWallpaperDownloader);
};

}  //   namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_CUSTOMIZATION_CUSTOMIZATION_WALLPAPER_DOWNLOADER_H_
