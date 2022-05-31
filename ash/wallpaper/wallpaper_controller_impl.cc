// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wallpaper/wallpaper_controller_impl.h"

#include <memory>
#include <numeric>
#include <string>
#include <utility>
#include <vector>

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/constants/ash_switches.h"
#include "ash/display/window_tree_host_manager.h"
#include "ash/login/ui/login_constants.h"
#include "ash/public/cpp/image_downloader.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/public/cpp/wallpaper/google_photos_wallpaper_params.h"
#include "ash/public/cpp/wallpaper/online_wallpaper_params.h"
#include "ash/public/cpp/wallpaper/online_wallpaper_variant.h"
#include "ash/public/cpp/wallpaper/wallpaper_controller.h"
#include "ash/public/cpp/wallpaper/wallpaper_controller_client.h"
#include "ash/public/cpp/wallpaper/wallpaper_controller_observer.h"
#include "ash/public/cpp/wallpaper/wallpaper_info.h"
#include "ash/public/cpp/wallpaper/wallpaper_types.h"
#include "ash/root_window_controller.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/shell_delegate.h"
#include "ash/style/ash_color_provider.h"
#include "ash/wallpaper/wallpaper_pref_manager.h"
#include "ash/wallpaper/wallpaper_utils/wallpaper_color_calculator.h"
#include "ash/wallpaper/wallpaper_utils/wallpaper_resizer.h"
#include "ash/wallpaper/wallpaper_view.h"
#include "ash/wallpaper/wallpaper_widget_controller.h"
#include "ash/wallpaper/wallpaper_window_state_manager.h"
#include "ash/webui/personalization_app/mojom/personalization_app.mojom.h"
#include "ash/wm/overview/overview_constants.h"
#include "ash/wm/overview/overview_controller.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "base/barrier_closure.h"
#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/check.h"
#include "base/command_line.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted_memory.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/metrics_hashes.h"
#include "base/no_destructor.h"
#include "base/numerics/safe_conversions.h"
#include "base/path_service.h"
#include "base/rand_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece_forward.h"
#include "base/strings/stringprintf.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_runner_util.h"
#include "base/task/thread_pool.h"
#include "base/values.h"
#include "components/account_id/account_id.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/user_manager/user_manager.h"
#include "components/user_manager/user_type.h"
#include "net/http/http_request_headers.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/data_decoder/public/cpp/decode_image.h"
#include "third_party/icu/source/i18n/unicode/gregocal.h"
#include "ui/compositor/compositor.h"
#include "ui/compositor/layer.h"
#include "ui/display/manager/display_manager.h"
#include "ui/display/manager/managed_display_info.h"
#include "ui/display/screen.h"
#include "ui/display/util/display_util.h"
#include "ui/gfx/codec/jpeg_codec.h"
#include "ui/gfx/color_analysis.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/views/widget/widget.h"
#include "url/gurl.h"

using color_utils::ColorProfile;
using color_utils::LumaRange;
using color_utils::SaturationRange;

using FilePathCallback = base::OnceCallback<void(const base::FilePath&)>;

namespace ash {

namespace {

// The file name of the policy wallpaper.
constexpr char kPolicyWallpaperFile[] = "policy-controlled.jpeg";

// File path suffix of resized small wallpapers.
constexpr char kSmallWallpaperSuffix[] = "_small";

// How long to wait reloading the wallpaper after the display size has changed.
constexpr base::TimeDelta kWallpaperReloadDelay = base::Milliseconds(100);

// How long to wait for resizing of the the wallpaper.
constexpr base::TimeDelta kCompositorLockTimeout = base::Milliseconds(750);

// Duration of the lock animation performed when pressing a lock button.
constexpr base::TimeDelta kLockAnimationBlurAnimationDuration =
    base::Milliseconds(100);

// Duration of the cross fade animation when loading wallpaper.
constexpr base::TimeDelta kWallpaperLoadAnimationDuration =
    base::Milliseconds(250);

// Default quality for encoding wallpaper.
constexpr int kDefaultEncodingQuality = 90;

// The color of the wallpaper if no other wallpaper images are available.
constexpr SkColor kDefaultWallpaperColor = SK_ColorGRAY;

constexpr net::NetworkTrafficAnnotationTag
    kDownloadGooglePhotoTrafficAnnotation =
        net::DefineNetworkTrafficAnnotation("wallpaper_download_google_photo",
                                            R"(
      semantics {
        sender: "ChromeOS Wallpaper Picker"
        description:
          "When the user selects a photo from their Google Photos collection, "
          "the image must be downloaded at a high enough resolution to display "
          "as a wallpaper. This request fetches that image."
        trigger: "When the user selects a Google Photo as their wallpaper, or "
                 "when that selection reaches this device from cross-device "
                 "sync."
        data: "Stored credentials for the user's Google account."
        destination: GOOGLE_OWNED_SERVICE
      }
      policy {
        cookies_allowed: NO
        setting: "N/A"
        policy_exception_justification:
          "Not implemented, considered not necessary."
      })");

// The paths of wallpaper directories.
base::FilePath& GlobalUserDataDir() {
  static base::NoDestructor<base::FilePath> dir_user_data;
  return *dir_user_data;
}

base::FilePath& GlobalChromeOSWallpapersDir() {
  static base::NoDestructor<base::FilePath> dir_chrome_os_wallpapers;
  return *dir_chrome_os_wallpapers;
}

base::FilePath& GlobalChromeOSCustomWallpapersDir() {
  static base::NoDestructor<base::FilePath> dir_chrome_os_custom_wallpapers;
  return *dir_chrome_os_custom_wallpapers;
}

base::FilePath& GlobalChromeOSGooglePhotosWallpapersDir() {
  static base::NoDestructor<base::FilePath>
      dir_chrome_os_google_photos_wallpapers;
  return *dir_chrome_os_google_photos_wallpapers;
}

void SetGlobalUserDataDir(const base::FilePath& path) {
  base::FilePath& global_path = GlobalUserDataDir();
  global_path = path;
}

void SetGlobalChromeOSWallpapersDir(const base::FilePath& path) {
  base::FilePath& global_path = GlobalChromeOSWallpapersDir();
  global_path = path;
}

void SetGlobalChromeOSGooglePhotosWallpapersDir(const base::FilePath& path) {
  base::FilePath& global_path = GlobalChromeOSGooglePhotosWallpapersDir();
  global_path = path;
}

void SetGlobalChromeOSCustomWallpapersDir(const base::FilePath& path) {
  base::FilePath& global_path = GlobalChromeOSCustomWallpapersDir();
  global_path = path;
}

base::FilePath GetUserGooglePhotosWallpaperDir(const AccountId& account_id) {
  DCHECK(account_id.HasAccountIdKey());
  return GlobalChromeOSGooglePhotosWallpapersDir().Append(
      account_id.GetAccountIdKey());
}

// Returns the appropriate wallpaper resolution for all root windows.
WallpaperControllerImpl::WallpaperResolution GetAppropriateResolution() {
  gfx::Size size = WallpaperControllerImpl::GetMaxDisplaySizeInNative();
  return (size.width() > kSmallWallpaperMaxWidth ||
          size.height() > kSmallWallpaperMaxHeight)
             ? WallpaperControllerImpl::WALLPAPER_RESOLUTION_LARGE
             : WallpaperControllerImpl::WALLPAPER_RESOLUTION_SMALL;
}

// Returns the path of the online wallpaper corresponding to |url| and
// |resolution|.
base::FilePath GetOnlineWallpaperPath(
    const std::string& url,
    WallpaperControllerImpl::WallpaperResolution resolution) {
  std::string file_name = GURL(url).ExtractFileName();
  if (resolution == WallpaperControllerImpl::WALLPAPER_RESOLUTION_SMALL) {
    file_name = base::FilePath(file_name)
                    .InsertBeforeExtension(kSmallWallpaperSuffix)
                    .value();
  }
  DCHECK(!GlobalChromeOSWallpapersDir().empty());
  return GlobalChromeOSWallpapersDir().Append(file_name);
}

// Returns wallpaper subdirectory name for current resolution.
std::string GetCustomWallpaperSubdirForCurrentResolution() {
  WallpaperControllerImpl::WallpaperResolution resolution =
      GetAppropriateResolution();
  return resolution == WallpaperControllerImpl::WALLPAPER_RESOLUTION_SMALL
             ? WallpaperControllerImpl::kSmallWallpaperSubDir
             : WallpaperControllerImpl::kLargeWallpaperSubDir;
}

// Resizes |image| to a resolution which is nearest to |preferred_width| and
// |preferred_height| while respecting the |layout| choice. Encodes the image to
// JPEG and saves to |output|. Returns true on success.
bool ResizeAndEncodeImage(const gfx::ImageSkia& image,
                          WallpaperLayout layout,
                          int preferred_width,
                          int preferred_height,
                          scoped_refptr<base::RefCountedBytes>* output) {
  int width = image.width();
  int height = image.height();
  int resized_width;
  int resized_height;
  *output = base::MakeRefCounted<base::RefCountedBytes>();

  if (layout == WALLPAPER_LAYOUT_CENTER_CROPPED) {
    // Do not resize wallpaper if it is smaller than preferred size.
    if (width < preferred_width || height < preferred_height)
      return false;

    double horizontal_ratio = static_cast<double>(preferred_width) / width;
    double vertical_ratio = static_cast<double>(preferred_height) / height;
    if (vertical_ratio > horizontal_ratio) {
      resized_width =
          base::ClampRound(static_cast<double>(width) * vertical_ratio);
      resized_height = preferred_height;
    } else {
      resized_width = preferred_width;
      resized_height =
          base::ClampRound(static_cast<double>(height) * horizontal_ratio);
    }
  } else if (layout == WALLPAPER_LAYOUT_STRETCH) {
    resized_width = preferred_width;
    resized_height = preferred_height;
  } else {
    resized_width = width;
    resized_height = height;
  }

  gfx::ImageSkia resized_image = gfx::ImageSkiaOperations::CreateResizedImage(
      image, skia::ImageOperations::RESIZE_LANCZOS3,
      gfx::Size(resized_width, resized_height));

  SkBitmap bitmap = *(resized_image.bitmap());
  gfx::JPEGCodec::Encode(bitmap, kDefaultEncodingQuality, &(*output)->data());
  return true;
}

// Resizes |image| to a resolution which is nearest to |preferred_width| and
// |preferred_height| while respecting the |layout| choice and saves the
// resized wallpaper to |path|. Returns true on success.
bool ResizeAndSaveWallpaper(const gfx::ImageSkia& image,
                            const base::FilePath& path,
                            WallpaperLayout layout,
                            int preferred_width,
                            int preferred_height) {
  if (layout == WALLPAPER_LAYOUT_CENTER) {
    if (base::PathExists(path))
      base::DeleteFile(path);
    return false;
  }
  scoped_refptr<base::RefCountedBytes> data;
  if (!ResizeAndEncodeImage(image, layout, preferred_width, preferred_height,
                            &data)) {
    return false;
  }

  // Saves |data| to |path| in local file system.
  size_t written_bytes =
      base::WriteFile(path, data->front_as<const char>(), data->size());
  return written_bytes == data->size();
}

// Creates a 1x1 solid color image.
gfx::ImageSkia CreateSolidColorWallpaper(SkColor color) {
  SkBitmap bitmap;
  bitmap.allocN32Pixels(1, 1);
  bitmap.eraseColor(color);
  return gfx::ImageSkia::CreateFrom1xBitmap(bitmap);
}

// Gets the color profiles for extracting wallpaper prominent colors.
std::vector<ColorProfile> GetProminentColorProfiles() {
  return {ColorProfile(LumaRange::DARK, SaturationRange::VIBRANT),
          ColorProfile(LumaRange::NORMAL, SaturationRange::VIBRANT),
          ColorProfile(LumaRange::LIGHT, SaturationRange::VIBRANT),
          ColorProfile(LumaRange::DARK, SaturationRange::MUTED),
          ColorProfile(LumaRange::NORMAL, SaturationRange::MUTED),
          ColorProfile(LumaRange::LIGHT, SaturationRange::MUTED)};
}

// Gets the corresponding color profile type based on the given
// |color_profile|.
ColorProfileType GetColorProfileType(ColorProfile color_profile) {
  bool vibrant = color_profile.saturation == SaturationRange::VIBRANT;
  switch (color_profile.luma) {
    case LumaRange::ANY:
      // There should be no color profiles with the ANY luma range.
      NOTREACHED();
      break;
    case LumaRange::DARK:
      return vibrant ? ColorProfileType::DARK_VIBRANT
                     : ColorProfileType::DARK_MUTED;
    case LumaRange::NORMAL:
      return vibrant ? ColorProfileType::NORMAL_VIBRANT
                     : ColorProfileType::NORMAL_MUTED;
    case LumaRange::LIGHT:
      return vibrant ? ColorProfileType::LIGHT_VIBRANT
                     : ColorProfileType::LIGHT_MUTED;
  }
  NOTREACHED();
  return ColorProfileType::DARK_MUTED;
}

// Deletes a list of wallpaper files in |file_list|.
void DeleteWallpaperInList(std::vector<base::FilePath> file_list) {
  for (const base::FilePath& path : file_list) {
    if (!base::DeletePathRecursively(path))
      LOG(ERROR) << "Failed to remove user wallpaper at " << path.value();
  }
}

// Creates all new custom wallpaper directories for |wallpaper_files_id| if they
// don't exist.
void EnsureCustomWallpaperDirectories(const std::string& wallpaper_files_id) {
  base::FilePath dir = WallpaperControllerImpl::GetCustomWallpaperDir(
                           WallpaperControllerImpl::kSmallWallpaperSubDir)
                           .Append(wallpaper_files_id);
  if (!base::PathExists(dir))
    base::CreateDirectory(dir);

  dir = WallpaperControllerImpl::GetCustomWallpaperDir(
            WallpaperControllerImpl::kLargeWallpaperSubDir)
            .Append(wallpaper_files_id);

  if (!base::PathExists(dir))
    base::CreateDirectory(dir);

  dir = WallpaperControllerImpl::GetCustomWallpaperDir(
            WallpaperControllerImpl::kOriginalWallpaperSubDir)
            .Append(wallpaper_files_id);
  if (!base::PathExists(dir))
    base::CreateDirectory(dir);
}

// Saves original custom wallpaper to |path| (absolute path) on filesystem
// and starts resizing operation of the custom wallpaper if necessary.
base::FilePath SaveCustomWallpaper(const std::string& wallpaper_files_id,
                                   const base::FilePath& original_path,
                                   WallpaperLayout layout,
                                   gfx::ImageSkia image) {
  base::DeletePathRecursively(
      WallpaperControllerImpl::GetCustomWallpaperDir(
          WallpaperControllerImpl::kOriginalWallpaperSubDir)
          .Append(wallpaper_files_id));
  base::DeletePathRecursively(
      WallpaperControllerImpl::GetCustomWallpaperDir(
          WallpaperControllerImpl::kSmallWallpaperSubDir)
          .Append(wallpaper_files_id));
  base::DeletePathRecursively(
      WallpaperControllerImpl::GetCustomWallpaperDir(
          WallpaperControllerImpl::kLargeWallpaperSubDir)
          .Append(wallpaper_files_id));
  EnsureCustomWallpaperDirectories(wallpaper_files_id);
  const std::string file_name = original_path.BaseName().value();
  const base::FilePath small_wallpaper_path =
      WallpaperControllerImpl::GetCustomWallpaperPath(
          WallpaperControllerImpl::kSmallWallpaperSubDir, wallpaper_files_id,
          file_name);
  const base::FilePath large_wallpaper_path =
      WallpaperControllerImpl::GetCustomWallpaperPath(
          WallpaperControllerImpl::kLargeWallpaperSubDir, wallpaper_files_id,
          file_name);

  // Re-encode orginal file to jpeg format and saves the result in case that
  // resized wallpaper is not generated (i.e. chrome shutdown before resized
  // wallpaper is saved).
  bool original_size_saved =
      ResizeAndSaveWallpaper(image, original_path, WALLPAPER_LAYOUT_STRETCH,
                             image.width(), image.height());
  ResizeAndSaveWallpaper(image, small_wallpaper_path, layout,
                         kSmallWallpaperMaxWidth, kSmallWallpaperMaxHeight);
  ResizeAndSaveWallpaper(image, large_wallpaper_path, layout,
                         kLargeWallpaperMaxWidth, kLargeWallpaperMaxHeight);
  return original_size_saved ? original_path : base::FilePath();
}

// Checks if kiosk app is running. Note: it returns false either when there's
// no active user (e.g. at login screen), or the active user is not kiosk.
bool IsInKioskMode() {
  absl::optional<user_manager::UserType> active_user_type =
      Shell::Get()->session_controller()->GetUserType();
  // |active_user_type| is empty when there's no active user.
  return active_user_type &&
         *active_user_type == user_manager::USER_TYPE_KIOSK_APP;
}

// Returns the currently active user session (at index 0).
const UserSession* GetActiveUserSession() {
  return Shell::Get()->session_controller()->GetUserSession(/*user index=*/0);
}

AccountId GetActiveAccountId() {
  const UserSession* const session = GetActiveUserSession();
  DCHECK(session);
  return session->user_info.account_id;
}

// Checks if |account_id| is the current active user.
bool IsActiveUser(const AccountId& account_id) {
  const UserSession* const session = GetActiveUserSession();
  return session && session->user_info.account_id == account_id;
}

// Returns the file path of the wallpaper corresponding to |url| if it exists in
// local file system, otherwise returns an empty file path.
base::FilePath GetExistingOnlineWallpaperPath(const std::string& url) {
  WallpaperControllerImpl::WallpaperResolution resolution =
      GetAppropriateResolution();
  base::FilePath wallpaper_path = GetOnlineWallpaperPath(url, resolution);
  if (base::PathExists(wallpaper_path))
    return wallpaper_path;

  // Falls back to the large wallpaper if the small one doesn't exist.
  if (resolution == WallpaperControllerImpl::WALLPAPER_RESOLUTION_SMALL) {
    wallpaper_path = GetOnlineWallpaperPath(
        url, WallpaperControllerImpl::WALLPAPER_RESOLUTION_LARGE);
    if (base::PathExists(wallpaper_path))
      return wallpaper_path;
  }
  return base::FilePath();
}

// Checks the file paths for the given online wallpaper variants. Return empty
// map if not all paths are available.
base::flat_map<std::string, base::FilePath> GetOnlineWallpaperVariantPaths(
    const std::vector<OnlineWallpaperVariant>& variants) {
  base::flat_map<std::string, base::FilePath> url_to_file_path_map;
  WallpaperControllerImpl::WallpaperResolution resolution =
      GetAppropriateResolution();

  for (const auto& variant : variants) {
    const std::string& url = variant.raw_url.spec();
    base::FilePath variant_path = GetOnlineWallpaperPath(url, resolution);
    base::FilePath large_variant_path = GetOnlineWallpaperPath(
        url, WallpaperControllerImpl::WALLPAPER_RESOLUTION_LARGE);
    if (base::PathExists(variant_path)) {
      url_to_file_path_map[url] = variant_path;
    } else if (resolution ==
                   WallpaperControllerImpl::WALLPAPER_RESOLUTION_SMALL &&
               base::PathExists(large_variant_path)) {
      // Falls back to the large wallpaper if the small one doesn't exist.
      url_to_file_path_map[url] = large_variant_path;
    } else {
      return base::flat_map<std::string, base::FilePath>();
    }
  }
  return url_to_file_path_map;
}

// Saves the online wallpaper with both large and small sizes to local file
// system.
void SaveOnlineWallpaper(const std::string& url,
                         WallpaperLayout layout,
                         gfx::ImageSkia image) {
  DCHECK(!GlobalChromeOSWallpapersDir().empty());
  if (!base::DirectoryExists(GlobalChromeOSWallpapersDir()) &&
      !base::CreateDirectory(GlobalChromeOSWallpapersDir())) {
    return;
  }
  ResizeAndSaveWallpaper(
      image,
      GetOnlineWallpaperPath(
          url, WallpaperControllerImpl::WALLPAPER_RESOLUTION_LARGE),
      layout, image.width(), image.height());
  ResizeAndSaveWallpaper(
      image,
      GetOnlineWallpaperPath(
          url, WallpaperControllerImpl::WALLPAPER_RESOLUTION_SMALL),
      WALLPAPER_LAYOUT_CENTER_CROPPED, kSmallWallpaperMaxWidth,
      kSmallWallpaperMaxHeight);
}

// Creates the google_photos directory in the local file system for caching
// Google Photos wallpapers if it does not already exist.
void EnsureGooglePhotosDirectoryExists(const AccountId& account_id) {
  auto user_directory = GetUserGooglePhotosWallpaperDir(account_id);
  if (!base::DirectoryExists(user_directory))
    base::CreateDirectory(user_directory);
}

// Implementation of |WallpaperControllerImpl::GetOfflineWallpaper|.
std::vector<std::string> GetOfflineWallpaperListImpl() {
  DCHECK(!GlobalChromeOSWallpapersDir().empty());
  std::vector<std::string> url_list;
  if (base::DirectoryExists(GlobalChromeOSWallpapersDir())) {
    base::FileEnumerator files(GlobalChromeOSWallpapersDir(),
                               /*recursive=*/false,
                               base::FileEnumerator::FILES);
    for (base::FilePath current = files.Next(); !current.empty();
         current = files.Next()) {
      // Do not add file name of small resolution wallpaper to the list.
      if (!base::EndsWith(current.BaseName().RemoveExtension().value(),
                          kSmallWallpaperSuffix,
                          base::CompareCase::SENSITIVE)) {
        url_list.push_back(current.BaseName().value());
      }
    }
  }
  return url_list;
}

// Returns true if the user's wallpaper is to be treated as ephemeral.
bool IsEphemeralUser(const AccountId& id) {
  if (user_manager::UserManager::IsInitialized()) {
    return user_manager::UserManager::Get()->IsUserNonCryptohomeDataEphemeral(
        id);
  }
  // Unit tests may not have a UserManager.
  return false;
}

// Returns the type of the user with the specified |id| or USER_TYPE_REGULAR.
user_manager::UserType GetUserType(const AccountId& id) {
  if (user_manager::UserManager::IsInitialized()) {
    if (auto* user = user_manager::UserManager::Get()->FindUser(id))
      return user->GetType();
  }
  // Unit tests may not have a UserManager.
  return user_manager::USER_TYPE_REGULAR;
}

// Gets |account_id|'s custom wallpaper at |wallpaper_path|. Falls back to the
// original custom wallpaper. Verifies that the returned path exists. If a valid
// path cannot be found, returns an empty FilePath. Must run on wallpaper
// sequenced worker thread.
base::FilePath PathWithFallback(const AccountId& account_id,
                                const WallpaperInfo& info,
                                const base::FilePath& wallpaper_path) {
  if (base::PathExists(wallpaper_path))
    return wallpaper_path;

  // Falls back to the original file if the file with correct resolution does
  // not exist. This may happen when the original custom wallpaper is small or
  // browser shutdown before resized wallpaper saved.
  base::FilePath valid_path =
      WallpaperControllerImpl::GetCustomWallpaperDir(
          WallpaperControllerImpl::kOriginalWallpaperSubDir)
          .Append(info.location);

  return base::PathExists(valid_path) ? valid_path : base::FilePath();
}

// Deletes the user-specific directory inside the Google Photos cache
// directory. Only call this by posting it to `sequenced_task_runner_` with no
// delay to ensure that file IO is called in a well defined order. This avoids
// accidentally deleting the cache immediately after creating it, etc.
void DeleteGooglePhotosCache(const AccountId& account_id) {
  // Don't bother deleting for anyone without an AccountId, since they don't
  // have a way to set Google Photos Wallpapers. Guest accounts may not be able
  // to call `AccountId::GetAccountIdKey()`, so we can't compute a path for
  // them.
  if (account_id.HasAccountIdKey()) {
    base::DeletePathRecursively(GetUserGooglePhotosWallpaperDir(account_id));
  }
}

// Fuzzes a timedelta by up to one hour into the future to prevent hotspotting.
base::TimeDelta FuzzTimeDelta(base::TimeDelta delta) {
  auto random_delay = base::Milliseconds(base::RandDouble() *
                                         base::Time::kMillisecondsPerSecond *
                                         base::Time::kSecondsPerHour);
  return delta + random_delay;
}

GURL AddDimensionsToGooglePhotosURL(GURL url) {
  // Add a string with size data to the URL to make sure we get back the correct
  // resolution image, within reason and maintaining aspect ratio. See:
  // https://developers.google.com/photos/library/guides/access-media-items
  return GURL(base::StringPrintf("%s=w%d-h%d", url.spec().c_str(),
                                 kLargeWallpaperMaxWidth,
                                 kLargeWallpaperMaxHeight));
}

void DownloadGooglePhotosImage(
    const GURL& url,
    const AccountId& account_id,
    ImageDownloader::DownloadCallback callback,
    const absl::optional<std::string>& access_token) {
  GURL url_with_dimensions = AddDimensionsToGooglePhotosURL(url);

  net::HttpRequestHeaders headers;
  if (access_token.has_value()) {
    headers.SetHeader(net::HttpRequestHeaders::kAuthorization,
                      "Bearer " + access_token.value());
  }
  ImageDownloader::Get()->Download(url_with_dimensions,
                                   kDownloadGooglePhotoTrafficAnnotation,
                                   headers, absl::nullopt, std::move(callback));
}

// Returns an appropriate ColorMode value based on the Light/Dark mode state.
OnlineWallpaperVariantInfoFetcher::ColorMode GetColorMode() {
  return Shell::Get()->ash_color_provider()->IsDarkModeEnabled()
             ? OnlineWallpaperVariantInfoFetcher::ColorMode::kDarkMode
             : OnlineWallpaperVariantInfoFetcher::ColorMode::kLightMode;
}

}  // namespace

const char WallpaperControllerImpl::kSmallWallpaperSubDir[] = "small";
const char WallpaperControllerImpl::kLargeWallpaperSubDir[] = "large";
const char WallpaperControllerImpl::kOriginalWallpaperSubDir[] = "original";

// static
std::unique_ptr<WallpaperControllerImpl> WallpaperControllerImpl::Create(
    PrefService* local_state) {
  auto online_wallpaper_variant_fetcher =
      std::make_unique<OnlineWallpaperVariantInfoFetcher>();
  auto pref_manager = WallpaperPrefManager::Create(local_state);
  return std::make_unique<WallpaperControllerImpl>(
      std::move(pref_manager), std::move(online_wallpaper_variant_fetcher));
}

WallpaperControllerImpl::WallpaperControllerImpl(
    std::unique_ptr<WallpaperPrefManager> pref_manager,
    std::unique_ptr<OnlineWallpaperVariantInfoFetcher> online_fetcher)
    : pref_manager_(std::move(pref_manager)),
      variant_info_fetcher_(std::move(online_fetcher)),
      color_profiles_(GetProminentColorProfiles()),
      wallpaper_reload_delay_(kWallpaperReloadDelay),
      sequenced_task_runner_(base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::USER_VISIBLE,
           base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN})) {
  DCHECK(!color_profiles_.empty());
  prominent_colors_ =
      std::vector<SkColor>(color_profiles_.size(), kInvalidWallpaperColor);
  Shell::Get()->window_tree_host_manager()->AddObserver(this);
  Shell::Get()->AddShellObserver(this);
  theme_observation_.Observe(ui::NativeTheme::GetInstanceForNativeUi());
}

