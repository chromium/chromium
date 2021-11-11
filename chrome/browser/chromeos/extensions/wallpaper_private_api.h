// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_EXTENSIONS_WALLPAPER_PRIVATE_API_H_
#define CHROME_BROWSER_CHROMEOS_EXTENSIONS_WALLPAPER_PRIVATE_API_H_

#include <memory>
#include <string>
#include <vector>

#include "chrome/browser/chromeos/extensions/wallpaper_function_base.h"
#include "chrome/common/extensions/api/wallpaper_private.h"
#include "components/account_id/account_id.h"

namespace backdrop {
class Collection;
class Image;
}  // namespace backdrop

namespace wallpaper_handlers {
class BackdropCollectionInfoFetcher;
class BackdropImageInfoFetcher;
class BackdropSurpriseMeImageFetcher;
}  // namespace wallpaper_handlers

// Wallpaper manager strings.
class WallpaperPrivateGetStringsFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("wallpaperPrivate.getStrings",
                             WALLPAPERPRIVATE_GETSTRINGS)

 protected:
  ~WallpaperPrivateGetStringsFunction() override {}

  // ExtensionFunction:
  ResponseAction Run() override;
};

// Check if sync themes setting is enabled.
class WallpaperPrivateGetSyncSettingFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("wallpaperPrivate.getSyncSetting",
                             WALLPAPERPRIVATE_GETSYNCSETTING)

 protected:
  ~WallpaperPrivateGetSyncSettingFunction() override {}

  // ExtensionFunction:
  ResponseAction Run() override;

 private:
  // Periodically check the sync service status until the sync service has
  // configured successfully or hit the retry limit.
  void CheckSyncServiceStatus();

  // The retry number to check to profile sync service status.
  int retry_number_ = 0;
};

class WallpaperPrivateSetWallpaperIfExistsFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("wallpaperPrivate.setWallpaperIfExists",
                             WALLPAPERPRIVATE_SETWALLPAPERIFEXISTS)

  WallpaperPrivateSetWallpaperIfExistsFunction();

  WallpaperPrivateSetWallpaperIfExistsFunction(
      const WallpaperPrivateSetWallpaperIfExistsFunction&) = delete;
  WallpaperPrivateSetWallpaperIfExistsFunction& operator=(
      const WallpaperPrivateSetWallpaperIfExistsFunction&) = delete;

 protected:
  ~WallpaperPrivateSetWallpaperIfExistsFunction() override;

  // ExtensionFunction overrides.
  ResponseAction Run() override;

 private:
  // Responds with the |file_exists| result.
  void OnSetOnlineWallpaperIfExistsCallback(bool file_exists);
};

class WallpaperPrivateSetWallpaperFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("wallpaperPrivate.setWallpaper",
                             WALLPAPERPRIVATE_SETWALLPAPER)

  WallpaperPrivateSetWallpaperFunction();

  WallpaperPrivateSetWallpaperFunction(
      const WallpaperPrivateSetWallpaperFunction&) = delete;
  WallpaperPrivateSetWallpaperFunction& operator=(
      const WallpaperPrivateSetWallpaperFunction&) = delete;

 protected:
  ~WallpaperPrivateSetWallpaperFunction() override;

  // ExtensionFunction overrides.
  ResponseAction Run() override;

 private:
  // Responds with the |success| status.
  void OnSetWallpaperCallback(bool success);
};

class WallpaperPrivateResetWallpaperFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("wallpaperPrivate.resetWallpaper",
                             WALLPAPERPRIVATE_RESETWALLPAPER)

  WallpaperPrivateResetWallpaperFunction();

 protected:
  ~WallpaperPrivateResetWallpaperFunction() override;

  // ExtensionFunction overrides.
  ResponseAction Run() override;
};

class WallpaperPrivateSetCustomWallpaperFunction
    : public WallpaperFunctionBase {
 public:
  DECLARE_EXTENSION_FUNCTION("wallpaperPrivate.setCustomWallpaper",
                             WALLPAPERPRIVATE_SETCUSTOMWALLPAPER)

  WallpaperPrivateSetCustomWallpaperFunction();

 protected:
  ~WallpaperPrivateSetCustomWallpaperFunction() override;

  // ExtensionFunction overrides.
  ResponseAction Run() override;

 private:
  void OnWallpaperDecoded(const gfx::ImageSkia& wallpaper) override;

  std::unique_ptr<
      extensions::api::wallpaper_private::SetCustomWallpaper::Params>
      params;

  // User account id of the active user when this api is been called.
  AccountId account_id_ = EmptyAccountId();
};

