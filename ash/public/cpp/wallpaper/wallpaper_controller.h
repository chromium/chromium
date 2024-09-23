// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_WALLPAPER_WALLPAPER_CONTROLLER_H_
#define ASH_PUBLIC_CPP_WALLPAPER_WALLPAPER_CONTROLLER_H_

#include <cstdint>
#include <optional>
#include <string>

#include "ash/public/cpp/ash_public_export.h"
#include "ash/public/cpp/wallpaper/google_photos_wallpaper_params.h"
#include "ash/public/cpp/wallpaper/online_wallpaper_params.h"
#include "ash/public/cpp/wallpaper/sea_pen_image.h"
#include "ash/public/cpp/wallpaper/wallpaper_info.h"
#include "ash/public/cpp/wallpaper/wallpaper_types.h"
#include "ash/webui/common/mojom/sea_pen.mojom-forward.h"
#include "base/containers/lru_cache.h"
#include "base/files/file_path.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/ref_counted_memory.h"
#include "base/time/time.h"
#include "components/user_manager/user_type.h"

class AccountId;

namespace gfx {
class ImageSkia;
}

namespace ash {

class WallpaperControllerObserver;
class WallpaperControllerClient;
class WallpaperDriveFsDelegate;

// Used by Chrome to set the wallpaper displayed by ash.
class ASH_PUBLIC_EXPORT WallpaperController {
 public:
  // A callback for confirming if Set*Wallpaper operations completed
  // successfully.
  using SetWallpaperCallback = base::OnceCallback<void(bool success)>;

  using DailyGooglePhotosIdCache = base::HashingLRUCacheSet<uint32_t>;

  using LoadPreviewImageCallback =
      base::OnceCallback<void(scoped_refptr<base::RefCountedMemory>)>;

  WallpaperController();
  virtual ~WallpaperController();

  static WallpaperController* Get();

  // Sets the client interface, used to show the wallpaper picker, etc.
  virtual void SetClient(WallpaperControllerClient* client) = 0;

  virtual void SetDriveFsDelegate(
      std::unique_ptr<WallpaperDriveFsDelegate> drivefs_delegate) = 0;

  // Sets paths for wallpaper directories and the device policy wallpaper path.
  // |user_data|: Directory where user data can be written.
  // |wallpapers|: Directory where downloaded chromeos wallpapers reside.
  // |custom_wallpapers|: Directory where custom wallpapers reside.
  // |device_policy_wallpaper|: Path of the device policy wallpaper (if any).
  virtual void Init(const base::FilePath& user_data,
                    const base::FilePath& wallpapers,
                    const base::FilePath& custom_wallpapers,
                    const base::FilePath& device_policy_wallpaper) = 0;

  // Whether the user with `account_id` can set wallpaper. Users may be
  // disallowed from setting wallpaper based on enterprise policy or if the
  // device is running in kiosk mode.
  virtual bool CanSetUserWallpaper(const AccountId& account_id) const = 0;

  // Sets the wallpaper from a local file and updates the saved wallpaper info
  // for the user.
  // |account_id|: The user's account id.
  // |file_path|: The path of the image file to read.
  // |layout|: The layout of the wallpaper, used for wallpaper resizing.
  // |preview_mode|: If true, show the wallpaper immediately but doesn't change
  //                 the user wallpaper info until |ConfirmPreviewWallpaper| is
  //                 called.
  // |callback|: Called when the image is set.
  virtual void SetCustomWallpaper(const AccountId& account_id,
                                  const base::FilePath& file_path,
                                  WallpaperLayout layout,
                                  bool preview_mode,
                                  SetWallpaperCallback callback) = 0;

  // Sets wallpaper from a local file and updates the saved wallpaper info for
  // the user.
  // |account_id|: The user's account id.
  // |file_name|: The name of the wallpaper file.
  // |layout|: The layout of the wallpaper, used for wallpaper resizing.
  // |preview_mode|: If true, show the wallpaper immediately but doesn't change
  //                 the user wallpaper info until |ConfirmPreviewWallpaper| is
  //                 called.
  // |callback|: Called when the wallpaper is set.
  // |file_path| The path of the image file to read.
  // |image|: The wallpaper image.
  virtual void SetDecodedCustomWallpaper(const AccountId& account_id,
                                         const std::string& file_name,
                                         WallpaperLayout layout,
                                         bool preview_mode,
                                         SetWallpaperCallback callback,
                                         const std::string& file_path,
                                         const gfx::ImageSkia& image) = 0;