WallpaperControllerImpl::~WallpaperControllerImpl() {
  if (current_wallpaper_)
    current_wallpaper_->RemoveObserver(this);
  if (color_calculator_)
    color_calculator_->RemoveObserver(this);
  Shell::Get()->window_tree_host_manager()->RemoveObserver(this);
  Shell::Get()->RemoveShellObserver(this);
}

// static
gfx::Size WallpaperControllerImpl::GetMaxDisplaySizeInNative() {
  // Return an empty size for test environments where the screen is null.
  if (!display::Screen::GetScreen())
    return gfx::Size();

  gfx::Size max;
  for (const auto& display : display::Screen::GetScreen()->GetAllDisplays())
    max.SetToMax(display.GetSizeInPixel());

  return max;
}

// static
base::FilePath WallpaperControllerImpl::GetCustomWallpaperPath(
    const std::string& sub_dir,
    const std::string& wallpaper_files_id,
    const std::string& file_name) {
  base::FilePath custom_wallpaper_path = GetCustomWallpaperDir(sub_dir);
  return custom_wallpaper_path.Append(wallpaper_files_id).Append(file_name);
}

// static
base::FilePath WallpaperControllerImpl::GetCustomWallpaperDir(
    const std::string& sub_dir) {
  DCHECK(!GlobalChromeOSCustomWallpapersDir().empty());
  return GlobalChromeOSCustomWallpapersDir().Append(sub_dir);
}

SkColor WallpaperControllerImpl::GetProminentColor(
    ColorProfile color_profile) const {
  ColorProfileType type = GetColorProfileType(color_profile);
  return prominent_colors_[static_cast<int>(type)];
}

gfx::ImageSkia WallpaperControllerImpl::GetWallpaper() const {
  return current_wallpaper_ ? current_wallpaper_->image() : gfx::ImageSkia();
}

WallpaperLayout WallpaperControllerImpl::GetWallpaperLayout() const {
  return current_wallpaper_ ? current_wallpaper_->wallpaper_info().layout
                            : NUM_WALLPAPER_LAYOUT;
}

WallpaperType WallpaperControllerImpl::GetWallpaperType() const {
  return current_wallpaper_ ? current_wallpaper_->wallpaper_info().type
                            : WallpaperType::kCount;
}

bool WallpaperControllerImpl::ShouldShowInitialAnimation() {
  // The slower initial animation is only applicable if:
  // 1) It's the first run after system boot, not after user sign-out.
  if (!base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kFirstExecAfterBoot)) {
    return false;
  }
  // 2) It's at the login screen.
  if (Shell::Get()->session_controller()->IsActiveUserSessionStarted() ||
      !base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kLoginManager)) {
    return false;
  }
  // 3) It's the first wallpaper being shown, not for the switching between
  //    multiple user pods.
  if (!is_first_wallpaper_)
    return false;

  return true;
}

bool WallpaperControllerImpl::CanOpenWallpaperPicker() {
  return ShouldShowWallpaperSetting() &&
         !IsActiveUserWallpaperControlledByPolicy();
}

bool WallpaperControllerImpl::HasShownAnyWallpaper() const {
  return !!current_wallpaper_;
}

void WallpaperControllerImpl::MaybeClosePreviewWallpaper() {
  if (!confirm_preview_wallpaper_callback_) {
    DCHECK(!reload_preview_wallpaper_callback_);
    return;
  }
  // May be null in tests.
  if (wallpaper_controller_client_)
    wallpaper_controller_client_->MaybeClosePreviewWallpaper();
  CancelPreviewWallpaper();
}

void WallpaperControllerImpl::ShowWallpaperImage(const gfx::ImageSkia& image,
                                                 WallpaperInfo info,
                                                 bool preview_mode,
                                                 bool always_on_top) {
  // Prevent showing other wallpapers if there is an always-on-top wallpaper.
  if (is_always_on_top_wallpaper_ && !always_on_top)
    return;

  // Ignore show wallpaper requests during preview mode. This could happen if a
  // custom wallpaper previously set on another device is being synced.
  if (confirm_preview_wallpaper_callback_ && !preview_mode)
    return;

  if (preview_mode) {
    DVLOG(1) << __func__ << " preview_mode=true";
    base::UmaHistogramBoolean("Ash.Wallpaper.Preview.Show", true);
    for (auto& observer : observers_)
      observer.OnWallpaperPreviewStarted();
  }

  // 1x1 wallpaper should be stretched to fill the entire screen.
  // (WALLPAPER_LAYOUT_TILE also serves this purpose.)
  if (image.width() == 1 && image.height() == 1)
    info.layout = WALLPAPER_LAYOUT_STRETCH;

  if (info.type == WallpaperType::kOneShot)
    info.one_shot_wallpaper = image.DeepCopy();

  VLOG(1) << "SetWallpaper: image_id=" << WallpaperResizer::GetImageId(image)
          << " layout=" << info.layout;

  if (WallpaperIsAlreadyLoaded(image, /*compare_layouts=*/true, info.layout)) {
    VLOG(1) << "Wallpaper is already loaded";
    return;
  }

  UMA_HISTOGRAM_ENUMERATION("Ash.Wallpaper.Type", info.type,
                            WallpaperType::kCount);

  // Cancel any in-flight color calculation because we have a new wallpaper.
  if (color_calculator_) {
    color_calculator_->RemoveObserver(this);
    color_calculator_.reset();
  }

  is_first_wallpaper_ = !current_wallpaper_;
  current_wallpaper_ = std::make_unique<WallpaperResizer>(
      image, GetMaxDisplaySizeInNative(), info, sequenced_task_runner_);
  current_wallpaper_->AddObserver(this);
  current_wallpaper_->StartResize();

  if (is_first_wallpaper_) {
    for (auto& observer : observers_)
      observer.OnFirstWallpaperShown();
  }

  for (auto& observer : observers_)
    observer.OnWallpaperChanging();

  wallpaper_mode_ = WALLPAPER_IMAGE;
  UpdateWallpaperForAllRootWindows(
      Shell::Get()->session_controller()->IsUserSessionBlocked());
  ++wallpaper_count_for_testing_;

  for (auto& observer : observers_)
    observer.OnWallpaperChanged();
}