class WallpaperPrivateSetCustomWallpaperLayoutFunction
    : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("wallpaperPrivate.setCustomWallpaperLayout",
                             WALLPAPERPRIVATE_SETCUSTOMWALLPAPERLAYOUT)

  WallpaperPrivateSetCustomWallpaperLayoutFunction();

 protected:
  ~WallpaperPrivateSetCustomWallpaperLayoutFunction() override;

  // ExtensionFunction overrides.
  ResponseAction Run() override;
};

class WallpaperPrivateMinimizeInactiveWindowsFunction
    : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("wallpaperPrivate.minimizeInactiveWindows",
                             WALLPAPERPRIVATE_MINIMIZEINACTIVEWINDOWS)

  WallpaperPrivateMinimizeInactiveWindowsFunction();

 protected:
  ~WallpaperPrivateMinimizeInactiveWindowsFunction() override;
  ResponseAction Run() override;
};

class WallpaperPrivateRestoreMinimizedWindowsFunction
    : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("wallpaperPrivate.restoreMinimizedWindows",
                             WALLPAPERPRIVATE_RESTOREMINIMIZEDWINDOWS)

  WallpaperPrivateRestoreMinimizedWindowsFunction();

 protected:
  ~WallpaperPrivateRestoreMinimizedWindowsFunction() override;
  ResponseAction Run() override;
};

class WallpaperPrivateGetThumbnailFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("wallpaperPrivate.getThumbnail",
                             WALLPAPERPRIVATE_GETTHUMBNAIL)

  WallpaperPrivateGetThumbnailFunction();

 protected:
  ~WallpaperPrivateGetThumbnailFunction() override;

  // ExtensionFunction overrides.
  ResponseAction Run() override;

 private:
  // Failed to get thumbnail for |file_name|.
  void Failure(const std::string& file_name);

  // Returns true to suppress javascript console error. Called when the
  // requested thumbnail is not found or corrupted in thumbnail directory.
  void FileNotLoaded();

  // Sets data field to the loaded thumbnail binary data in the results. Called
  // when requested wallpaper thumbnail loaded successfully.
  void FileLoaded(const std::string& data);

  // Gets thumbnail from |path|. If |path| does not exist, call FileNotLoaded().
  void Get(const base::FilePath& path);
};

class WallpaperPrivateSaveThumbnailFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("wallpaperPrivate.saveThumbnail",
                             WALLPAPERPRIVATE_SAVETHUMBNAIL)

  WallpaperPrivateSaveThumbnailFunction();

 protected:
  ~WallpaperPrivateSaveThumbnailFunction() override;

  // ExtensionFunction overrides.
  ResponseAction Run() override;

 private:
  // Failed to save thumbnail for |file_name|.
  void Failure(const std::string& file_name);

  // Saved thumbnail to thumbnail directory.
  void Success();

  // Saves thumbnail to thumbnail directory as |file_name|.
  void Save(const std::vector<uint8_t>& data, const std::string& file_name);
};

class WallpaperPrivateGetOfflineWallpaperListFunction
    : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("wallpaperPrivate.getOfflineWallpaperList",
                             WALLPAPERPRIVATE_GETOFFLINEWALLPAPERLIST)
  WallpaperPrivateGetOfflineWallpaperListFunction();

  WallpaperPrivateGetOfflineWallpaperListFunction(
      const WallpaperPrivateGetOfflineWallpaperListFunction&) = delete;
  WallpaperPrivateGetOfflineWallpaperListFunction& operator=(
      const WallpaperPrivateGetOfflineWallpaperListFunction&) = delete;

 protected:
  ~WallpaperPrivateGetOfflineWallpaperListFunction() override;

  // ExtensionFunction overrides.
  ResponseAction Run() override;

 private:
  // Responds with the list of urls.
  void OnOfflineWallpaperListReturned(const std::vector<std::string>& url_list);
};