  // Sets the wallpaper at |params.asset_id|, |params.url| and
  // |params.collection_id| as the active wallpaper for the user at
  // |params.account_id|. The first time this is called, will download the
  // wallpaper and cache on disk. Subsequent calls with the same url will use
  // the stored wallpaper. If |params.preview_mode| is true, the visible
  // background wallpaper will change, but that change will not be persisted in
  // preferences. Call |ConfirmPreviewMode| or |CancelPreviewMode| to finalize.
  // |callback| is required and will be called after the image is fetched (from
  // network or disk) and decoded.
  virtual void SetOnlineWallpaper(const OnlineWallpaperParams& params,
                                  SetWallpaperCallback callback) = 0;

  // Used to select, load, and show the OOBE wallpaper
  virtual void ShowOobeWallpaper() = 0;

  // Sets the Google Photos photo with id |params.id| as the active wallpaper.
  virtual void SetGooglePhotosWallpaper(
      const GooglePhotosWallpaperParams& params,
      SetWallpaperCallback callback) = 0;

  // Sets and stores the collection id used to refresh Google Photos wallpapers.
  // `album_id` is empty if daily refresh is not enabled.
  virtual void SetGooglePhotosDailyRefreshAlbumId(
      const AccountId& account_id,
      const std::string& album_id) = 0;

  // Get the Google Photos daily refresh album id. Empty if
  // `current_wallpaper_.type` is not `kDailyGooglePhotos`
  virtual std::string GetGooglePhotosDailyRefreshAlbumId(
      const AccountId& account_id) const = 0;

  // Set and get the cache of hashed ids for recently shown daily Google Photos.
  // This cache is persisted in local preferences, and is not synced across
  // devices.
  virtual bool SetDailyGooglePhotosWallpaperIdCache(
      const AccountId& account_id,
      const DailyGooglePhotosIdCache& ids) = 0;
  virtual bool GetDailyGooglePhotosWallpaperIdCache(
      const AccountId& account_id,
      DailyGooglePhotosIdCache& ids_out) const = 0;

  // Downloads and sets a time of day wallpaper to be the active wallpaper.
  // |acount_id|: The user's account id.
  // |callback|: Called with a boolean to indicate success when the wallpaper is
  // fetched and decoded.
  virtual void SetTimeOfDayWallpaper(const AccountId& account_id,
                                     SetWallpaperCallback callback) = 0;

  // Sets the user's wallpaper to be the default wallpaper. Note: different user
  // types may have different default wallpapers.
  // |account_id|: The user's account id.
  // |show_wallpaper|: If false, don't show the new wallpaper now but only
  //                   update cache.
  // |callback|: Called with a boolean to indicate success when the default
  //             wallpaper is read and decoded.
  virtual void SetDefaultWallpaper(const AccountId& account_id,
                                   bool show_wallpaper,
                                   SetWallpaperCallback callback) = 0;

  // Get the path to the default wallpaper file for the |user_type|. Will be
  // empty if this user/device has no recommended default wallpaper.
  virtual base::FilePath GetDefaultWallpaperPath(
      user_manager::UserType user_type) = 0;

  // Sets the paths of the customized default wallpaper to be used wherever a
  // default wallpaper is needed. If a default wallpaper is being shown, updates
  // the screen to replace the old default wallpaper. Note: it doesn't change
  // the default wallpaper for guest and child accounts.
  // |customized_default_small_path|: The file path of the small-size customized
  //                                  default wallpaper, if any.
  // |customized_default_large_path|: The file path of the large-size customized
  //                                  default wallpaper, if any.
  virtual void SetCustomizedDefaultWallpaperPaths(
      const base::FilePath& customized_default_small_path,
      const base::FilePath& customized_default_large_path) = 0;

  // Sets wallpaper from policy. If the user has logged in, show the policy
  // wallpaper immediately, otherwise, the policy wallpaper will be shown the
  // next time |ShowUserWallpaper| is called. Note: it is different from device
  // policy. This function may be called on the login screen, thus it's
  // responsibility of the caller to provide the correct |user_type| for such
  // case i.e. |user_type| should be derived by using
  // |user_manager::UserManager|.
  // |account_id|: The user's account id.
  // |user_type|: The type of user.
  // |data|: The data used to decode the image.
  virtual void SetPolicyWallpaper(const AccountId& account_id,
                                  user_manager::UserType user_type,
                                  const std::string& data) = 0;