void WallpaperControllerImpl::UpdateWallpaperBlurForLockState(bool blur) {
  if (!IsBlurAllowedForLockState())
    return;

  bool changed = is_wallpaper_blurred_for_lock_state_ != blur;
  // is_wallpaper_blurrred_for_lock_state_ may already be updated in
  // InstallDesktopController. Always try to update, then invoke observer
  // if something changed.
  for (auto* root_window_controller : Shell::GetAllRootWindowControllers()) {
    changed |=
        root_window_controller->wallpaper_widget_controller()->SetWallpaperBlur(
            blur ? wallpaper_constants::kLockLoginBlur
                 : wallpaper_constants::kClear,
            kLockAnimationBlurAnimationDuration);
  }

  is_wallpaper_blurred_for_lock_state_ = blur;
  if (changed) {
    for (auto& observer : observers_)
      observer.OnWallpaperBlurChanged();
  }
}

void WallpaperControllerImpl::RestoreWallpaperBlurForLockState(float blur) {
  if (!IsBlurAllowedForLockState())
    return;

  // |is_wallpaper_blurrred_for_lock_state_| may already be updated in
  // InstallDesktopController. Always try to update, then invoke observer
  // if something changed.
  for (auto* root_window_controller : Shell::GetAllRootWindowControllers()) {
    root_window_controller->wallpaper_widget_controller()->SetWallpaperBlur(
        blur, kLockAnimationBlurAnimationDuration);
  }

  DCHECK(is_wallpaper_blurred_for_lock_state_);
  is_wallpaper_blurred_for_lock_state_ = false;
  for (auto& observer : observers_)
    observer.OnWallpaperBlurChanged();
}

bool WallpaperControllerImpl::ShouldApplyShield() const {
  // Apply a shield on the wallpaper in a blocked user session or overview or in
  // tablet mode unless during wallpaper preview.
  const bool needs_shield =
      Shell::Get()->session_controller()->IsUserSessionBlocked() ||
      Shell::Get()->overview_controller()->InOverviewSession() ||
      (Shell::Get()->tablet_mode_controller()->InTabletMode() &&
       !confirm_preview_wallpaper_callback_);
  return needs_shield && !IsOneShotWallpaper();
}

bool WallpaperControllerImpl::IsBlurAllowedForLockState() const {
  return !IsDevicePolicyWallpaper() && !IsOneShotWallpaper();
}

bool WallpaperControllerImpl::SetUserWallpaperInfo(const AccountId& account_id,
                                                   const WallpaperInfo& info) {
  if (info.type != WallpaperType::kDaily &&
      info.type != WallpaperType::kOnceGooglePhotos &&
      info.type != WallpaperType::kDailyGooglePhotos) {
    update_wallpaper_timer_.Stop();
  }

  if (info.type == WallpaperType::kOnceGooglePhotos)
    StartGooglePhotosStalenessTimer();

  if (info.type != WallpaperType::kOnceGooglePhotos &&
      info.type != WallpaperType::kDailyGooglePhotos) {
    sequenced_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&DeleteGooglePhotosCache, account_id));
  }

  return pref_manager_->SetUserWallpaperInfo(account_id,
                                             IsEphemeralUser(account_id), info);
}

bool WallpaperControllerImpl::GetUserWallpaperInfo(const AccountId& account_id,
                                                   WallpaperInfo* info) const {
  return pref_manager_->GetUserWallpaperInfo(account_id,
                                             IsEphemeralUser(account_id), info);
}

bool WallpaperControllerImpl::GetWallpaperFromCache(const AccountId& account_id,
                                                    gfx::ImageSkia* image) {
  CustomWallpaperMap::const_iterator it = wallpaper_cache_map_.find(account_id);
  if (it != wallpaper_cache_map_.end() && !it->second.second.isNull()) {
    *image = it->second.second;
    return true;
  }
  return false;
}

bool WallpaperControllerImpl::GetPathFromCache(const AccountId& account_id,
                                               base::FilePath* path) {
  CustomWallpaperMap::const_iterator it = wallpaper_cache_map_.find(account_id);
  if (it != wallpaper_cache_map_.end()) {
    *path = it->second.first;
    return true;
  }
  return false;
}

void WallpaperControllerImpl::AddFirstWallpaperAnimationEndCallback(
    base::OnceClosure callback,
    aura::Window* window) {
  WallpaperWidgetController* wallpaper_widget_controller =
      RootWindowController::ForWindow(window)->wallpaper_widget_controller();
  if (!current_wallpaper_ ||
      (is_first_wallpaper_ && wallpaper_widget_controller->IsAnimating())) {
    // No wallpaper has been set, or the first wallpaper is still animating.
    wallpaper_widget_controller->AddAnimationEndCallback(std::move(callback));
  } else {
    std::move(callback).Run();
  }
}

void WallpaperControllerImpl::StartDecodeFromPath(
    const AccountId& account_id,
    const WallpaperInfo& info,
    bool show_wallpaper,
    const base::FilePath& wallpaper_path) {
  if (wallpaper_path.empty()) {
    // Fallback to default if the path is empty.
    SetDefaultWallpaperImpl(account_id, show_wallpaper, base::DoNothing());
    return;
  }

  ReadAndDecodeWallpaper(
      base::BindOnce(&WallpaperControllerImpl::OnWallpaperDecoded,
                     weak_factory_.GetWeakPtr(), account_id, wallpaper_path,
                     info, show_wallpaper),
      wallpaper_path);
}

void WallpaperControllerImpl::SetClient(WallpaperControllerClient* client) {
  wallpaper_controller_client_ = client;
  pref_manager_->SetClient(client);
  variant_info_fetcher_->SetClient(client);
}

void WallpaperControllerImpl::Init(
    const base::FilePath& user_data_path,
    const base::FilePath& chromeos_wallpapers_path,
    const base::FilePath& chromeos_custom_wallpapers_path,
    const base::FilePath& device_policy_wallpaper_path) {
  SetGlobalUserDataDir(user_data_path);
  SetGlobalChromeOSWallpapersDir(chromeos_wallpapers_path);
  SetGlobalChromeOSGooglePhotosWallpapersDir(
      chromeos_wallpapers_path.Append("google_photos/"));
  SetGlobalChromeOSCustomWallpapersDir(chromeos_custom_wallpapers_path);
  SetDevicePolicyWallpaperPath(device_policy_wallpaper_path);
}

void WallpaperControllerImpl::SetCustomWallpaper(
    const AccountId& account_id,
    const base::FilePath& file_path,
    WallpaperLayout layout,
    bool preview_mode,
    SetWallpaperCallback callback) {
  DCHECK(Shell::Get()->session_controller()->IsActiveUserSessionStarted());
  if (!CanSetUserWallpaper(account_id)) {
    // Return early to skip the work of decoding.
    std::move(callback).Run(/*success=*/false);
    return;
  }

  // Invalidate weak ptrs to cancel prior requests to set wallpaper.
  set_wallpaper_weak_factory_.InvalidateWeakPtrs();
  ReadAndDecodeWallpaper(
      base::BindOnce(&WallpaperControllerImpl::OnCustomWallpaperDecoded,
                     set_wallpaper_weak_factory_.GetWeakPtr(), account_id,
                     file_path, layout, preview_mode, std::move(callback)),
      file_path);
}

void WallpaperControllerImpl::SetCustomWallpaper(const AccountId& account_id,
                                                 const std::string& file_name,
                                                 WallpaperLayout layout,
                                                 const gfx::ImageSkia& image,
                                                 bool preview_mode) {
  DCHECK(Shell::Get()->session_controller()->IsActiveUserSessionStarted());
  if (!CanSetUserWallpaper(account_id))
    return;

  const bool is_active_user = IsActiveUser(account_id);
  if (preview_mode) {
    DCHECK(is_active_user);
    confirm_preview_wallpaper_callback_ =
        base::BindOnce(&WallpaperControllerImpl::SaveAndSetWallpaper,
                       weak_factory_.GetWeakPtr(), account_id, file_name,
                       WallpaperType::kCustomized, layout,
                       /*show_wallpaper=*/false, image);
    reload_preview_wallpaper_callback_ = base::BindRepeating(
        &WallpaperControllerImpl::ShowWallpaperImage,
        weak_factory_.GetWeakPtr(), image,
        WallpaperInfo{/*in_location=*/std::string(), layout,
                      WallpaperType::kCustomized, base::Time::Now()},
        /*preview_mode=*/true, /*always_on_top=*/false);
    // Show the preview wallpaper.
    reload_preview_wallpaper_callback_.Run();
  } else {
    SaveAndSetWallpaperWithCompletion(
        account_id, file_name, WallpaperType::kCustomized, layout,
        /*show_wallpaper=*/is_active_user, image,
        base::BindOnce(
            &WallpaperControllerImpl::SaveWallpaperToDriveFsAndSyncInfo,
            weak_factory_.GetWeakPtr(), account_id));
  }
}

void WallpaperControllerImpl::SetOnlineWallpaper(
    const OnlineWallpaperParams& params,
    SetWallpaperCallback callback) {
  DCHECK(callback);
  DCHECK(Shell::Get()->session_controller()->IsActiveUserSessionStarted());
  if (!CanSetUserWallpaper(params.account_id)) {
    std::move(callback).Run(/*success=*/false);
    return;
  }
  // Invalidate weak ptrs to cancel prior requests to set wallpaper.
  set_wallpaper_weak_factory_.InvalidateWeakPtrs();
  SetOnlineWallpaperIfExists(
      params,
      base::BindOnce(&WallpaperControllerImpl::OnAttemptSetOnlineWallpaper,
                     set_wallpaper_weak_factory_.GetWeakPtr(), params,
                     std::move(callback)));
}

void WallpaperControllerImpl::SetOnlineWallpaperIfExists(
    const OnlineWallpaperParams& params,
    SetWallpaperCallback callback) {
  DCHECK(callback);
  DCHECK(Shell::Get()->session_controller()->IsActiveUserSessionStarted());
  DVLOG(1) << __func__ << " params=" << params;

  if (!CanSetUserWallpaper(params.account_id)) {
    std::move(callback).Run(/*success=*/false);
    return;
  }

  if (params.from_user) {
    // |unit_id| is empty when set by old wallpaper picker.
    const absl::optional<uint64_t>& unit_id = params.unit_id;
    if (unit_id.has_value()) {
      const int unit_id_val = unit_id.value();
      base::UmaHistogramSparse("Ash.Wallpaper.Image", unit_id_val);
    }
    const std::string& collection_id = params.collection_id;
    // |collection_id| is empty when the wallpaper is automatically refreshed
    // by old wallpaper app.
    if (!collection_id.empty()) {
      const int collection_id_hash = base::PersistentHash(collection_id);
      base::UmaHistogramSparse("Ash.Wallpaper.Collection", collection_id_hash);
    }
  }

  if (params.variants.empty()) {
    // |params.variants| can be empty for users who use the old wallpaper
    // picker. If that's the case, just follow the old flow.
    base::PostTaskAndReplyWithResult(
        sequenced_task_runner_.get(), FROM_HERE,
        base::BindOnce(&GetExistingOnlineWallpaperPath, params.url.spec()),
        base::BindOnce(&WallpaperControllerImpl::SetOnlineWallpaperFromPath,
                       set_wallpaper_weak_factory_.GetWeakPtr(),
                       std::move(callback), params));
  } else {
    base::PostTaskAndReplyWithResult(
        sequenced_task_runner_.get(), FROM_HERE,
        base::BindOnce(&GetOnlineWallpaperVariantPaths, params.variants),
        base::BindOnce(
            &WallpaperControllerImpl::SetOnlineWallpaperFromVariantPaths,
            set_wallpaper_weak_factory_.GetWeakPtr(), std::move(callback),
            params));
  }
}

void WallpaperControllerImpl::SetOnlineWallpaperFromData(
    const OnlineWallpaperParams& params,
    const std::string& image_data,
    SetWallpaperCallback callback) {
  if (!Shell::Get()->session_controller()->IsActiveUserSessionStarted() ||
      !CanSetUserWallpaper(params.account_id)) {
    std::move(callback).Run(/*success=*/false);
    return;
  }

  image_util::DecodeImageCallback decoded_callback =
      base::BindOnce(&WallpaperControllerImpl::OnOnlineWallpaperDecoded,
                     weak_factory_.GetWeakPtr(), params, /*save_file=*/true,
                     std::move(callback));
  if (bypass_decode_for_testing_) {
    std::move(decoded_callback)
        .Run(CreateSolidColorWallpaper(kDefaultWallpaperColor));
    return;
  }
  image_util::DecodeImageData(std::move(decoded_callback), image_data);
}

void WallpaperControllerImpl::SetGooglePhotosWallpaper(
    const GooglePhotosWallpaperParams& params,
    WallpaperController::SetWallpaperCallback callback) {
  if (!features::IsWallpaperGooglePhotosIntegrationEnabled()) {
    std::move(callback).Run(false);
    return;
  }
  if (!Shell::Get()->session_controller()->IsActiveUserSessionStarted() ||
      !CanSetUserWallpaper(params.account_id)) {
    std::move(callback).Run(/*success=*/false);
    return;
  }
  set_wallpaper_weak_factory_.InvalidateWeakPtrs();

  if (params.daily_refresh_enabled) {
    // If `params.id` is empty, then we are disabling Daily Refresh, so we set
    // the currently shown wallpaper as a `WallpaperType::kGooglePhotos`
    // Wallpaper.
    if (params.id.empty()) {
      WallpaperInfo info;
      if (!GetUserWallpaperInfo(params.account_id, &info) ||
          info.type != WallpaperType::kDailyGooglePhotos) {
        LOG(ERROR) << "Failed to get wallpaper info when disabling google "
                      "photos daily refresh.";
        std::move(callback).Run(false);
        return;
      }

      update_wallpaper_timer_.Stop();
      std::move(callback).Run(true);

      info.collection_id = std::string();
      info.type = WallpaperType::kOnceGooglePhotos;
      SetUserWallpaperInfo(params.account_id, info);
      return;
    } else {
      wallpaper_controller_client_->FetchDailyGooglePhotosPhoto(
          params.account_id, params.id,
          base::BindOnce(
              &WallpaperControllerImpl::OnDailyGooglePhotosPhotoFetched,
              set_wallpaper_weak_factory_.GetWeakPtr(), params.account_id,
              params.id, std::move(callback)));
    }
  } else {
    wallpaper_controller_client_->FetchGooglePhotosPhoto(
        params.account_id, params.id,
        base::BindOnce(&WallpaperControllerImpl::OnGooglePhotosPhotoFetched,
                       set_wallpaper_weak_factory_.GetWeakPtr(), params,
                       std::move(callback)));
  }
}

std::string WallpaperControllerImpl::GetGooglePhotosDailyRefreshAlbumId(
    const AccountId& account_id) const {
  WallpaperInfo info = GetActiveUserWallpaperInfo();
  if (info.type != WallpaperType::kDailyGooglePhotos)
    return std::string();
  return info.collection_id;
}

bool WallpaperControllerImpl::SetDailyGooglePhotosWallpaperIdCache(
    const AccountId& account_id,
    const DailyGooglePhotosIdCache& ids) {
  return pref_manager_->SetDailyGooglePhotosWallpaperIdCache(account_id, ids);
}

bool WallpaperControllerImpl::GetDailyGooglePhotosWallpaperIdCache(
    const AccountId& account_id,
    DailyGooglePhotosIdCache& ids_out) const {
  return pref_manager_->GetDailyGooglePhotosWallpaperIdCache(account_id,
                                                             ids_out);
}

