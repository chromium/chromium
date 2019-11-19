// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_WALLPAPER_CONTROLLER_H_
#define ASH_PUBLIC_CPP_WALLPAPER_CONTROLLER_H_

#include <string>
#include <vector>

#include "ash/public/cpp/ash_public_export.h"
#include "ash/public/cpp/wallpaper_info.h"
#include "ash/public/cpp/wallpaper_types.h"
#include "base/files/file_path.h"
#include "base/time/time.h"

class AccountId;

namespace gfx {
class ImageSkia;
}

namespace ash {

class WallpaperControllerObserver;
class WallpaperControllerClient;

// Used by Chrome to set the wallpaper displayed by ash.
class ASH_PUBLIC_EXPORT WallpaperController {
 public:
  static WallpaperController* Get();

  // Sets the client interface, used to show the wallpaper picker, etc.
  virtual void SetClient(WallpaperControllerClient* client) = 0;

  // Sets paths for wallpaper directories and the device policy wallpaper path.
  // |user_data|: Directory where user data can be written.
  // |wallpapers|: Directory where downloaded chromeos wallpapers reside.
  // |custom_wallpapers|: Directory where custom wallpapers reside.
  // |device_policy_wallpaper|: Path of the device policy wallpaper (if any).
  virtual void Init(const base::FilePath& user_data,
                    const base::FilePath& wallpapers,
                    const base::FilePath& custom_wallpapers,
                    const base::FilePath& device_policy_wallpaper) = 0;

  // Sets wallpaper from a local file and updates the saved wallpaper info for
  // the user.
  // |account_id|: The user's account id.
  // |wallpaper_files_id|: The file id for |account_id|.
  // |file_name|: The name of the wallpaper file.
  // |layout|: The layout of the wallpaper, used for wallpaper resizing.
  // |image|: The wallpaper image.
  // |preview_mode|: If true, show the wallpaper immediately but doesn't change
  //                 the user wallpaper info until |ConfirmPreviewWallpaper| is
  //                 called.
  virtual void SetCustomWallpaper(const AccountId& account_id,
                                  const std::string& wallpaper_files_id,
                                  const std::string& file_name,
                                  WallpaperLayout layout,
                                  const gfx::ImageSkia& image,
                                  bool preview_mode) = 0;

  // Sets wallpaper from the Chrome OS wallpaper picker. If the wallpaper file
  // corresponding to |url| already exists in local file system (i.e.
  // |SetOnlineWallpaperFromData| was called earlier with the same |url|),
  // returns true and sets wallpaper for the user, otherwise returns false.
  // |account_id|: The user's account id.
  // |url|: The wallpaper url.
  // |layout|: The layout of the wallpaper, used for wallpaper resizing.
  // |preview_mode|: If true, show the wallpaper immediately but doesn't change
  //                 the user wallpaper info until |ConfirmPreviewWallpaper| is
  //                 called.
  // Responds with true if the wallpaper file exists in local file system.
  using SetOnlineWallpaperIfExistsCallback = base::OnceCallback<void(bool)>;
  virtual void SetOnlineWallpaperIfExists(
      const AccountId& account_id,
      const std::string& url,
      WallpaperLayout layout,
      bool preview_mode,
      SetOnlineWallpaperIfExistsCallback callback) = 0;

  // Sets wallpaper from the Chrome OS wallpaper picker and saves the wallpaper
  // to local file system. After this, |SetOnlineWallpaperIfExists| will return
  // true for the same |url|, so that there's no need to provide |image_data|
  // when the same wallpaper needs to be set again or for another user.
  // |account_id|: The user's account id.
  // |url|: The wallpaper url.
  // |layout|: The layout of the wallpaper, used for wallpaper resizing.
  // |preview_mode|: If true, show the wallpaper immediately but doesn't change
  //                 the user wallpaper info until |ConfirmPreviewWallpaper| is
  //                 called.
  // Responds with true if the wallpaper is set successfully (i.e. no decoding
  // error etc.).
  using SetOnlineWallpaperFromDataCallback = base::OnceCallback<void(bool)>;
  virtual void SetOnlineWallpaperFromData(
      const AccountId& account_id,
      const std::string& image_data,
      const std::string& url,
      WallpaperLayout layout,
      bool preview_mode,
      SetOnlineWallpaperFromDataCallback callback) = 0;

  // Sets the user's wallpaper to be the default wallpaper. Note: different user
  // types may have different default wallpapers.
  // |account_id|: The user's account id.
  // |wallpaper_files_id|: The file id for |account_id|.
  // |show_wallpaper|: If false, don't show the new wallpaper now but only
  //                   update cache.
  virtual void SetDefaultWallpaper(const AccountId& account_id,
                                   const std::string& wallpaper_files_id,
                                   bool show_wallpaper) = 0;

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
  // policy.
  // |account_id|: The user's account id.
  // |wallpaper_files_id|: The file id for |account_id|.
  // |data|: The data used to decode the image.
  virtual void SetPolicyWallpaper(const AccountId& account_id,
                                  const std::string& wallpaper_files_id,
                                  const std::string& data) = 0;

  // Sets the path of device policy wallpaper.
  // |device_policy_wallpaper_path|: The file path of the device policy
  //                                 wallpaper if it was set or empty value if
  //                                 it was cleared.
  virtual void SetDevicePolicyWallpaperPath(
      const base::FilePath& device_policy_wallpaper_path) = 0;