  // Sets the path of device policy wallpaper.
  // |device_policy_wallpaper_path|: The file path of the device policy
  //                                 wallpaper if it was set or empty value if
  //                                 it was cleared.
  virtual void SetDevicePolicyWallpaperPath(
      const base::FilePath& device_policy_wallpaper_path) = 0;

  // Sets wallpaper from a third-party app (as opposed to the Chrome OS
  // wallpaper picker). Chrome extensions and Arc++ call this function.
  // |account_id|: The user's account id.
  // |wallpaper_files_id|: The file id for |account_id|.
  // |file_name|: The name of the wallpaper file.
  // |layout|: The layout of the wallpaper, used for wallpaper resizing.
  // |image|: The wallpaper image.
  // Returns if the wallpaper is allowed to be shown on screen. It's false if:
  // 1) the user is not permitted to change wallpaper, or
  // 2) updating the on-screen wallpaper is not allowed at the given moment.
  virtual bool SetThirdPartyWallpaper(const AccountId& account_id,
                                      const std::string& file_name,
                                      WallpaperLayout layout,
                                      const gfx::ImageSkia& image) = 0;

  // Sets `image_id` as system wallpaper for user with `account_id`. A
  // corresponding image with id `image_id` is expected to be present on disk
  // and will be loaded via SeaPenWallpaperManager. Calls `callback` with
  // boolean success. Can fail if `account_id` is not allowed to set wallpaper,
  // or the image failed to decode.
  virtual void SetSeaPenWallpaper(const AccountId& account_id,
                                  uint32_t image_id,
                                  bool preview_mode,
                                  SetWallpaperCallback callback) = 0;

  // Confirms the wallpaper being previewed to be set as the actual user
  // wallpaper. Must be called in preview mode.
  virtual void ConfirmPreviewWallpaper() = 0;

  // Cancels the wallpaper preview and reverts to the user wallpaper. Must be
  // called in preview mode.
  virtual void CancelPreviewWallpaper() = 0;

  // Updates the layout for the user's current wallpaper and reloads the
  // wallpaper with the new layout. Note that only custom and Google Photos
  // wallpaper types are currently supported.
  // |account_id|: The user's account id.
  // |layout|: The new layout of the wallpaper.
  virtual void UpdateCurrentWallpaperLayout(const AccountId& account_id,
                                            WallpaperLayout layout) = 0;

  // Shows the user's wallpaper, which is determined in the following order:
  // 1) Use device policy wallpaper if it exists AND we are at the login screen.
  // 2) Use user policy wallpaper if it exists.
  // 3) Use the wallpaper set by the user (either by |SetOnlineWallpaper| or
  //    |SetCustomWallpaper|), if any.
  // 4) Use the default wallpaper of this user.
  virtual void ShowUserWallpaper(const AccountId& account_id) = 0;

  // Shows the user's wallpaper but uses |user_type| to determine default
  // wallpaper if necessary. This is intendend for use where users are not
  // yet logged in (i.e. login screen).
  virtual void ShowUserWallpaper(const AccountId& account_id,
                                 const user_manager::UserType user_type) = 0;

  // Used by the gaia-signin UI. Signin wallpaper is considered either as the
  // device policy wallpaper or the default wallpaper.
  virtual void ShowSigninWallpaper() = 0;

  // Shows a one-shot wallpaper, which does not belong to any particular user
  // and is not saved to file. Note: the wallpaper will never be dimmed or
  // blurred because it's assumed that the caller wants to show the image as is
  // when using this method.
  virtual void ShowOneShotWallpaper(const gfx::ImageSkia& image) = 0;

  // Shows an override wallpaper instead of the wallpaper that would normally be
  // shown. All other wallpaper requests are ignored when the override wallpaper
  // is being shown.
  // |image_path|: The file path to read the image data from.
  // |always_on_top|: Whether the override wallpaper should be shown on top of
  //                  everything except for the power off animation.
  virtual void ShowOverrideWallpaper(const base::FilePath& image_path,
                                     bool always_on_top) = 0;

  // Removes the override wallpaper. The wallpaper will revert to the previous
  // one, or a default one if there was none. No-op if the current wallpaper is
  // not overridden.
  virtual void RemoveOverrideWallpaper() = 0;