void WallpaperControllerImpl::SetDefaultWallpaper(
    const AccountId& account_id,
    bool show_wallpaper,
    SetWallpaperCallback callback) {
  if (!CanSetUserWallpaper(account_id)) {
    std::move(callback).Run(/*success=*/false);
    return;
  }

  update_wallpaper_timer_.Stop();

  RemoveUserWallpaper(account_id);
  if (!SetDefaultWallpaperInfo(account_id, base::Time::Now())) {
    LOG(ERROR) << "Initializing user wallpaper info fails. This should never "
                  "happen except in tests.";
  }
  if (show_wallpaper) {
    SetDefaultWallpaperImpl(account_id, /*show_wallpaper=*/true,
                            std::move(callback));
  } else {
    std::move(callback).Run(/*success=*/true);
  }
}

base::FilePath WallpaperControllerImpl::GetDefaultWallpaperPath(
    const AccountId& account_id) {
  const bool use_small =
      (GetAppropriateResolution() == WALLPAPER_RESOLUTION_SMALL);
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  const user_manager::UserType user_type = GetUserType(account_id);
  // The wallpaper is determined in the following order:
  // Guest wallpaper, child wallpaper, customized default wallpaper, and regular
  // default wallpaper.
  if (user_type == user_manager::USER_TYPE_GUEST) {
    const base::StringPiece switch_string =
        use_small ? switches::kGuestWallpaperSmall
                  : switches::kGuestWallpaperLarge;
    return command_line->GetSwitchValuePath(switch_string);
  } else if (user_type == user_manager::USER_TYPE_CHILD) {
    const base::StringPiece switch_string =
        use_small ? switches::kChildWallpaperSmall
                  : switches::kChildWallpaperLarge;
    return command_line->GetSwitchValuePath(switch_string);
  } else if (!customized_default_small_path_.empty()) {
    DCHECK(!customized_default_large_path_.empty());
    return use_small ? customized_default_small_path_
                     : customized_default_large_path_;
  } else {
    const base::StringPiece switch_string =
        use_small ? switches::kDefaultWallpaperSmall
                  : switches::kDefaultWallpaperLarge;
    return command_line->GetSwitchValuePath(switch_string);
  }
}

void WallpaperControllerImpl::SetCustomizedDefaultWallpaperPaths(
    const base::FilePath& customized_default_small_path,
    const base::FilePath& customized_default_large_path) {
  customized_default_small_path_ = customized_default_small_path;
  customized_default_large_path_ = customized_default_large_path;

  // If the current wallpaper has type `WallpaperType::kDefault`, the new
  // customized default wallpaper should be shown immediately to update the
  // screen. It shouldn't replace wallpapers of other types.
  bool show_wallpaper = (GetWallpaperType() == WallpaperType::kDefault);

  // Customized default wallpapers are subject to the same restrictions as other
  // default wallpapers, e.g. they should not be set during guest sessions.
  SetDefaultWallpaperImpl(EmptyAccountId(), show_wallpaper, base::DoNothing());
}

void WallpaperControllerImpl::SetPolicyWallpaper(const AccountId& account_id,
                                                 const std::string& data) {
  // There is no visible wallpaper in kiosk mode.
  if (IsInKioskMode())
    return;

  // Updates the screen only when the user with this account_id has logged in.
  const bool show_wallpaper = IsActiveUser(account_id);
  image_util::DecodeImageCallback callback = base::BindOnce(
      &WallpaperControllerImpl::SaveAndSetWallpaper, weak_factory_.GetWeakPtr(),
      account_id, kPolicyWallpaperFile, WallpaperType::kPolicy,
      WALLPAPER_LAYOUT_CENTER_CROPPED, show_wallpaper);

  if (bypass_decode_for_testing_) {
    std::move(callback).Run(CreateSolidColorWallpaper(kDefaultWallpaperColor));
    return;
  }
  image_util::DecodeImageData(std::move(callback), data);
}

void WallpaperControllerImpl::SetDevicePolicyWallpaperPath(
    const base::FilePath& device_policy_wallpaper_path) {
  const bool was_device_policy_wallpaper_enforced =
      !device_policy_wallpaper_path_.empty();
  device_policy_wallpaper_path_ = device_policy_wallpaper_path;
  if (ShouldSetDevicePolicyWallpaper()) {
    SetDevicePolicyWallpaper();
  } else if (was_device_policy_wallpaper_enforced &&
             device_policy_wallpaper_path.empty()) {
    // If the device wallpaper policy is cleared, the wallpaper should revert to
    // the wallpaper of the current user with the large pod in the users list in
    // the login screen. If there is no such user, use the first user in the
    // users list.
    // TODO(xdai): Get the account id from the session controller and then call
    // ShowUserWallpaper() to display it.
  }
}

bool WallpaperControllerImpl::SetThirdPartyWallpaper(
    const AccountId& account_id,
    const std::string& file_name,
    WallpaperLayout layout,
    const gfx::ImageSkia& image) {
  bool allowed_to_set_wallpaper = CanSetUserWallpaper(account_id);
  bool allowed_to_show_wallpaper = IsActiveUser(account_id);

  if (allowed_to_set_wallpaper) {
    SaveAndSetWallpaper(account_id, file_name, WallpaperType::kCustomized,
                        layout, allowed_to_show_wallpaper, image);
  }
  return allowed_to_set_wallpaper && allowed_to_show_wallpaper;
}

void WallpaperControllerImpl::ConfirmPreviewWallpaper() {
  if (!confirm_preview_wallpaper_callback_) {
    DCHECK(!reload_preview_wallpaper_callback_);
    return;
  }
  std::move(confirm_preview_wallpaper_callback_).Run();
  reload_preview_wallpaper_callback_.Reset();

  // Ensure shield is applied after confirming the preview wallpaper.
  if (ShouldApplyShield())
    RepaintWallpaper();

  for (auto& observer : observers_)
    observer.OnWallpaperPreviewEnded();
}

void WallpaperControllerImpl::CancelPreviewWallpaper() {
  if (!confirm_preview_wallpaper_callback_) {
    DCHECK(!reload_preview_wallpaper_callback_);
    return;
  }
  confirm_preview_wallpaper_callback_.Reset();
  reload_preview_wallpaper_callback_.Reset();
  ReloadWallpaper(/*clear_cache=*/false);
  for (auto& observer : observers_)
    observer.OnWallpaperPreviewEnded();
}

void WallpaperControllerImpl::UpdateCurrentWallpaperLayout(
    const AccountId& account_id,
    WallpaperLayout layout) {
  // This method has a very specific use case: the user should be active and
  // have a custom wallpaper.
  if (!IsActiveUser(account_id))
    return;

  WallpaperInfo info;
  if (!GetUserWallpaperInfo(account_id, &info) ||
      ((info.type != WallpaperType::kCustomized) &&
       (info.type != WallpaperType::kOnceGooglePhotos))) {
    return;
  }
  if (info.layout == layout)
    return;

  info.layout = layout;
  if (!SetUserWallpaperInfo(account_id, info)) {
    LOG(ERROR) << "Setting user wallpaper info fails. This should never happen "
                  "except in tests.";
  }
  ShowUserWallpaper(account_id);
}

void WallpaperControllerImpl::ShowUserWallpaper(const AccountId& account_id) {
  current_user_ = account_id;
  const user_manager::UserType user_type = GetUserType(account_id);
  if (user_type == user_manager::USER_TYPE_KIOSK_APP ||
      user_type == user_manager::USER_TYPE_ARC_KIOSK_APP) {
    return;
  }

  if (ShouldSetDevicePolicyWallpaper()) {
    SetDevicePolicyWallpaper();
    return;
  }

  WallpaperInfo info;
  if (!GetUserWallpaperInfo(account_id, &info)) {
    if (!SetDefaultWallpaperInfo(account_id, base::Time::Min()))
      return;
    GetUserWallpaperInfo(account_id, &info);
  }

  // For ephemeral users, the cache is the only place to access their wallpaper
  // because it is not saved to disk. If the image doesn't exist in cache, it
  // means the user's wallpaper type is default (i.e. the user never sets their
  // own wallpaper), and it's a bug if it's not.
  //
  // For regular users, the image will be read from disk if the cache is not
  // hit (e.g. when the first time the wallpaper is shown on login screen).
  gfx::ImageSkia user_wallpaper;
  if (GetWallpaperFromCache(account_id, &user_wallpaper)) {
    ShowWallpaperImage(user_wallpaper, info, /*preview_mode=*/false,
                       /*always_on_top=*/false);
    return;
  }

  if (info.type == WallpaperType::kDefault) {
    SetDefaultWallpaperImpl(account_id, /*show_wallpaper=*/true,
                            base::DoNothing());
    return;
  }

  if (info.type != WallpaperType::kCustomized &&
      info.type != WallpaperType::kPolicy &&
      info.type != WallpaperType::kDevice) {
    // Load wallpaper according to WallpaperInfo.
    SetWallpaperFromInfo(account_id, info, /*show_wallpaper=*/true);
    return;
  }

  base::FilePath wallpaper_path;
  if (info.type == WallpaperType::kDevice) {
    DCHECK(!device_policy_wallpaper_path_.empty());
    wallpaper_path = device_policy_wallpaper_path_;
  } else {
    std::string sub_dir = GetCustomWallpaperSubdirForCurrentResolution();
    // Wallpaper is not resized when layout is
    // WALLPAPER_LAYOUT_CENTER.
    // Original wallpaper should be used in this case.
    if (info.layout == WALLPAPER_LAYOUT_CENTER)
      sub_dir = kOriginalWallpaperSubDir;
    wallpaper_path = GetCustomWallpaperDir(sub_dir).Append(info.location);
  }

  CustomWallpaperMap::iterator it = wallpaper_cache_map_.find(account_id);
  // Do not try to load the wallpaper if the path is the same, since loading
  // could still be in progress. We ignore the existence of the image.
  if (it != wallpaper_cache_map_.end() && it->second.first == wallpaper_path)
    return;

  // Set the new path and reset the existing image - the image will be
  // added once it becomes available.
  wallpaper_cache_map_[account_id] =
      CustomWallpaperElement(wallpaper_path, gfx::ImageSkia());

  sequenced_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&PathWithFallback, account_id, info, wallpaper_path),
      base::BindOnce(&WallpaperControllerImpl::StartDecodeFromPath,
                     weak_factory_.GetWeakPtr(), account_id, info,
                     /*show_wallpaper=*/true));
}

void WallpaperControllerImpl::ShowSigninWallpaper() {
  current_user_ = EmptyAccountId();
  if (ShouldSetDevicePolicyWallpaper())
    SetDevicePolicyWallpaper();
  else
    SetDefaultWallpaperImpl(EmptyAccountId(), /*show_wallpaper=*/true,
                            base::DoNothing());
}

void WallpaperControllerImpl::ShowOneShotWallpaper(
    const gfx::ImageSkia& image) {
  const WallpaperInfo info = {/*in_location=*/std::string(),
                              WallpaperLayout::WALLPAPER_LAYOUT_STRETCH,
                              WallpaperType::kOneShot, base::Time::Now()};
  ShowWallpaperImage(image, info, /*preview_mode=*/false,
                     /*always_on_top=*/false);
}

void WallpaperControllerImpl::ShowAlwaysOnTopWallpaper(
    const base::FilePath& image_path) {
  is_always_on_top_wallpaper_ = true;
  const WallpaperInfo info = {/*in_location=*/std::string(),
                              WallpaperLayout::WALLPAPER_LAYOUT_CENTER_CROPPED,
                              WallpaperType::kOneShot, base::Time::Now()};
  ReparentWallpaper(GetWallpaperContainerId(locked_));
  ReadAndDecodeWallpaper(
      base::BindOnce(&WallpaperControllerImpl::OnAlwaysOnTopWallpaperDecoded,
                     weak_factory_.GetWeakPtr(), info),
      image_path);
}

void WallpaperControllerImpl::RemoveAlwaysOnTopWallpaper() {
  if (!is_always_on_top_wallpaper_) {
    DCHECK(!reload_always_on_top_wallpaper_callback_);
    return;
  }
  is_always_on_top_wallpaper_ = false;
  reload_always_on_top_wallpaper_callback_.Reset();
  ReparentWallpaper(GetWallpaperContainerId(locked_));
  // Forget current wallpaper data.
  current_wallpaper_.reset();
  ReloadWallpaper(/*clear_cache=*/false);
}

void WallpaperControllerImpl::RemoveUserWallpaper(const AccountId& account_id) {
  if (wallpaper_cache_map_.find(account_id) != wallpaper_cache_map_.end())
    wallpaper_cache_map_.erase(account_id);
  pref_manager_->RemoveUserWallpaperInfo(account_id);
  RemoveUserWallpaperImpl(account_id);
}

void WallpaperControllerImpl::RemovePolicyWallpaper(
    const AccountId& account_id) {
  if (!IsWallpaperControlledByPolicy(account_id))
    return;

  // Updates the screen only when the user has logged in.
  const bool show_wallpaper =
      Shell::Get()->session_controller()->IsActiveUserSessionStarted();
  // Removes the wallpaper info so that the user is no longer policy controlled,
  // otherwise setting default wallpaper is not allowed.
  pref_manager_->RemoveUserWallpaperInfo(account_id);
  SetDefaultWallpaper(account_id, show_wallpaper, base::DoNothing());
}

void WallpaperControllerImpl::GetOfflineWallpaperList(
    GetOfflineWallpaperListCallback callback) {
  base::PostTaskAndReplyWithResult(sequenced_task_runner_.get(), FROM_HERE,
                                   base::BindOnce(&GetOfflineWallpaperListImpl),
                                   std::move(callback));
}

void WallpaperControllerImpl::SetAnimationDuration(
    base::TimeDelta animation_duration) {
  animation_duration_ = animation_duration;
}

void WallpaperControllerImpl::OpenWallpaperPickerIfAllowed() {
  if (wallpaper_controller_client_ && CanOpenWallpaperPicker())
    wallpaper_controller_client_->OpenWallpaperPicker();
}

void WallpaperControllerImpl::MinimizeInactiveWindows(
    const std::string& user_id_hash) {
  if (!window_state_manager_)
    window_state_manager_ = std::make_unique<WallpaperWindowStateManager>();

  window_state_manager_->MinimizeInactiveWindows(user_id_hash);
}

void WallpaperControllerImpl::RestoreMinimizedWindows(
    const std::string& user_id_hash) {
  if (!window_state_manager_) {
    DVLOG(1) << "No minimized window state saved";
    return;
  }
  window_state_manager_->RestoreMinimizedWindows(user_id_hash);
}

void WallpaperControllerImpl::AddObserver(
    WallpaperControllerObserver* observer) {
  observers_.AddObserver(observer);
}

void WallpaperControllerImpl::RemoveObserver(
    WallpaperControllerObserver* observer) {
  observers_.RemoveObserver(observer);
}

gfx::ImageSkia WallpaperControllerImpl::GetWallpaperImage() {
  return GetWallpaper();
}

const std::vector<SkColor>& WallpaperControllerImpl::GetWallpaperColors() {
  return prominent_colors_;
}

bool WallpaperControllerImpl::IsWallpaperBlurredForLockState() const {
  return is_wallpaper_blurred_for_lock_state_;
}

bool WallpaperControllerImpl::IsActiveUserWallpaperControlledByPolicy() {
  const UserSession* const active_user_session = GetActiveUserSession();
  if (!active_user_session)
    return false;
  return IsWallpaperControlledByPolicy(
      active_user_session->user_info.account_id);
}

bool WallpaperControllerImpl::IsWallpaperControlledByPolicy(
    const AccountId& account_id) const {
  WallpaperInfo info;
  return GetUserWallpaperInfo(account_id, &info) &&
         info.type == WallpaperType::kPolicy;
}

WallpaperInfo WallpaperControllerImpl::GetActiveUserWallpaperInfo() const {
  WallpaperInfo info;
  const UserSession* const active_user_session = GetActiveUserSession();
  if (!active_user_session ||
      !GetUserWallpaperInfo(active_user_session->user_info.account_id, &info)) {
    info.location = std::string();
    info.layout = NUM_WALLPAPER_LAYOUT;
  }
  return info;
}

bool WallpaperControllerImpl::ShouldShowWallpaperSetting() {
  const UserSession* const active_user_session = GetActiveUserSession();
  if (!active_user_session)
    return false;

  // Since everything gets wiped at the end of the Public Session (and Managed
  // Guest Session), users are disallowed to set wallpaper (and other
  // personalization settings) to avoid unnecessary confusion and surprise when
  // everything resets.
  user_manager::UserType active_user_type = active_user_session->user_info.type;
  return active_user_type == user_manager::USER_TYPE_REGULAR ||
         active_user_type == user_manager::USER_TYPE_CHILD;
}