  // Sets wallpaper from a third-party app (as opposed to the Chrome OS
  // wallpaper picker).
  // |account_id|: The user's account id.
  // |wallpaper_files_id|: The file id for |account_id|.
  // |file_name|: The name of the wallpaper file.
  // |layout|: The layout of the wallpaper, used for wallpaper resizing.
  // |image|: The wallpaper image.
  // Returns if the wallpaper is allowed to be shown on screen. It's false if:
  // 1) the user is not permitted to change wallpaper, or
  // 2) updating the on-screen wallpaper is not allowed at the given moment.
  virtual bool SetThirdPartyWallpaper(const AccountId& account_id,
                                      const std::string& wallpaper_files_id,
                                      const std::string& file_name,
                                      WallpaperLayout layout,
                                      const gfx::ImageSkia& image) = 0;

  // Confirms the wallpaper being previewed to be set as the actual user
  // wallpaper. Must be called in preview mode.
  virtual void ConfirmPreviewWallpaper() = 0;

  // Cancels the wallpaper preview and reverts to the user wallpaper. Must be
  // called in preview mode.
  virtual void CancelPreviewWallpaper() = 0;

  // Updates the layout for the user's custom wallpaper and reloads the
  // wallpaper with the new layout.
  // |account_id|: The user's account id.
  // |layout|: The new layout of the wallpaper.
  virtual void UpdateCustomWallpaperLayout(const AccountId& account_id,
                                           WallpaperLayout layout) = 0;

  // Shows the user's wallpaper, which is determined in the following order:
  // 1) Use device policy wallpaper if it exists AND we are at the login screen.
  // 2) Use user policy wallpaper if it exists.
  // 3) Use the wallpaper set by the user (either by |SetOnlineWallpaper| or
  //    |SetCustomWallpaper|), if any.
  // 4) Use the default wallpaper of this user.
  virtual void ShowUserWallpaper(const AccountId& account_id) = 0;

  // Used by the gaia-signin UI. Signin wallpaper is considered either as the
  // device policy wallpaper or the default wallpaper.
  virtual void ShowSigninWallpaper() = 0;

  // Shows a one-shot wallpaper, which does not belong to any particular user
  // and is not saved to file. Note: the wallpaper will never be dimmed or
  // blurred because it's assumed that the caller wants to show the image as is
  // when using this method.
  virtual void ShowOneShotWallpaper(const gfx::ImageSkia& image) = 0;

  // Shows a wallpaper that stays on top of everything except for the power off
  // animation. All other wallpaper requests are ignored when the always-on-top
  // wallpaper is being shown.
  // |image_path|: The file path to read the image data from.
  virtual void ShowAlwaysOnTopWallpaper(const base::FilePath& image_path) = 0;

  // Removes the always-on-top wallpaper. The wallpaper will revert to the
  // previous one, or a default one if there was none. No-op if the current
  // wallpaper is not always-on-top.
  virtual void RemoveAlwaysOnTopWallpaper() = 0;

  // Removes all of the user's saved wallpapers and related info.
  // |account_id|: The user's account id.
  // |wallpaper_files_id|: The file id for |account_id|.
  virtual void RemoveUserWallpaper(const AccountId& account_id,
                                   const std::string& wallpaper_files_id) = 0;

  // Removes all of the user's saved wallpapers and related info if the
  // wallpaper was set by |SetPolicyWallpaper|. In addition, sets the user's
  // wallpaper to be the default. If the user has logged in, show the default
  // wallpaper immediately, otherwise, the default wallpaper will be shown the
  // next time |ShowUserWallpaper| is called.
  // |account_id|: The user's account id.
  // |wallpaper_files_id|: The file id for |account_id|.
  virtual void RemovePolicyWallpaper(const AccountId& account_id,
                                     const std::string& wallpaper_files_id) = 0;

  // Returns the urls of the wallpapers that exist in local file system (i.e.
  // |SetOnlineWallpaperFromData| was called earlier). The url is used as id
  // to identify which wallpapers are available to be set offline.
  using GetOfflineWallpaperListCallback =
      base::OnceCallback<void(const std::vector<std::string>&)>;
  virtual void GetOfflineWallpaperList(
      GetOfflineWallpaperListCallback callback) = 0;

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

  // Returns the wallpaper prominent colors.
  virtual const std::vector<SkColor>& GetWallpaperColors() = 0;

  // Returns whether the current wallpaper is blurred.
  virtual bool IsWallpaperBlurred() = 0;

  // Returns true if the wallpaper of the currently active user (if any) is
  // controlled by policy (excluding device policy). If there's no active user,
  // returns false.
  virtual bool IsActiveUserWallpaperControlledByPolicy() = 0;

  // Returns a struct with info about the active user's wallpaper; the location
  // is an empty string and the layout is invalid if there's no active user.
  virtual WallpaperInfo GetActiveUserWallpaperInfo() = 0;

  // Returns true if the wallpaper setting (used to open the wallpaper picker)
  // should be visible.
  virtual bool ShouldShowWallpaperSetting() = 0;

 protected:
  static WallpaperController* g_instance_;
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_WALLPAPER_CONTROLLER_H_