  // Removes all of the user's saved wallpapers and related info.
  // |account_id|: The user's account id.
  virtual void RemoveUserWallpaper(const AccountId& account_id,
                                   base::OnceClosure on_removed) = 0;

  // Removes all of the user's saved wallpapers and related info if the
  // wallpaper was set by |SetPolicyWallpaper|. In addition, sets the user's
  // wallpaper to be the default. If the user has logged in, show the default
  // wallpaper immediately, otherwise, the default wallpaper will be shown the
  // next time |ShowUserWallpaper| is called.
  // |account_id|: The user's account id.
  virtual void RemovePolicyWallpaper(const AccountId& account_id) = 0;

  // Sets wallpaper animation duration. Passing an empty value disables the
  // animation.
  virtual void SetAnimationDuration(base::TimeDelta animation_duration) = 0;

  // Opens the wallpaper picker if the active user is not controlled by policy
  // and it's allowed to change wallpaper per the user type and the login state.
  virtual void OpenWallpaperPickerIfAllowed() = 0;

  // Minimizes all windows except the active window.
  // |user_id_hash|: The hash value corresponding to |User::username_hash|.
  virtual void MinimizeInactiveWindows(const std::string& user_id_hash) = 0;

  // Restores all minimized windows to their previous states. This should only
  // be called after calling |MinimizeInactiveWindows|.
  // |user_id_hash|: The hash value corresponding to |User::username_hash|.
  virtual void RestoreMinimizedWindows(const std::string& user_id_hash) = 0;

  // Add and remove wallpaper observers.
  virtual void AddObserver(WallpaperControllerObserver* observer) = 0;
  virtual void RemoveObserver(WallpaperControllerObserver* observer) = 0;

  // Returns the wallpaper image currently being shown.
  virtual gfx::ImageSkia GetWallpaperImage() = 0;

  // Loads the preview image of the currently shown wallpaper. Callback is
  // called after the operation completes.
  virtual void LoadPreviewImage(LoadPreviewImageCallback callback) = 0;

  // Returns whether the current wallpaper is blurred on lock/login screen.
  virtual bool IsWallpaperBlurredForLockState() const = 0;

  // Returns true if the wallpaper of the currently active user (if any) is
  // controlled by policy (excluding device policy). If there's no active user,
  // returns false.
  virtual bool IsActiveUserWallpaperControlledByPolicy() = 0;

  // Returns true if the wallpaper of the user with the given |account_id| is
  // controlled by policy (excluding device policy). If the |account_id| is
  // invalid, returns false.
  virtual bool IsWallpaperControlledByPolicy(
      const AccountId& account_id) const = 0;

  // Returns active user's `WallpaperInfo` if there is an active user that has
  // valid `WallpaperInfo`.
  virtual std::optional<WallpaperInfo> GetActiveUserWallpaperInfo() const = 0;

  // Returns a `WallpaperInfo` for the given `account_id` if `account_id` exists
  // and has valid saved info.
  virtual std::optional<WallpaperInfo> GetWallpaperInfoForAccountId(
      const AccountId& account_id) const = 0;

  // Set and store the collection id used to update refreshable wallpapers.
  // Empty if daily refresh is not enabled.
  virtual void SetDailyRefreshCollectionId(
      const AccountId& account_id,
      const std::string& collection_id) = 0;

  // Get the daily refresh collection id. Empty if daily refresh is not enabled;
  virtual std::string GetDailyRefreshCollectionId(
      const AccountId& account_id) const = 0;

  // With daily refresh enabled, this updates the wallpaper by asking for a
  // wallpaper from within the user specified collection.
  using RefreshWallpaperCallback = base::OnceCallback<void(bool success)>;
  virtual void UpdateDailyRefreshWallpaper(
      RefreshWallpaperCallback callback = base::DoNothing()) = 0;

  // Sync wallpaper infos and images.
  // |account_id|: The account id of the user.
  virtual void SyncLocalAndRemotePrefs(const AccountId& account_id) = 0;

  // The `AccountId` for the user whose wallpaper is currently displayed. May be
  // empty `AccountId` for things like OOBE and device policy wallpaper.
  virtual const AccountId& CurrentAccountId() const = 0;
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_WALLPAPER_WALLPAPER_CONTROLLER_H_