void WallpaperControllerImpl::OnDisplayConfigurationChanged() {
  gfx::Size max_display_size = GetMaxDisplaySizeInNative();
  if (current_max_display_size_ == max_display_size)
    return;

  current_max_display_size_ = max_display_size;
  if (wallpaper_mode_ == WALLPAPER_IMAGE && current_wallpaper_) {
    timer_.Stop();
    GetInternalDisplayCompositorLock();
    timer_.Start(
        FROM_HERE, wallpaper_reload_delay_,
        base::BindOnce(&WallpaperControllerImpl::ReloadWallpaper,
                       weak_factory_.GetWeakPtr(), /*clear_cache=*/false));
  }
}

void WallpaperControllerImpl::OnRootWindowAdded(aura::Window* root_window) {
  // The wallpaper hasn't been set yet.
  if (wallpaper_mode_ == WALLPAPER_NONE)
    return;

  // Handle resolution change for "built-in" images.
  gfx::Size max_display_size = GetMaxDisplaySizeInNative();
  if (current_max_display_size_ != max_display_size) {
    current_max_display_size_ = max_display_size;
    if (wallpaper_mode_ == WALLPAPER_IMAGE && current_wallpaper_)
      ReloadWallpaper(/*clear_cache=*/true);
  }

  UpdateWallpaperForRootWindow(root_window, /*lock_state_changed=*/false,
                               /*new_root=*/true);
}

void WallpaperControllerImpl::OnShellInitialized() {
  Shell::Get()->tablet_mode_controller()->AddObserver(this);
  Shell::Get()->overview_controller()->AddObserver(this);
  Shell::Get()->ash_color_provider()->AddObserver(this);
}

void WallpaperControllerImpl::OnShellDestroying() {
  Shell::Get()->tablet_mode_controller()->RemoveObserver(this);
  Shell::Get()->overview_controller()->RemoveObserver(this);
  Shell::Get()->ash_color_provider()->RemoveObserver(this);
}

void WallpaperControllerImpl::OnWallpaperResized() {
  CalculateWallpaperColors();
  compositor_lock_.reset();
}

void WallpaperControllerImpl::OnColorCalculationComplete() {
  const std::vector<SkColor> colors = color_calculator_->prominent_colors();
  color_calculator_.reset();
  // Use |WallpaperInfo::location| as the key for storing |prominent_colors_| in
  // the |kWallpaperColors| pref.
  // TODO(crbug.com/787134): The |prominent_colors_| of wallpapers with empty
  // location should be cached as well.
  // TODO(skau): This does not guarantee that the current wallpaper is the same
  // wallpaper for which the colors were calculated.
  pref_manager_->CacheProminentColors(GetActiveAccountId(), colors);
  SetProminentColors(colors);
}

void WallpaperControllerImpl::OnActiveUserSessionChanged(
    const AccountId& account_id) {
  // It is possible to switch to another user when preview is on. In this case,
  // we should close the preview and show the user's actual wallpaper.
  MaybeClosePreviewWallpaper();

  // We check to make sure the user's Google Photos wallpapers are still valid
  // here, otherwise switching back and forth between users constantly would
  // prevent us from ever checking.
  WallpaperInfo info;
  pref_manager_->GetLocalWallpaperInfo(account_id, &info);
  if (info.type == WallpaperType::kOnceGooglePhotos)
    CheckGooglePhotosStaleness(account_id, info);
}

void WallpaperControllerImpl::OnSessionStateChanged(
    session_manager::SessionState state) {
  // Replace the device policy wallpaper with a user wallpaper if necessary.
  if (IsDevicePolicyWallpaper() && !ShouldSetDevicePolicyWallpaper())
    ReloadWallpaper(/*clear_cache=*/false);

  CalculateWallpaperColors();

  locked_ = state != session_manager::SessionState::ACTIVE;

  // The wallpaper may be dimmed/blurred based on session state. The color of
  // the dimming overlay depends on the prominent color cached from a previous
  // calculation, or a default color if cache is not available. It should never
  // depend on any in-flight color calculation.
  if (wallpaper_mode_ == WALLPAPER_IMAGE &&
      (state == session_manager::SessionState::ACTIVE ||
       state == session_manager::SessionState::LOCKED ||
       state == session_manager::SessionState::LOGIN_SECONDARY)) {
    UpdateWallpaperForAllRootWindows(/*lock_state_changed=*/true);
  } else {
    // Just update container.
    ReparentWallpaper(GetWallpaperContainerId(locked_));
  }
}

void WallpaperControllerImpl::OnTabletModeStarted() {
  RepaintWallpaper();
}

void WallpaperControllerImpl::OnTabletModeEnded() {
  RepaintWallpaper();
}

void WallpaperControllerImpl::OnColorModeChanged(bool dark_mode_enabled) {
  if (!Shell::Get()->session_controller()->IsActiveUserSessionStarted())
    return;
  AccountId account_id = GetActiveAccountId();
  WallpaperInfo local_info;
  if (!pref_manager_->GetLocalWallpaperInfo(account_id, &local_info))
    return;

  switch (local_info.type) {
    case WallpaperType::kDaily:
    case WallpaperType::kOnline: {
      HandleSettingOnlineWallpaperFromWallpaperInfo(account_id, local_info);
      break;
    }
    case WallpaperType::kCustomized:
    case WallpaperType::kDefault:
    case WallpaperType::kPolicy:
    case WallpaperType::kThirdParty:
    case WallpaperType::kDevice:
    case WallpaperType::kOneShot:
    case WallpaperType::kOnceGooglePhotos:
    case WallpaperType::kDailyGooglePhotos:
    case WallpaperType::kCount:
      return;
  }
}

void WallpaperControllerImpl::OnNativeThemeUpdated(
    ui::NativeTheme* observed_theme) {
  RepaintWallpaper();
}

void WallpaperControllerImpl::OnOverviewModeWillStart() {
  // Due to visual glitches when overview mode is activated whilst wallpaper
  // preview is active (http://crbug.com/895265), cancel wallpaper preview and
  // close its front-end before toggling overview mode.
  MaybeClosePreviewWallpaper();
}

void WallpaperControllerImpl::CompositorLockTimedOut() {
  compositor_lock_.reset();
}

void WallpaperControllerImpl::OnActiveUserPrefServiceChanged(
    PrefService* pref_service) {
  if (!features::IsWallpaperWebUIEnabled())
    return;
  AccountId account_id = GetActiveAccountId();
  if (wallpaper_controller_client_->IsWallpaperSyncEnabled(account_id)) {
    pref_change_registrar_ = std::make_unique<PrefChangeRegistrar>();
    pref_change_registrar_->Init(pref_service);
    pref_change_registrar_->Add(
        prefs::kSyncableWallpaperInfo,
        base::BindRepeating(&WallpaperControllerImpl::SyncLocalAndRemotePrefs,
                            weak_factory_.GetWeakPtr(), account_id));

    WallpaperInfo local_info;
    WallpaperInfo synced_info;

    // Migrate wallpaper info to syncable prefs.
    if (!pref_manager_->GetSyncedWallpaperInfo(account_id, &synced_info) &&
        pref_manager_->GetLocalWallpaperInfo(account_id, &local_info) &&
        IsWallpaperTypeSyncable(local_info.type)) {
      if (local_info.type == WallpaperType::kCustomized) {
        base::FilePath source = GetCustomWallpaperDir(kOriginalWallpaperSubDir)
                                    .Append(local_info.location);
        SaveWallpaperToDriveFsAndSyncInfo(account_id, source);
      } else {
        pref_manager_->SetSyncedWallpaperInfo(account_id, local_info);
        wallpaper_controller_client_->MigrateCollectionIdFromChromeApp(
            account_id,
            base::BindOnce(&WallpaperController::SetDailyRefreshCollectionId,
                           weak_factory_.GetWeakPtr(), account_id));
      }
    }

    SyncLocalAndRemotePrefs(account_id);
  }

  if (IsDailyRefreshEnabled() || IsDailyGooglePhotosWallpaperSelected())
    StartDailyRefreshTimer();
  if (IsGooglePhotosWallpaperSet())
    StartGooglePhotosStalenessTimer();
}

void WallpaperControllerImpl::ShowDefaultWallpaperForTesting() {
  SetDefaultWallpaperImpl(EmptyAccountId(), /*show_wallpaper=*/true,
                          base::DoNothing());
}

void WallpaperControllerImpl::CreateEmptyWallpaperForTesting() {
  ResetProminentColors();
  current_wallpaper_.reset();
  wallpaper_mode_ = WALLPAPER_IMAGE;
  UpdateWallpaperForAllRootWindows(/*lock_state_changed=*/false);
}

void WallpaperControllerImpl::ReloadWallpaperForTesting(bool clear_cache) {
  ReloadWallpaper(clear_cache);
}

void WallpaperControllerImpl::ClearPrefChangeObserverForTesting() {
  pref_change_registrar_.reset();
}

void WallpaperControllerImpl::UpdateDailyRefreshWallpaperForTesting() {
  UpdateDailyRefreshWallpaper();
}

base::WallClockTimer&
WallpaperControllerImpl::GetUpdateWallpaperTimerForTesting() {
  return update_wallpaper_timer_;
}

void WallpaperControllerImpl::UpdateWallpaperForRootWindow(
    aura::Window* root_window,
    bool lock_state_changed,
    bool new_root) {
  DCHECK_EQ(WALLPAPER_IMAGE, wallpaper_mode_);

  auto* wallpaper_widget_controller =
      RootWindowController::ForWindow(root_window)
          ->wallpaper_widget_controller();
  float blur = wallpaper_widget_controller->GetWallpaperBlur();

  if (lock_state_changed || new_root) {
    const bool is_wallpaper_blurred_for_lock_state =
        Shell::Get()->session_controller()->IsUserSessionBlocked() &&
        IsBlurAllowedForLockState();
    if (is_wallpaper_blurred_for_lock_state_ !=
        is_wallpaper_blurred_for_lock_state) {
      is_wallpaper_blurred_for_lock_state_ =
          is_wallpaper_blurred_for_lock_state;
      for (auto& observer : observers_)
        observer.OnWallpaperBlurChanged();
    }
    const int container_id = GetWallpaperContainerId(locked_);
    wallpaper_widget_controller->Reparent(container_id);

    blur = is_wallpaper_blurred_for_lock_state
               ? wallpaper_constants::kLockLoginBlur
               : wallpaper_constants::kClear;
  }

  wallpaper_widget_controller->wallpaper_view()->ClearCachedImage();
  wallpaper_widget_controller->SetWallpaperBlur(
      blur, new_root ? base::TimeDelta() : kWallpaperLoadAnimationDuration);
}

void WallpaperControllerImpl::UpdateWallpaperForAllRootWindows(
    bool lock_state_changed) {
  for (aura::Window* root : Shell::GetAllRootWindows())
    UpdateWallpaperForRootWindow(root, lock_state_changed, /*new_root=*/false);
  current_max_display_size_ = GetMaxDisplaySizeInNative();
}

bool WallpaperControllerImpl::ReparentWallpaper(int container) {
  bool moved = false;
  for (auto* root_window_controller : Shell::GetAllRootWindowControllers()) {
    moved |= root_window_controller->wallpaper_widget_controller()->Reparent(
        container);
  }
  return moved;
}

int WallpaperControllerImpl::GetWallpaperContainerId(bool locked) {
  if (is_always_on_top_wallpaper_)
    return kShellWindowId_AlwaysOnTopWallpaperContainer;

  return locked ? kShellWindowId_LockScreenWallpaperContainer
                : kShellWindowId_WallpaperContainer;
}

void WallpaperControllerImpl::RemoveUserWallpaperImpl(
    const AccountId& account_id) {
  if (wallpaper_controller_client_) {
    wallpaper_controller_client_->GetFilesId(
        account_id,
        base::BindOnce(
            &WallpaperControllerImpl::RemoveUserWallpaperImplWithFilesId,
            weak_factory_.GetWeakPtr(), account_id));
  } else {
    LOG(ERROR) << "Failed to remove wallpaper. wallpaper_controller_client_ no "
                  "longer exists.";
  }
}

void WallpaperControllerImpl::RemoveUserWallpaperImplWithFilesId(
    const AccountId& account_id,
    const std::string& wallpaper_files_id) {
  if (wallpaper_files_id.empty())
    return;

  std::vector<base::FilePath> files_to_remove;

  // Remove small user wallpapers.
  base::FilePath wallpaper_path = GetCustomWallpaperDir(kSmallWallpaperSubDir);
  files_to_remove.push_back(wallpaper_path.Append(wallpaper_files_id));

  // Remove large user wallpapers.
  wallpaper_path = GetCustomWallpaperDir(kLargeWallpaperSubDir);
  files_to_remove.push_back(wallpaper_path.Append(wallpaper_files_id));

  // Remove original user wallpapers.
  wallpaper_path = GetCustomWallpaperDir(kOriginalWallpaperSubDir);
  files_to_remove.push_back(wallpaper_path.Append(wallpaper_files_id));

  base::ThreadPool::PostTask(
      FROM_HERE,
      {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
       base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
      base::BindOnce(&DeleteWallpaperInList, std::move(files_to_remove)));
}

void WallpaperControllerImpl::SetDefaultWallpaperImpl(
    const AccountId& account_id,
    bool show_wallpaper,
    SetWallpaperCallback callback) {
  // There is no visible wallpaper in kiosk mode.
  if (IsInKioskMode()) {
    std::move(callback).Run(/*success=*/false);
    return;
  }

  wallpaper_cache_map_.erase(account_id);

  const bool use_small =
      (GetAppropriateResolution() == WALLPAPER_RESOLUTION_SMALL);
  WallpaperLayout layout =
      use_small ? WALLPAPER_LAYOUT_CENTER : WALLPAPER_LAYOUT_CENTER_CROPPED;
  base::FilePath file_path = GetDefaultWallpaperPath(account_id);

  // We need to decode the image if there's no cache, or if the file path
  // doesn't match the cached value (i.e. the cache is outdated). Otherwise,
  // directly run the callback with the cached image.
  if (!cached_default_wallpaper_.image.isNull() &&
      cached_default_wallpaper_.file_path == file_path) {
    OnDefaultWallpaperDecoded(file_path, layout, show_wallpaper,
                              std::move(callback),
                              cached_default_wallpaper_.image);
  } else {
    ReadAndDecodeWallpaper(
        base::BindOnce(&WallpaperControllerImpl::OnDefaultWallpaperDecoded,
                       weak_factory_.GetWeakPtr(), file_path, layout,
                       show_wallpaper, std::move(callback)),
        file_path);
  }
}

bool WallpaperControllerImpl::CanSetUserWallpaper(
    const AccountId& account_id) const {
  // There is no visible wallpaper in kiosk mode.
  if (IsInKioskMode())
    return false;
  // Don't allow user wallpapers while policy is in effect.
  if (IsWallpaperControlledByPolicy(account_id))
    return false;
  return true;
}

bool WallpaperControllerImpl::WallpaperIsAlreadyLoaded(
    const gfx::ImageSkia& image,
    bool compare_layouts,
    WallpaperLayout layout) const {
  if (!current_wallpaper_)
    return false;

  // Compare layouts only if necessary.
  if (compare_layouts && layout != current_wallpaper_->wallpaper_info().layout)
    return false;

  return WallpaperResizer::GetImageId(image) ==
         current_wallpaper_->original_image_id();
}

void WallpaperControllerImpl::ReadAndDecodeWallpaper(
    image_util::DecodeImageCallback callback,
    const base::FilePath& file_path) {
  decode_requests_for_testing_.push_back(file_path);
  if (bypass_decode_for_testing_) {
    std::move(callback).Run(CreateSolidColorWallpaper(kDefaultWallpaperColor));
    return;
  }
  image_util::DecodeImageFile(std::move(callback), file_path);
}

bool WallpaperControllerImpl::SetDefaultWallpaperInfo(
    const AccountId& account_id,
    const base::Time& date) {
  const WallpaperInfo info = {/*in_location=*/std::string(),
                              WALLPAPER_LAYOUT_CENTER_CROPPED,
                              WallpaperType::kDefault, date};
  return SetUserWallpaperInfo(account_id, info);
}

void WallpaperControllerImpl::SetOnlineWallpaperFromPath(
    SetWallpaperCallback callback,
    const OnlineWallpaperParams& params,
    const base::FilePath& file_path) {
  bool file_exists = !file_path.empty();
  if (!file_exists) {
    std::move(callback).Run(false);
    return;
  }

  ReadAndDecodeWallpaper(
      base::BindOnce(&WallpaperControllerImpl::OnOnlineWallpaperDecoded,
                     set_wallpaper_weak_factory_.GetWeakPtr(), params,
                     /*save_file=*/false, std::move(callback)),
      file_path);
}

void WallpaperControllerImpl::SetOnlineWallpaperFromVariantPaths(
    SetWallpaperCallback callback,
    const OnlineWallpaperParams& params,
    const base::flat_map<std::string, base::FilePath>& url_to_file_path_map) {
  if (url_to_file_path_map.empty()) {
    std::move(callback).Run(false);
    return;
  }

  ReadAndDecodeWallpaper(
      base::BindOnce(&WallpaperControllerImpl::OnOnlineWallpaperDecoded,
                     set_wallpaper_weak_factory_.GetWeakPtr(), params,
                     /*save_file=*/false, std::move(callback)),
      url_to_file_path_map.at(params.url.spec()));
}

void WallpaperControllerImpl::OnWallpaperVariantsFetched(
    WallpaperType type,
    SetWallpaperCallback callback,
    absl::optional<OnlineWallpaperParams> params) {
  DCHECK(type == WallpaperType::kDaily || type == WallpaperType::kOnline);
  if (params) {
    SetOnlineWallpaper(*params, std::move(callback));

    // The Daily Refresh timer depends on the value of the user WallpaperInfo.
    // it after setting the wallpaper value.
    if (type == WallpaperType::kDaily)
      StartDailyRefreshTimer();
    return;
  }

  // Report that setting the wallpaper failed.
  std::move(callback).Run(false);

  // Daily wallpaper should schedule retry.
  if (type == WallpaperType::kDaily)
    OnFetchDailyWallpaperFailed();
}

void WallpaperControllerImpl::OnOnlineWallpaperDecoded(
    const OnlineWallpaperParams& params,
    bool save_file,
    SetWallpaperCallback callback,
    const gfx::ImageSkia& image) {
  bool success = !image.isNull();
  if (callback)
    std::move(callback).Run(success);
  if (!success) {
    LOG(ERROR) << "Failed to decode online wallpaper.";
    return;
  }

  if (save_file) {
    image.EnsureRepsForSupportedScales();
    gfx::ImageSkia deep_copy(image.DeepCopy());
    sequenced_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&SaveOnlineWallpaper, params.url.spec(),
                                  params.layout, deep_copy));
  }

  const bool is_active_user = IsActiveUser(params.account_id);
  if (params.preview_mode) {
    DCHECK(is_active_user);
    confirm_preview_wallpaper_callback_ = base::BindOnce(
        &WallpaperControllerImpl::SetOnlineWallpaperImpl,
        weak_factory_.GetWeakPtr(), params, image, /*show_wallpaper=*/false);
    reload_preview_wallpaper_callback_ = base::BindRepeating(
        &WallpaperControllerImpl::ShowWallpaperImage,
        weak_factory_.GetWeakPtr(), image, WallpaperInfo(params),
        /*preview_mode=*/true, /*always_on_top=*/false);
    // Show the preview wallpaper.
    reload_preview_wallpaper_callback_.Run();
  } else {
    SetOnlineWallpaperImpl(params, image, /*show_wallpaper=*/is_active_user);
  }
}