// The wallpaper UMA is recorded when a new wallpaper is set, either by the
// built-in Wallpaper Picker App, or by a third party App.
class WallpaperPrivateRecordWallpaperUMAFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("wallpaperPrivate.recordWallpaperUMA",
                             WALLPAPERPRIVATE_RECORDWALLPAPERUMA)

 protected:
  ~WallpaperPrivateRecordWallpaperUMAFunction() override {}

  // ExtensionFunction:
  ResponseAction Run() override;
};

class WallpaperPrivateGetCollectionsInfoFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("wallpaperPrivate.getCollectionsInfo",
                             WALLPAPERPRIVATE_GETCOLLECTIONSINFO)
  WallpaperPrivateGetCollectionsInfoFunction();

  WallpaperPrivateGetCollectionsInfoFunction(
      const WallpaperPrivateGetCollectionsInfoFunction&) = delete;
  WallpaperPrivateGetCollectionsInfoFunction& operator=(
      const WallpaperPrivateGetCollectionsInfoFunction&) = delete;

 protected:
  ~WallpaperPrivateGetCollectionsInfoFunction() override;

  // ExtensionFunction:
  ResponseAction Run() override;

 private:
  // The fetcher responsible for downloading and deserializing collections info.
  std::unique_ptr<wallpaper_handlers::BackdropCollectionInfoFetcher>
      collection_info_fetcher_;

  // Callback upon completion of fetching the collections info.
  void OnCollectionsInfoFetched(
      bool success,
      const std::vector<backdrop::Collection>& collections);
};

class WallpaperPrivateGetImagesInfoFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("wallpaperPrivate.getImagesInfo",
                             WALLPAPERPRIVATE_GETIMAGESINFO)
  WallpaperPrivateGetImagesInfoFunction();

  WallpaperPrivateGetImagesInfoFunction(
      const WallpaperPrivateGetImagesInfoFunction&) = delete;
  WallpaperPrivateGetImagesInfoFunction& operator=(
      const WallpaperPrivateGetImagesInfoFunction&) = delete;

 protected:
  ~WallpaperPrivateGetImagesInfoFunction() override;

  // ExtensionFunction:
  ResponseAction Run() override;

 private:
  // The fetcher responsible for downloading and deserializing the info of
  // images belonging to a specific collection.
  std::unique_ptr<wallpaper_handlers::BackdropImageInfoFetcher>
      image_info_fetcher_;

  // Callback upon completion of fetching the images info.
  void OnImagesInfoFetched(bool success,
                           const std::string& collection_id,
                           const std::vector<backdrop::Image>& images);
};

class WallpaperPrivateGetLocalImagePathsFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("wallpaperPrivate.getLocalImagePaths",
                             WALLPAPERPRIVATE_GETLOCALIMAGEPATHS)
  WallpaperPrivateGetLocalImagePathsFunction();

  WallpaperPrivateGetLocalImagePathsFunction(
      const WallpaperPrivateGetLocalImagePathsFunction&) = delete;
  WallpaperPrivateGetLocalImagePathsFunction& operator=(
      const WallpaperPrivateGetLocalImagePathsFunction&) = delete;

 protected:
  ~WallpaperPrivateGetLocalImagePathsFunction() override;

  // ExtensionFunction:
  ResponseAction Run() override;

 private:
  // Responds with the list of collected image paths.
  void OnGetImagePathsComplete(const std::vector<base::FilePath>& image_paths);
};

class WallpaperPrivateGetLocalImageDataFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("wallpaperPrivate.getLocalImageData",
                             WALLPAPERPRIVATE_GETLOCALIMAGEDATA)
  WallpaperPrivateGetLocalImageDataFunction();

  WallpaperPrivateGetLocalImageDataFunction(
      const WallpaperPrivateGetLocalImageDataFunction&) = delete;
  WallpaperPrivateGetLocalImageDataFunction& operator=(
      const WallpaperPrivateGetLocalImageDataFunction&) = delete;

 protected:
  ~WallpaperPrivateGetLocalImageDataFunction() override;

  // ExtensionFunction:
  ResponseAction Run() override;

 private:
  // Responds with the image data or an error message.
  void OnReadImageDataComplete(std::unique_ptr<std::string> image_data,
                               bool success);
};

