// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_EXTENSIONS_WALLPAPER_API_H_
#define CHROME_BROWSER_CHROMEOS_EXTENSIONS_WALLPAPER_API_H_

#include <memory>

#include "chrome/browser/chromeos/extensions/wallpaper_function_base.h"
#include "chrome/common/extensions/api/wallpaper.h"
#include "components/account_id/account_id.h"
#include "net/url_request/url_request_status.h"

// Implementation of chrome.wallpaper.setWallpaper API.
// After this API being called, a jpeg encoded wallpaper will be saved to
// /home/chronos/custom_wallpaper/{resolution}/{wallpaper_files_id_}/file_name.
// The wallpaper can then persist after Chrome restart. New call to this API
// will replace the previous saved wallpaper with new one.
// Note: For security reason, the original encoded wallpaper image is not saved
// directly. It is decoded and re-encoded to jpeg format before saved to file
// system.
class WallpaperSetWallpaperFunction : public WallpaperFunctionBase {
 public:
  DECLARE_EXTENSION_FUNCTION("wallpaper.setWallpaper",
                             WALLPAPER_SETWALLPAPER)

  WallpaperSetWallpaperFunction();

 protected:
  ~WallpaperSetWallpaperFunction() override;

  // ExtensionFunction overrides.
  ResponseAction Run() override;

 private:
  void OnWallpaperDecoded(const gfx::ImageSkia& image) override;

  // Called by OnURLFetchComplete().
  void OnWallpaperFetched(bool success, const std::string& response);

  std::unique_ptr<extensions::api::wallpaper::SetWallpaper::Params> params_;

  // Unique file name of the custom wallpaper.
  std::string file_name_;

  // User id of the user who initiate this API call.
  AccountId account_id_ = EmptyAccountId();

  // Id used to identify user wallpaper files on hard drive.
  std::string wallpaper_files_id_;
};

#endif  // CHROME_BROWSER_CHROMEOS_EXTENSIONS_WALLPAPER_API_H_