void WallpaperControllerImpl::SetOnlineWallpaperImpl(
    const OnlineWallpaperParams& params,
    const gfx::ImageSkia& image,
    bool show_wallpaper) {
  WallpaperInfo wallpaper_info = WallpaperInfo(params);
  if (!SetUserWallpaperInfo(params.account_id, wallpaper_info)) {
    LOG(ERROR) << "Setting user wallpaper info fails. This should never happen "
                  "except in tests.";
  }
  if (show_wallpaper) {
    ShowWallpaperImage(image, wallpaper_info, /*preview_mode=*/false,
                       /*always_on_top=*/false);
  }

  wallpaper_cache_map_[params.account_id] =
      CustomWallpaperElement(base::FilePath(), image);
}

void WallpaperControllerImpl::OnGooglePhotosPhotoFetched(
    GooglePhotosWallpaperParams params,
    SetWallpaperCallback callback,
    ash::personalization_app::mojom::GooglePhotosPhotoPtr photo,
    bool success) {
  // It should be impossible for us to get back a photo successfully from
  // a request that failed.
  DCHECK(success || !photo);
  // If the request failed, there's nothing to do here, since we can't update
  // the wallpaper but also don't want to delete the cache.
  if (!success) {
    std::move(callback).Run(false);
    return;
  }

  if (photo.is_null()) {
    // The photo doesn't exist, or has been deleted. If this photo is the
    // current wallpaper, we need to reset to the default.
    if (current_wallpaper_->wallpaper_info().location == params.id) {
      sequenced_task_runner_->PostTask(
          FROM_HERE,
          base::BindOnce(&DeleteGooglePhotosCache, params.account_id));
      SetDefaultWallpaperImpl(params.account_id, /*show_wallpaper=*/true,
                              base::DoNothing());
      return;
    }
    std::move(callback).Run(false);
    return;
  }

  params.dedup_key = photo->dedup_key;

  auto cached_path =
      GetUserGooglePhotosWallpaperDir(params.account_id).Append(params.id);
  sequenced_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE, base::BindOnce(&base::PathExists, cached_path),
      base::BindOnce(
          &WallpaperControllerImpl::GetGooglePhotosWallpaperFromCacheOrDownload,
          set_wallpaper_weak_factory_.GetWeakPtr(), std::move(params),
          std::move(photo), std::move(callback), cached_path));
}

void WallpaperControllerImpl::OnDailyGooglePhotosPhotoFetched(
    const AccountId& account_id,
    const std::string& album_id,
    RefreshWallpaperCallback callback,
    ash::personalization_app::mojom::GooglePhotosPhotoPtr photo,
    bool success) {
  // It should be impossible for us to get back a photo successfully from
  // a request that failed.
  DCHECK(success || !photo);
  if (!success || photo.is_null()) {
    std::move(callback).Run(false);
    WallpaperInfo info;
    if (GetUserWallpaperInfo(account_id, &info) &&
        info.collection_id == album_id) {
      if (success) {
        // If the request succeeded, but no photos came back, then the album is
        // empty or deleted. Reset to default as a fallback.
        SetDefaultWallpaper(account_id, /*show_wallpaper=*/true,
                            base::DoNothing());
      } else {
        // If the request simply failed, retry in an hour.
        StartUpdateWallpaperTimer(base::Hours(1));
      }
    }
    return;
  }

  ImageDownloader::DownloadCallback download_callback = base::BindOnce(
      &WallpaperControllerImpl::OnDailyGooglePhotosWallpaperDownloaded,
      set_wallpaper_weak_factory_.GetWeakPtr(), account_id, photo->id, album_id,
      photo->dedup_key, std::move(callback));
  wallpaper_controller_client_->FetchGooglePhotosAccessToken(
      account_id, base::BindOnce(&DownloadGooglePhotosImage, photo->url,
                                 account_id, std::move(download_callback)));
}

void WallpaperControllerImpl::OnDailyGooglePhotosWallpaperDownloaded(
    const AccountId& account_id,
    const std::string& photo_id,
    const std::string& album_id,
    absl::optional<std::string> dedup_key,
    RefreshWallpaperCallback callback,
    const gfx::ImageSkia& image) {
  DCHECK(callback);
  if (image.isNull()) {
    std::move(callback).Run(false);
    return;
  }
  // Image returned successfully. We can reliably assume success from here, and
  // we need to call the callback before `ShowWallpaperImage()` to ensure proper
  // propagation of `CurrentWallpaper` to the WebUI.
  std::move(callback).Run(true);

  WallpaperInfo wallpaper_info(
      {account_id, album_id, /*daily_refresh_enabled=*/true,
       ash::WallpaperLayout::WALLPAPER_LAYOUT_CENTER_CROPPED,
       /*preview_mode=*/false, /*dedup_key=*/absl::nullopt});
  wallpaper_info.location = photo_id;
  wallpaper_info.dedup_key = dedup_key;

  if (!SetUserWallpaperInfo(account_id, wallpaper_info)) {
    LOG(ERROR) << "Setting user wallpaper info fails. This should never happen "
                  "except in tests.";
  }

  StartDailyRefreshTimer();

  sequenced_task_runner_->PostTaskAndReply(
      FROM_HERE, base::BindOnce(&DeleteGooglePhotosCache, account_id),
      base::BindOnce(
          &WallpaperControllerImpl::SetGooglePhotosWallpaperAndUpdateCache,
          set_wallpaper_weak_factory_.GetWeakPtr(), account_id, wallpaper_info,
          image, /*show_wallpaper=*/true));
}

void WallpaperControllerImpl::GetGooglePhotosWallpaperFromCacheOrDownload(
    const GooglePhotosWallpaperParams& params,
    ash::personalization_app::mojom::GooglePhotosPhotoPtr photo,
    SetWallpaperCallback callback,
    const base::FilePath& cached_path,
    bool cached_path_exists) {
  if (cached_path_exists) {
    ReadAndDecodeWallpaper(
        base::BindOnce(&WallpaperControllerImpl::OnGooglePhotosWallpaperDecoded,
                       set_wallpaper_weak_factory_.GetWeakPtr(),
                       WallpaperInfo(params), params.account_id, cached_path,
                       std::move(callback)),
        cached_path);
  } else {
    ImageDownloader::DownloadCallback download_callback = base::BindOnce(
        &WallpaperControllerImpl::OnGooglePhotosWallpaperDownloaded,
        set_wallpaper_weak_factory_.GetWeakPtr(), params, std::move(callback));
    wallpaper_controller_client_->FetchGooglePhotosAccessToken(
        params.account_id,
        base::BindOnce(&DownloadGooglePhotosImage, photo->url,
                       params.account_id, std::move(download_callback)));
  }
}

void WallpaperControllerImpl::OnGooglePhotosWallpaperDecoded(
    const WallpaperInfo& info,
    const AccountId& account_id,
    const base::FilePath& path,
    SetWallpaperCallback callback,
    const gfx::ImageSkia& image) {
  std::move(callback).Run(!image.isNull());
  OnWallpaperDecoded(account_id, path, info, /*show_wallpaper=*/true, image);
}

void WallpaperControllerImpl::OnGooglePhotosWallpaperDownloaded(
    const GooglePhotosWallpaperParams& params,
    SetWallpaperCallback callback,
    const gfx::ImageSkia& image) {
  DCHECK(callback);
  if (image.isNull()) {
    std::move(callback).Run(false);
    return;
  }
  // Image returned successfully. We can reliably assume success from here, and
  // we need to call the callback before `ShowWallpaperImage` to ensure proper
  // propagation of `CurrentWallpaper` to the WebUI.
  std::move(callback).Run(true);

  bool is_active_user = IsActiveUser(params.account_id);
  WallpaperInfo wallpaper_info(params);
  if (params.preview_mode) {
    DCHECK(is_active_user);
    confirm_preview_wallpaper_callback_ = base::BindOnce(
        &WallpaperControllerImpl::SetGooglePhotosWallpaperAndUpdateCache,
        weak_factory_.GetWeakPtr(), params.account_id, wallpaper_info, image,
        /*show_wallpaper=*/false);
    reload_preview_wallpaper_callback_ =
        base::BindRepeating(&WallpaperControllerImpl::ShowWallpaperImage,
                            weak_factory_.GetWeakPtr(), image, wallpaper_info,
                            /*preview_mode=*/true, /*always_on_top=*/false);

    // Show the preview wallpaper.
    reload_preview_wallpaper_callback_.Run();
  } else {
    SetGooglePhotosWallpaperAndUpdateCache(params.account_id, wallpaper_info,
                                           image,
                                           /*show_wallpaper=*/is_active_user);
  }
}

void WallpaperControllerImpl::SetGooglePhotosWallpaperAndUpdateCache(
    const AccountId& account_id,
    const WallpaperInfo& wallpaper_info,
    const gfx::ImageSkia& image,
    bool show_wallpaper) {
  if (!SetUserWallpaperInfo(account_id, wallpaper_info)) {
    LOG(ERROR) << "Setting user wallpaper info fails. This should never happen "
                  "except in tests.";
  }

  if (show_wallpaper) {
    ShowWallpaperImage(image, wallpaper_info, /*preview_mode=*/false,
                       /*always_on_top=*/false);
  }

  // Add current Google Photos wallpaper to in-memory cache.
  wallpaper_cache_map_[account_id] =
      CustomWallpaperElement(base::FilePath(), image);

  // Clear persistent cache and repopulate with current Google Photos wallpaper.
  gfx::ImageSkia thread_safe_image(image);
  thread_safe_image.MakeThreadSafe();
  auto path = GetUserGooglePhotosWallpaperDir(account_id)
                  .Append(wallpaper_info.location);
  sequenced_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&DeleteGooglePhotosCache, account_id)
          .Then(base::BindOnce(&EnsureGooglePhotosDirectoryExists, account_id))
          .Then(base::BindOnce(&ResizeAndSaveWallpaper, thread_safe_image, path,
                               wallpaper_info.layout, thread_safe_image.width(),
                               thread_safe_image.height())),
      base::BindOnce([](bool success) {
        if (!success)
          LOG(ERROR) << "Failed to save Google Photos wallpaper.";
      }));
}

void WallpaperControllerImpl::SetWallpaperFromInfo(const AccountId& account_id,
                                                   const WallpaperInfo& info,
                                                   bool show_wallpaper) {
  if (info.type != WallpaperType::kOnline &&
      info.type != WallpaperType::kDaily &&
      info.type != WallpaperType::kOnceGooglePhotos &&
      info.type != WallpaperType::kDailyGooglePhotos &&
      info.type != WallpaperType::kDefault) {
    // This method is meant to be used for `WallpaperType::kOnline` and
    // `WallpaperType::kDefault` types. In unexpected cases, revert to default
    // wallpaper to fail safely. See crosbug.com/38429.
    LOG(ERROR) << "Wallpaper reverts to default unexpected.";
    SetDefaultWallpaperImpl(account_id, show_wallpaper, base::DoNothing());
    return;
  }

  // Do a sanity check that the file path is not empty.
  if (info.location.empty()) {
    // File name might be empty on debug configurations when stub users
    // were created directly in local state (for testing). Ignore such
    // errors i.e. allow such type of debug configurations on the desktop.
    LOG(WARNING) << "User wallpaper info is empty: " << account_id.Serialize();
    SetDefaultWallpaperImpl(account_id, show_wallpaper, base::DoNothing());
    return;
  }

  base::FilePath wallpaper_path;
  if (info.type == WallpaperType::kOnline ||
      info.type == WallpaperType::kDaily) {
    wallpaper_path =
        GetOnlineWallpaperPath(info.location, GetAppropriateResolution());

    // If the wallpaper exists and it already contains the correct image we
    // can return immediately.
    CustomWallpaperMap::iterator it = wallpaper_cache_map_.find(account_id);
    if (it != wallpaper_cache_map_.end() &&
        it->second.first == wallpaper_path && !it->second.second.isNull())
      return;

    ReadAndDecodeWallpaper(
        base::BindOnce(&WallpaperControllerImpl::OnWallpaperDecoded,
                       weak_factory_.GetWeakPtr(), account_id, wallpaper_path,
                       info, show_wallpaper),
        wallpaper_path);
  } else if (info.type == WallpaperType::kOnceGooglePhotos ||
             info.type == WallpaperType::kDailyGooglePhotos) {
    auto path =
        GetUserGooglePhotosWallpaperDir(account_id).Append(info.location);
    ReadAndDecodeWallpaper(
        base::BindOnce(&WallpaperControllerImpl::OnGooglePhotosWallpaperDecoded,
                       set_wallpaper_weak_factory_.GetWeakPtr(), info,
                       account_id, path, base::DoNothing()),
        path);
    return;
  } else {
    // Default wallpapers are migrated from M21 user profiles. A code
    // refactor overlooked that case and caused these wallpapers not being
    // loaded at all. On some slow devices, it caused login webui not
    // visible after upgrade to M26 from M21. See crosbug.com/38429 for
    // details.
    DCHECK(!GlobalUserDataDir().empty());
    wallpaper_path = GlobalUserDataDir().Append(info.location);

    ReadAndDecodeWallpaper(
        base::BindOnce(&WallpaperControllerImpl::OnWallpaperDecoded,
                       weak_factory_.GetWeakPtr(), account_id, wallpaper_path,
                       info, show_wallpaper),
        wallpaper_path);
  }
}