class WallpaperPrivateConfirmPreviewWallpaperFunction
    : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("wallpaperPrivate.confirmPreviewWallpaper",
                             WALLPAPERPRIVATE_CONFIRMPREVIEWWALLPAPER)
  WallpaperPrivateConfirmPreviewWallpaperFunction();

  WallpaperPrivateConfirmPreviewWallpaperFunction(
      const WallpaperPrivateConfirmPreviewWallpaperFunction&) = delete;
  WallpaperPrivateConfirmPreviewWallpaperFunction& operator=(
      const WallpaperPrivateConfirmPreviewWallpaperFunction&) = delete;

 protected:
  ~WallpaperPrivateConfirmPreviewWallpaperFunction() override;

  // ExtensionFunction:
  ResponseAction Run() override;
};

class WallpaperPrivateCancelPreviewWallpaperFunction
    : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("wallpaperPrivate.cancelPreviewWallpaper",
                             WALLPAPERPRIVATE_CANCELPREVIEWWALLPAPER)
  WallpaperPrivateCancelPreviewWallpaperFunction();

  WallpaperPrivateCancelPreviewWallpaperFunction(
      const WallpaperPrivateCancelPreviewWallpaperFunction&) = delete;
  WallpaperPrivateCancelPreviewWallpaperFunction& operator=(
      const WallpaperPrivateCancelPreviewWallpaperFunction&) = delete;

 protected:
  ~WallpaperPrivateCancelPreviewWallpaperFunction() override;

  // ExtensionFunction:
  ResponseAction Run() override;
};

class WallpaperPrivateGetCurrentWallpaperThumbnailFunction
    : public WallpaperFunctionBase {
 public:
  DECLARE_EXTENSION_FUNCTION("wallpaperPrivate.getCurrentWallpaperThumbnail",
                             WALLPAPERPRIVATE_GETCURRENTWALLPAPERTHUMBNAIL)
  WallpaperPrivateGetCurrentWallpaperThumbnailFunction();

  WallpaperPrivateGetCurrentWallpaperThumbnailFunction(
      const WallpaperPrivateGetCurrentWallpaperThumbnailFunction&) = delete;
  WallpaperPrivateGetCurrentWallpaperThumbnailFunction& operator=(
      const WallpaperPrivateGetCurrentWallpaperThumbnailFunction&) = delete;

 protected:
  ~WallpaperPrivateGetCurrentWallpaperThumbnailFunction() override;

  // ExtensionFunction:
  ResponseAction Run() override;

 private:
  // WallpaperFunctionBase:
  void OnWallpaperDecoded(const gfx::ImageSkia& wallpaper) override;
};

class WallpaperPrivateGetSurpriseMeImageFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("wallpaperPrivate.getSurpriseMeImage",
                             WALLPAPERPRIVATE_GETSURPRISEMEIMAGE)
  WallpaperPrivateGetSurpriseMeImageFunction();

  WallpaperPrivateGetSurpriseMeImageFunction(
      const WallpaperPrivateGetSurpriseMeImageFunction&) = delete;
  WallpaperPrivateGetSurpriseMeImageFunction& operator=(
      const WallpaperPrivateGetSurpriseMeImageFunction&) = delete;

 protected:
  ~WallpaperPrivateGetSurpriseMeImageFunction() override;

  // ExtensionFunction:
  ResponseAction Run() override;

 private:
  // Callback upon completion of fetching the surprise me image info.
  void OnSurpriseMeImageFetched(bool success,
                                const backdrop::Image& image,
                                const std::string& next_resume_token);

  // Fetcher for the surprise me image info.
  std::unique_ptr<wallpaper_handlers::BackdropSurpriseMeImageFetcher>
      surprise_me_image_fetcher_;
};

class WallpaperPrivateIsSwaEnabledFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("wallpaperPrivate.isSwaEnabled",
                             WALLPAPERPRIVATE_ISSWAENABLED)

 protected:
  ~WallpaperPrivateIsSwaEnabledFunction() override = default;

  // ExtensionFunction:
  ResponseAction Run() override;
};

#endif  // CHROME_BROWSER_CHROMEOS_EXTENSIONS_WALLPAPER_PRIVATE_API_H_