void WallpaperControllerImpl::OnDefaultWallpaperDecoded(
    const base::FilePath& path,
    WallpaperLayout layout,
    bool show_wallpaper,
    SetWallpaperCallback callback,
    const gfx::ImageSkia& image) {
  if (image.isNull()) {
    // Create a solid color wallpaper if the default wallpaper decoding fails.
    cached_default_wallpaper_.image =
        CreateSolidColorWallpaper(kDefaultWallpaperColor);
    cached_default_wallpaper_.file_path.clear();
  } else {
    cached_default_wallpaper_.image = image;
    cached_default_wallpaper_.file_path = path;
  }

  // Setting default wallpaper always succeeds even if the intended image failed
  // decoding. Run the callback before doing the final step of showing the
  // wallpaper to be consistent with other wallpaper controller methods.
  std::move(callback).Run(/*success=*/true);

  if (show_wallpaper) {
    WallpaperInfo info(cached_default_wallpaper_.file_path.value(), layout,
                       WallpaperType::kDefault, base::Time::Now());
    ShowWallpaperImage(cached_default_wallpaper_.image, info,
                       /*preview_mode=*/false, /*always_on_top=*/false);
  }
}

void WallpaperControllerImpl::SaveAndSetWallpaper(const AccountId& account_id,
                                                  const std::string& file_name,
                                                  WallpaperType type,
                                                  WallpaperLayout layout,
                                                  bool show_wallpaper,
                                                  const gfx::ImageSkia& image) {
  SaveAndSetWallpaperWithCompletion(account_id, file_name, type, layout,
                                    show_wallpaper, image, base::DoNothing());
}

void WallpaperControllerImpl::SaveAndSetWallpaperWithCompletion(
    const AccountId& account_id,
    const std::string& file_name,
    WallpaperType type,
    WallpaperLayout layout,
    bool show_wallpaper,
    const gfx::ImageSkia& image,
    FilePathCallback image_saved_callback) {
  if (wallpaper_controller_client_) {
    wallpaper_controller_client_->GetFilesId(
        account_id,
        base::BindOnce(
            &WallpaperControllerImpl::SaveAndSetWallpaperWithCompletionFilesId,
            weak_factory_.GetWeakPtr(), account_id, file_name, type, layout,
            show_wallpaper, image, std::move(image_saved_callback)));
  }
}

void WallpaperControllerImpl::SaveAndSetWallpaperWithCompletionFilesId(
    const AccountId& account_id,
    const std::string& file_name,
    WallpaperType type,
    WallpaperLayout layout,
    bool show_wallpaper,
    const gfx::ImageSkia& image,
    FilePathCallback image_saved_callback,
    const std::string& wallpaper_files_id) {
  // If the image of the new wallpaper is empty, the current wallpaper is still
  // kept instead of reverting to the default.
  if (image.isNull()) {
    LOG(ERROR) << "The wallpaper image is empty due to a decoding failure, or "
                  "the client provided an empty image.";
    return;
  }

  const std::string relative_path =
      base::FilePath(wallpaper_files_id).Append(file_name).value();
  // User's custom wallpaper path is determined by relative path and the
  // appropriate wallpaper resolution.
  WallpaperInfo info = {relative_path, layout, type, base::Time::Now()};
  if (!SetUserWallpaperInfo(account_id, info)) {
    LOG(ERROR) << "Setting user wallpaper info fails. This should never happen "
                  "except in tests.";
  }

  base::FilePath wallpaper_path =
      GetCustomWallpaperPath(WallpaperControllerImpl::kOriginalWallpaperSubDir,
                             wallpaper_files_id, file_name);

  const bool should_save_to_disk =
      !IsEphemeralUser(account_id) ||
      (type == WallpaperType::kPolicy &&
       GetUserType(account_id) == user_manager::USER_TYPE_PUBLIC_ACCOUNT);

  if (should_save_to_disk) {
    image.EnsureRepsForSupportedScales();
    gfx::ImageSkia deep_copy(image.DeepCopy());
    // Block shutdown on this task. Otherwise, we may lose the custom wallpaper
    // that the user selected.
    scoped_refptr<base::SequencedTaskRunner> blocking_task_runner =
        base::ThreadPool::CreateSequencedTaskRunner(
            {base::MayBlock(), base::TaskPriority::USER_BLOCKING,
             base::TaskShutdownBehavior::BLOCK_SHUTDOWN});
    blocking_task_runner->PostTaskAndReplyWithResult(
        FROM_HERE,
        base::BindOnce(&SaveCustomWallpaper, wallpaper_files_id, wallpaper_path,
                       layout, deep_copy),
        std::move(image_saved_callback));
  }

  if (show_wallpaper) {
    ShowWallpaperImage(image, info, /*preview_mode=*/false,
                       /*always_on_top=*/false);
  }

  wallpaper_cache_map_[account_id] =
      CustomWallpaperElement(wallpaper_path, image);
}

void WallpaperControllerImpl::OnCustomWallpaperDecoded(
    const AccountId& account_id,
    const base::FilePath& path,
    WallpaperLayout layout,
    bool preview_mode,
    SetWallpaperCallback callback,
    const gfx::ImageSkia& image) {
  bool success = !image.isNull();
  // Run callback before finishing setting the image. This is the same timing of
  // success callback, then |WallpaperControllerObserver::OnWallpaperChanged|,
  // when setting online wallpaper and simplifies the logic in observers.
  std::move(callback).Run(success);
  if (success) {
    SetCustomWallpaper(account_id, path.BaseName().value(), layout, image,
                       preview_mode);
  }
}

void WallpaperControllerImpl::OnWallpaperDecoded(const AccountId& account_id,
                                                 const base::FilePath& path,
                                                 const WallpaperInfo& info,
                                                 bool show_wallpaper,
                                                 const gfx::ImageSkia& image) {
  // Empty image indicates decode failure. Use default wallpaper in this case.
  if (image.isNull()) {
    LOG(ERROR) << "Failed to decode user wallpaper at " << path.value()
               << " Falls back to default wallpaper. ";
    SetDefaultWallpaperImpl(account_id, show_wallpaper, base::DoNothing());
    return;
  }

  wallpaper_cache_map_[account_id] = CustomWallpaperElement(path, image);
  if (show_wallpaper) {
    ShowWallpaperImage(image, info, /*preview_mode=*/false,
                       /*always_on_top=*/false);
  }
}

void WallpaperControllerImpl::ReloadWallpaper(bool clear_cache) {
  const bool was_one_shot_wallpaper = IsOneShotWallpaper();
  const gfx::ImageSkia one_shot_wallpaper =
      was_one_shot_wallpaper
          ? current_wallpaper_->wallpaper_info().one_shot_wallpaper
          : gfx::ImageSkia();
  current_wallpaper_.reset();
  if (clear_cache)
    wallpaper_cache_map_.clear();

  if (reload_always_on_top_wallpaper_callback_)
    reload_always_on_top_wallpaper_callback_.Run();
  else if (reload_preview_wallpaper_callback_)
    reload_preview_wallpaper_callback_.Run();
  else if (current_user_.is_valid())
    ShowUserWallpaper(current_user_);
  else if (was_one_shot_wallpaper)
    ShowOneShotWallpaper(one_shot_wallpaper);
  else
    ShowSigninWallpaper();
}

void WallpaperControllerImpl::SetProminentColors(
    const std::vector<SkColor>& colors) {
  if (prominent_colors_ == colors)
    return;

  prominent_colors_ = colors;
  for (auto& observer : observers_)
    observer.OnWallpaperColorsChanged();
}

void WallpaperControllerImpl::ResetProminentColors() {
  static const std::vector<SkColor> kInvalidColors(color_profiles_.size(),
                                                   kInvalidWallpaperColor);
  SetProminentColors(kInvalidColors);
}

void WallpaperControllerImpl::CalculateWallpaperColors() {
  if (!current_wallpaper_)
    return;

  // Cancel any in-flight color calculation.
  if (color_calculator_) {
    color_calculator_->RemoveObserver(this);
    color_calculator_.reset();
  }

  if (GetActiveUserSession()) {
    // The cache is only available if we have an active session.
    // Fetch the color cache if it exists.
    absl::optional<std::vector<SkColor>> cached_colors =
        pref_manager_->GetCachedColors(GetActiveAccountId());
    if (cached_colors.has_value()) {
      SetProminentColors(cached_colors.value());
      return;
    }
  }

  // Color calculation is only allowed during an active session for performance
  // reasons. Observers outside an active session are notified of the cache, or
  // an invalid color if a previous calculation during active session failed.
  if (!ShouldCalculateColors()) {
    ResetProminentColors();
    return;
  }

  color_calculator_ = std::make_unique<WallpaperColorCalculator>(
      GetWallpaper(), color_profiles_, sequenced_task_runner_);
  color_calculator_->AddObserver(this);
  if (!color_calculator_->StartCalculation()) {
    ResetProminentColors();
  }
}

bool WallpaperControllerImpl::ShouldCalculateColors() const {
  gfx::ImageSkia image = GetWallpaper();
  return Shell::Get()->session_controller()->GetSessionState() ==
             session_manager::SessionState::ACTIVE &&
         !image.isNull();
}

void WallpaperControllerImpl::OnAlwaysOnTopWallpaperDecoded(
    const WallpaperInfo& info,
    const gfx::ImageSkia& image) {
  // Do nothing if |RemoveAlwaysOnTopWallpaper| was called before decoding
  // completes.
  if (!is_always_on_top_wallpaper_)
    return;
  if (image.isNull()) {
    is_always_on_top_wallpaper_ = false;
    return;
  }
  reload_always_on_top_wallpaper_callback_ =
      base::BindRepeating(&WallpaperControllerImpl::ShowWallpaperImage,
                          weak_factory_.GetWeakPtr(), image, info,
                          /*preview_mode=*/false, /*always_on_top=*/true);
  reload_always_on_top_wallpaper_callback_.Run();
}

bool WallpaperControllerImpl::IsDevicePolicyWallpaper() const {
  return current_wallpaper_ &&
         current_wallpaper_->wallpaper_info().type == WallpaperType::kDevice;
}

bool WallpaperControllerImpl::IsOneShotWallpaper() const {
  return current_wallpaper_ &&
         current_wallpaper_->wallpaper_info().type == WallpaperType::kOneShot;
}

bool WallpaperControllerImpl::ShouldSetDevicePolicyWallpaper() const {
  // Only allow the device wallpaper if the policy is in effect for enterprise
  // managed devices.
  if (device_policy_wallpaper_path_.empty())
    return false;

  // Only set the device wallpaper if we're at the login screen.
  return Shell::Get()->session_controller()->GetSessionState() ==
         session_manager::SessionState::LOGIN_PRIMARY;
}

void WallpaperControllerImpl::SetDevicePolicyWallpaper() {
  DCHECK(ShouldSetDevicePolicyWallpaper());
  ReadAndDecodeWallpaper(
      base::BindOnce(&WallpaperControllerImpl::OnDevicePolicyWallpaperDecoded,
                     weak_factory_.GetWeakPtr()),
      device_policy_wallpaper_path_);
}

void WallpaperControllerImpl::OnDevicePolicyWallpaperDecoded(
    const gfx::ImageSkia& image) {
  // It might be possible that the device policy controlled wallpaper finishes
  // decoding after the user logs in. In this case do nothing.
  if (!ShouldSetDevicePolicyWallpaper())
    return;

  if (image.isNull()) {
    // If device policy wallpaper failed decoding, fall back to the default
    // wallpaper.
    SetDefaultWallpaperImpl(EmptyAccountId(), /*show_wallpaper=*/true,
                            base::DoNothing());
  } else {
    WallpaperInfo info = {device_policy_wallpaper_path_.value(),
                          WALLPAPER_LAYOUT_CENTER_CROPPED,
                          WallpaperType::kDevice, base::Time::Now()};
    ShowWallpaperImage(image, info, /*preview_mode=*/false,
                       /*always_on_top=*/false);
  }
}

void WallpaperControllerImpl::GetInternalDisplayCompositorLock() {
  if (!display::HasInternalDisplay())
    return;

  aura::Window* root_window =
      Shell::GetRootWindowForDisplayId(display::Display::InternalDisplayId());
  if (!root_window)
    return;

  compositor_lock_ = root_window->layer()->GetCompositor()->GetCompositorLock(
      this, kCompositorLockTimeout);
}

void WallpaperControllerImpl::RepaintWallpaper() {
  for (auto* root_window_controller : Shell::GetAllRootWindowControllers()) {
    auto* wallpaper_view =
        root_window_controller->wallpaper_widget_controller()->wallpaper_view();
    if (wallpaper_view)
      wallpaper_view->SchedulePaint();
  }
}

void WallpaperControllerImpl::HandleWallpaperInfoSyncedIn(
    const AccountId& account_id,
    WallpaperInfo info) {
  if (!CanSetUserWallpaper(account_id))
    return;
  // We don't sync for background users because we don't want to update the
  // wallpaper for background users. Instead, we call
  // HandleWallpaperInfoSyncedIn again in OnActiveUserPrefServiceChanged.
  if (!IsActiveUser(account_id))
    return;
  switch (info.type) {
    case WallpaperType::kCustomized:
      HandleCustomWallpaperInfoSyncedIn(account_id, info);
      break;
    case WallpaperType::kDaily:
      HandleDailyWallpaperInfoSyncedIn(account_id, info);
      break;
    case WallpaperType::kOnline:
      HandleSettingOnlineWallpaperFromWallpaperInfo(account_id, info);
      break;
    case WallpaperType::kOnceGooglePhotos:
    case WallpaperType::kDailyGooglePhotos:
      HandleGooglePhotosWallpaperInfoSyncedIn(account_id, info);
      break;
    case WallpaperType::kDefault:
    case WallpaperType::kPolicy:
    case WallpaperType::kThirdParty:
    case WallpaperType::kDevice:
    case WallpaperType::kOneShot:
    case WallpaperType::kCount:
      DCHECK(false) << "Synced in an unsyncable wallpaper type";
      break;
  }
}

void WallpaperControllerImpl::OnAttemptSetOnlineWallpaper(
    const OnlineWallpaperParams& params,
    SetWallpaperCallback callback,
    bool success) {
  if (success) {
    std::move(callback).Run(true);
    return;
  }

  const std::vector<OnlineWallpaperVariant>& variants = params.variants;
  if (variants.empty()) {
    // |variants| can be empty for users who have just migrated from the old
    // wallpaper picker to the new one.
    std::string url = params.url.spec() + GetBackdropWallpaperSuffix();
    ImageDownloader::Get()->Download(
        GURL(url), MISSING_TRAFFIC_ANNOTATION,
        base::BindOnce(&WallpaperControllerImpl::OnOnlineWallpaperDecoded,
                       set_wallpaper_weak_factory_.GetWeakPtr(), params,
                       /*save_file=*/true, std::move(callback)));
  } else {
    // Start fetching the wallpaper variants.
    url_to_image_map_.clear();
    auto on_done = base::BarrierClosure(
        variants.size(),
        base::BindOnce(
            &WallpaperControllerImpl::OnAllOnlineWallpaperVariantsDownloaded,
            weak_factory_.GetWeakPtr(), params, std::move(callback)));

    for (size_t i = 0; i < variants.size(); i++) {
      ImageDownloader::Get()->Download(
          GURL(variants.at(i).raw_url.spec() + GetBackdropWallpaperSuffix()),
          MISSING_TRAFFIC_ANNOTATION,
          base::BindOnce(
              &WallpaperControllerImpl::OnOnlineWallpaperVariantDownloaded,
              set_wallpaper_weak_factory_.GetWeakPtr(), params, on_done,
              /*current_index=*/i));
    }
  }
}

void WallpaperControllerImpl::OnOnlineWallpaperVariantDownloaded(
    const OnlineWallpaperParams& params,
    base::RepeatingClosure on_done,
    size_t current_index,
    const gfx::ImageSkia& image) {
  if (image.isNull()) {
    LOG(WARNING) << "Image download failed " << current_index;
    std::move(on_done).Run();
    return;
  }

  const std::vector<OnlineWallpaperVariant>& variants = params.variants;
  const OnlineWallpaperVariant& current_variant = variants.at(current_index);
  // Keep track of each downloaded image.
  url_to_image_map_.insert({current_variant.raw_url.spec(), image});

  // Save the image to disk.
  image.EnsureRepsForSupportedScales();
  gfx::ImageSkia deep_copy(image.DeepCopy());
  sequenced_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&SaveOnlineWallpaper, current_variant.raw_url.spec(),
                     params.layout, deep_copy));
  std::move(on_done).Run();
}

void WallpaperControllerImpl::OnAllOnlineWallpaperVariantsDownloaded(
    const OnlineWallpaperParams& params,
    SetWallpaperCallback callback) {
  bool success = url_to_image_map_.size() == params.variants.size() &&
                 !url_to_image_map_.at(params.url.spec()).isNull();
  if (!success) {
    std::move(callback).Run(success);
    return;
  }

  OnOnlineWallpaperDecoded(params, /*save_file=*/false, std::move(callback),
                           url_to_image_map_.at(params.url.spec()));
}

constexpr bool WallpaperControllerImpl::IsWallpaperTypeSyncable(
    WallpaperType type) {
  switch (type) {
    case WallpaperType::kDaily:
    case WallpaperType::kCustomized:
    case WallpaperType::kOnline:
    case WallpaperType::kOnceGooglePhotos:
    case WallpaperType::kDailyGooglePhotos:
      return true;
    case WallpaperType::kDefault:
    case WallpaperType::kPolicy:
    case WallpaperType::kThirdParty:
    case WallpaperType::kDevice:
    case WallpaperType::kOneShot:
    case WallpaperType::kCount:
      return false;
  }
}

void WallpaperControllerImpl::SetDailyRefreshCollectionId(
    const AccountId& account_id,
    const std::string& collection_id) {
  WallpaperInfo info;
  if (!GetUserWallpaperInfo(account_id, &info))
    return;

  // If daily refresh is being enabled.
  if (!collection_id.empty()) {
    info.type = WallpaperType::kDaily;
    info.collection_id = collection_id;
  }

  // If Daily Refresh is disabled without selecting another wallpaper, we should
  // keep the current wallpaper and change to type `WallpaperType::kOnline`, so
  // daily refreshes stop.
  if (collection_id.empty() && info.type == WallpaperType::kDaily) {
    info.type = WallpaperType::kOnline;
  }
  SetUserWallpaperInfo(account_id, info);
}

std::string WallpaperControllerImpl::GetDailyRefreshCollectionId(
    const AccountId& account_id) const {
  WallpaperInfo info;
  if (!GetUserWallpaperInfo(account_id, &info))
    return std::string();
  if (info.type != WallpaperType::kDaily)
    return std::string();
  return info.collection_id;
}

void WallpaperControllerImpl::SyncLocalAndRemotePrefs(
    const AccountId& account_id) {
  // Check if the synced info was set by another device, and if we have already
  // handled it locally.
  WallpaperInfo synced_info;
  WallpaperInfo local_info;
  if (!pref_manager_->GetSyncedWallpaperInfo(account_id, &synced_info))
    return;
  if (!pref_manager_->GetLocalWallpaperInfo(account_id, &local_info)) {
    HandleWallpaperInfoSyncedIn(account_id, synced_info);
    return;
  }
  if (synced_info == local_info)
    return;
  if (synced_info.date >= local_info.date) {
    // If synced is newer or the same age, it wins.
    HandleWallpaperInfoSyncedIn(account_id, synced_info);
  } else if (local_info.type == WallpaperType::kCustomized) {
    // Generally, we handle setting synced_info when local_info is updated.
    // But for custom images, we wait until the image is uploaded to Drive,
    // which may not be available at the time of setting the local_info.
    base::FilePath source = GetCustomWallpaperDir(kOriginalWallpaperSubDir)
                                .Append(local_info.location);
    SaveWallpaperToDriveFsAndSyncInfo(account_id, source);
  }
}

bool WallpaperControllerImpl::IsDailyRefreshEnabled() const {
  return features::IsWallpaperWebUIEnabled() &&
         !GetDailyRefreshCollectionId(GetActiveAccountId()).empty();
}

bool WallpaperControllerImpl::IsDailyGooglePhotosWallpaperSelected() {
  return GetActiveUserWallpaperInfo().type == WallpaperType::kDailyGooglePhotos;
}

bool WallpaperControllerImpl::IsGooglePhotosWallpaperSet() const {
  WallpaperInfo info;
  GetUserWallpaperInfo(GetActiveAccountId(), &info);
  return info.type == WallpaperType::kOnceGooglePhotos;
}

void WallpaperControllerImpl::UpdateDailyRefreshWallpaper(
    RefreshWallpaperCallback callback) {
  // Invalidate weak ptrs to cancel prior requests to set wallpaper.
  set_wallpaper_weak_factory_.InvalidateWeakPtrs();
  if (!IsDailyRefreshEnabled() && !IsDailyGooglePhotosWallpaperSelected()) {
    update_wallpaper_timer_.Stop();
    std::move(callback).Run(false);
    return;
  }

  AccountId account_id = GetActiveAccountId();
  WallpaperInfo info;

  // |wallpaper_controller_cient_| has a slightly shorter lifecycle than
  // wallpaper controller.
  if (wallpaper_controller_client_ && GetUserWallpaperInfo(account_id, &info)) {
    if (info.type == WallpaperType::kDailyGooglePhotos) {
      wallpaper_controller_client_->FetchDailyGooglePhotosPhoto(
          account_id, info.collection_id,
          base::BindOnce(
              &WallpaperControllerImpl::OnDailyGooglePhotosPhotoFetched,
              set_wallpaper_weak_factory_.GetWeakPtr(), account_id,
              info.collection_id, std::move(callback)));
    } else {
      DCHECK_EQ(info.type, WallpaperType::kDaily);
      OnlineWallpaperVariantInfoFetcher::FetchParamsCallback fetch_callback =
          base::BindOnce(&WallpaperControllerImpl::OnWallpaperVariantsFetched,
                         set_wallpaper_weak_factory_.GetWeakPtr(), info.type,
                         std::move(callback));
      // Fetch can fail if wallpaper_controller_client has been cleared or
      // |info| is malformed.
      if (!variant_info_fetcher_->FetchDailyWallpaper(
              account_id, info, GetColorMode(), std::move(fetch_callback))) {
        // Could not start fetch of wallpaper variants. Likely because the
        // chrome client isn't ready. Schedule for later.
        NOTREACHED() << "Failed to initiate daily wallpaper fetch";
      }
    }
  } else {
    StartDailyRefreshTimer();
    std::move(callback).Run(false);
  }
}

void WallpaperControllerImpl::StartDailyRefreshTimer() {
  base::TimeDelta timer_delay =
      features::IsWallpaperFastRefreshEnabled()
          ? base::Seconds(10)
          : FuzzTimeDelta(GetTimeToNextDailyRefreshUpdate());
  StartUpdateWallpaperTimer(timer_delay);
}

void WallpaperControllerImpl::StartGooglePhotosStalenessTimer() {
  base::TimeDelta timer_delay = FuzzTimeDelta(base::Days(1));
  StartUpdateWallpaperTimer(timer_delay);
}

void WallpaperControllerImpl::OnFetchDailyWallpaperFailed() {
  StartUpdateWallpaperTimer(base::Hours(1));
}

void WallpaperControllerImpl::StartUpdateWallpaperTimer(base::TimeDelta delay) {
  DCHECK(delay.is_positive());
  base::Time desired_run_time = base::Time::Now() + delay;
  update_wallpaper_timer_.Start(
      FROM_HERE, desired_run_time,
      base::BindOnce(&WallpaperControllerImpl::OnUpdateWallpaperTimerExpired,
                     weak_factory_.GetWeakPtr()));
}

base::TimeDelta WallpaperControllerImpl::GetTimeToNextDailyRefreshUpdate()
    const {
  WallpaperInfo info;
  if (!GetUserWallpaperInfo(GetActiveAccountId(), &info))
    return base::TimeDelta();
  base::TimeDelta delta = (info.date + base::Days(1)) - base::Time::Now();
  // Guarantee the delta is always 0 or positive.
  return delta.is_positive() ? delta : base::TimeDelta();
}

void WallpaperControllerImpl::OnUpdateWallpaperTimerExpired() {
  WallpaperInfo info;
  auto account_id = GetActiveAccountId();
  if (!pref_manager_->GetLocalWallpaperInfo(account_id, &info)) {
    LOG(ERROR) << "Timer to update wallpaper expired, but the current "
               << "wallpaper info is missing or invalid.";
    return;
  }
  switch (info.type) {
    case WallpaperType::kDaily:
    case WallpaperType::kDailyGooglePhotos:
      UpdateDailyRefreshWallpaper();
      return;
    case WallpaperType::kOnceGooglePhotos:
      CheckGooglePhotosStaleness(account_id, info);
      return;
    case WallpaperType::kOnline:
    case WallpaperType::kCustomized:
    case WallpaperType::kDefault:
    case WallpaperType::kPolicy:
    case WallpaperType::kThirdParty:
    case WallpaperType::kDevice:
    case WallpaperType::kOneShot:
    case WallpaperType::kCount:
      LOG(ERROR) << "Timer to update wallpaper expired, but the current "
                 << "wallpaper type doesn't support/require updating.";
      return;
  }
}

void WallpaperControllerImpl::CheckGooglePhotosStaleness(
    const AccountId& account_id,
    const WallpaperInfo& info) {
  DCHECK_EQ(info.type, WallpaperType::kOnceGooglePhotos);
  wallpaper_controller_client_->FetchGooglePhotosPhoto(
      account_id, info.location,
      base::BindOnce(&WallpaperControllerImpl::HandleGooglePhotosStalenessCheck,
                     set_wallpaper_weak_factory_.GetWeakPtr(), account_id));
  // Start a recheck timer in case our weak ptr above is invalidated, or other
  // confusing conditions happen before the API call returns.
  StartUpdateWallpaperTimer(base::Hours(1));
}

void WallpaperControllerImpl::HandleGooglePhotosStalenessCheck(
    const AccountId& account_id,
    ash::personalization_app::mojom::GooglePhotosPhotoPtr photo,
    bool success) {
  // If `success` is false, then we failed to connect to the Google Photos API
  // for some reason. We're already set to try again later, since the timer was
  // reset to one hour in `CheckGooglePhotosStaleness`.
  if (!success)
    return;

  if (!photo) {
    sequenced_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&DeleteGooglePhotosCache, account_id));
    SetDefaultWallpaper(account_id, /*show_wallpaper=*/true, base::DoNothing());
  } else {
    StartGooglePhotosStalenessTimer();
  }
}

void WallpaperControllerImpl::SaveWallpaperToDriveFsAndSyncInfo(
    const AccountId& account_id,
    const base::FilePath& origin_path) {
  if (!features::IsWallpaperWebUIEnabled())
    return;
  if (!wallpaper_controller_client_)
    return;
  if (!wallpaper_controller_client_->IsWallpaperSyncEnabled(account_id))
    return;
  wallpaper_controller_client_->SaveWallpaperToDriveFs(
      account_id, origin_path,
      base::BindOnce(&WallpaperControllerImpl::WallpaperSavedToDriveFS,
                     weak_factory_.GetWeakPtr(), account_id));
}

void WallpaperControllerImpl::WallpaperSavedToDriveFS(
    const AccountId& account_id,
    bool success) {
  if (!success)
    return;
  WallpaperInfo local_info;
  CHECK(pref_manager_->GetLocalWallpaperInfo(account_id, &local_info));
  pref_manager_->SetSyncedWallpaperInfo(account_id, local_info);
}

void WallpaperControllerImpl::HandleCustomWallpaperInfoSyncedIn(
    const AccountId& account_id,
    WallpaperInfo info) {
  base::FilePath drivefs_path =
      wallpaper_controller_client_->GetWallpaperPathFromDriveFs(account_id);
  if (drivefs_path.empty())
    return;
  base::File::Info drivefs_file_info;
  base::GetFileInfo(drivefs_path, &drivefs_file_info);
  // If the drivefs image is older than the synced info date, we know it is
  // outdated.
  if (drivefs_file_info.last_modified < info.date) {
    drive_fs_wallpaper_watcher_.Watch(
        drivefs_path, base::FilePathWatcher::Type::kNonRecursive,
        base::BindRepeating(&WallpaperControllerImpl::DriveFsWallpaperChanged,
                            weak_factory_.GetWeakPtr()));
    return;
  }
  base::FilePath path_in_prefs = base::FilePath(info.location);
  std::string file_name = path_in_prefs.BaseName().value();
  ReadAndDecodeWallpaper(
      base::BindOnce(&WallpaperControllerImpl::SaveAndSetWallpaper,
                     weak_factory_.GetWeakPtr(), account_id, file_name,
                     WallpaperType::kCustomized, info.layout,
                     /*show_wallpaper=*/true),
      drivefs_path);
}

void WallpaperControllerImpl::DriveFsWallpaperChanged(
    const base::FilePath& path,
    bool error) {
  SyncLocalAndRemotePrefs(GetActiveAccountId());
}

PrefService* WallpaperControllerImpl::GetUserPrefServiceSyncable(
    const AccountId& account_id) const {
  if (!features::IsWallpaperWebUIEnabled())
    return nullptr;
  if (!wallpaper_controller_client_->IsWallpaperSyncEnabled(account_id))
    return nullptr;
  return Shell::Get()->session_controller()->GetUserPrefServiceForUser(
      account_id);
}

void WallpaperControllerImpl::HandleDailyWallpaperInfoSyncedIn(
    const AccountId& account_id,
    const WallpaperInfo& info) {
  DCHECK(info.type == WallpaperType::kDaily);
  std::string old_collection_id = GetDailyRefreshCollectionId(account_id);
  if (info.collection_id == old_collection_id)
    return;
  OnlineWallpaperVariantInfoFetcher::FetchParamsCallback callback =
      base::BindOnce(&WallpaperControllerImpl::OnWallpaperVariantsFetched,
                     weak_factory_.GetWeakPtr(), info.type, base::DoNothing());
  if (!variant_info_fetcher_->FetchDailyWallpaper(
          account_id, info, GetColorMode(), std::move(callback))) {
    NOTREACHED() << "Fetch of daily wallpaper info failed.";
  }
}

void WallpaperControllerImpl::HandleGooglePhotosWallpaperInfoSyncedIn(
    const AccountId& account_id,
    const WallpaperInfo& info) {
  if (!features::IsWallpaperGooglePhotosIntegrationEnabled()) {
    NOTREACHED();
    return;
  }
  bool daily_refresh_enabled = info.type == WallpaperType::kDailyGooglePhotos;
  if (daily_refresh_enabled) {
    // We only want to update the user's `WallpaperInfo` if this is a new
    // album.  Otherwise, each time the daily refresh timer expires on multiple
    // devices we could trigger devices to refresh multiple times.
    WallpaperInfo old_info;
    if (GetUserWallpaperInfo(account_id, &old_info) &&
        old_info.collection_id != info.collection_id) {
      SetGooglePhotosWallpaper(
          GooglePhotosWallpaperParams(
              account_id, info.collection_id,
              /*daily_refresh_enabled=*/true, info.layout,
              /*preview_mode=*/false, /*dedup_key=*/absl::nullopt),
          base::DoNothing());
    }
  } else {
    SetGooglePhotosWallpaper(GooglePhotosWallpaperParams(
                                 account_id, info.location,
                                 /*daily_refresh_enabled=*/false, info.layout,
                                 /*preview_mode=*/false, info.dedup_key),
                             base::DoNothing());
  }
}

void WallpaperControllerImpl::HandleSettingOnlineWallpaperFromWallpaperInfo(
    const AccountId& account_id,
    const WallpaperInfo& info) {
  DCHECK(info.type == WallpaperType::kDaily ||
         info.type == WallpaperType::kOnline);
  if (!info.asset_id.has_value()) {
    // If a user has not changed their wallpaper since the addition of asset_id,
    // we can have a WallpaperInfo without an asset_id from synced data.
    // In this case, skip it. We don't have enough information to retrieve the
    // wallpaper.
    LOG(WARNING) << "Synced old online wallpaper info";
    return;
  }

  OnlineWallpaperVariantInfoFetcher::FetchParamsCallback callback =
      base::BindOnce(&WallpaperControllerImpl::OnWallpaperVariantsFetched,
                     weak_factory_.GetWeakPtr(), info.type, base::DoNothing());

  variant_info_fetcher_->FetchOnlineWallpaper(account_id, info, GetColorMode(),
                                              std::move(callback));
}

}  // namespace ash
