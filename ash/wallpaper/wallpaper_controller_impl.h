// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WALLPAPER_WALLPAPER_CONTROLLER_IMPL_H_
#define ASH_WALLPAPER_WALLPAPER_CONTROLLER_IMPL_H_

#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "ash/ash_export.h"
#include "ash/display/window_tree_host_manager.h"
#include "ash/public/cpp/tablet_mode_observer.h"
#include "ash/public/cpp/wallpaper_controller.h"
#include "ash/public/cpp/wallpaper_info.h"
#include "ash/public/cpp/wallpaper_types.h"
#include "ash/session/session_observer.h"
#include "ash/shell_observer.h"
#include "ash/wallpaper/wallpaper_utils/wallpaper_color_calculator_observer.h"
#include "ash/wallpaper/wallpaper_utils/wallpaper_resizer_observer.h"
#include "base/files/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/observer_list.h"
#include "base/timer/timer.h"
#include "ui/compositor/compositor_lock.h"
#include "ui/gfx/image/image_skia.h"

class PrefRegistrySimple;

namespace base {
class SequencedTaskRunner;
}  // namespace base

namespace color_utils {
struct ColorProfile;
}  // namespace color_utils

namespace ash {

class WallpaperColorCalculator;
class WallpaperResizer;
class WallpaperWindowStateManager;

// The |CustomWallpaperElement| contains |first| the path of the image which
// is currently being loaded and or in progress of being loaded and |second|
// the image itself.
using CustomWallpaperElement = std::pair<base::FilePath, gfx::ImageSkia>;
using CustomWallpaperMap = std::map<AccountId, CustomWallpaperElement>;

using LoadedCallback = base::OnceCallback<void(const gfx::ImageSkia& image)>;

// Controls the desktop background wallpaper:
//   - Sets a wallpaper image and layout;
//   - Handles display change (add/remove display, configuration change etc);
//   - Calculates prominent colors.
//   - Move wallpaper to locked container(s) when session state is not ACTIVE to
//     hide the user desktop and move it to unlocked container when session
//     state is ACTIVE;
class ASH_EXPORT WallpaperControllerImpl
    : public WallpaperController,
      public WindowTreeHostManager::Observer,
      public ShellObserver,
      public WallpaperResizerObserver,
      public WallpaperColorCalculatorObserver,
      public SessionObserver,
      public TabletModeObserver,
      public ui::CompositorLockClient {
 public:
  enum WallpaperResolution {
    WALLPAPER_RESOLUTION_LARGE,
    WALLPAPER_RESOLUTION_SMALL
  };

  // Directory names of custom wallpapers.
  static const char kSmallWallpaperSubDir[];
  static const char kLargeWallpaperSubDir[];
  static const char kOriginalWallpaperSubDir[];

  explicit WallpaperControllerImpl(PrefService* local_state);
  ~WallpaperControllerImpl() override;

  static void RegisterLocalStatePrefs(PrefRegistrySimple* registry);

  // Returns the maximum size of all displays combined in native
  // resolutions.  Note that this isn't the bounds of the display who
  // has maximum resolutions. Instead, this returns the size of the
  // maximum width of all displays, and the maximum height of all displays.
  static gfx::Size GetMaxDisplaySizeInNative();

  // Returns custom wallpaper path. Appends |sub_dir|, |wallpaper_files_id| and
  // |file_name| to custom wallpaper directory.
  static base::FilePath GetCustomWallpaperPath(
      const std::string& sub_dir,
      const std::string& wallpaper_files_id,
      const std::string& file_name);

  // Returns custom wallpaper directory by appending corresponding |sub_dir|.
  static base::FilePath GetCustomWallpaperDir(const std::string& sub_dir);

  // Gets |account_id|'s custom wallpaper at |wallpaper_path|. Falls back to the
  // original custom wallpaper. When |show_wallpaper| is true, shows the
  // wallpaper immediately. Must run on wallpaper sequenced worker thread.
  static void SetWallpaperFromPath(
      const AccountId& account_id,
      const WallpaperInfo& info,
      const base::FilePath& wallpaper_path,
      bool show_wallpaper,
      const scoped_refptr<base::SingleThreadTaskRunner>& reply_task_runner,
      base::WeakPtr<WallpaperControllerImpl> weak_ptr);

  // Returns the prominent color based on |color_profile|.
  SkColor GetProminentColor(color_utils::ColorProfile color_profile) const;

  // Returns current image on the wallpaper, or an empty image if there's no
  // wallpaper.
  gfx::ImageSkia GetWallpaper() const;

  // Returns the layout of the current wallpaper, or an invalid value if there's
  // no wallpaper.
  WallpaperLayout GetWallpaperLayout() const;

  // Returns the type of the current wallpaper, or an invalid value if there's
  // no wallpaper.
  WallpaperType GetWallpaperType() const;

  base::TimeDelta animation_duration() const { return animation_duration_; }

  // Returns true if the slower initial animation should be shown (as opposed to
  // the faster animation that's used e.g. when switching between different
  // wallpapers at login screen).
  bool ShouldShowInitialAnimation();

  // Returns true if the active user is allowed to open the wallpaper picker.
  bool CanOpenWallpaperPicker();

  // Returns whether any wallpaper has been shown. It returns false before the
  // first wallpaper is set (which happens momentarily after startup), and will
  // always return true thereafter.
  bool HasShownAnyWallpaper() const;

  // Shows the wallpaper and alerts observers of changes.
  // Does not show the image if:
  // 1)  |preview_mode| is false and the current wallpaper is still being
  //     previewed. See comments for |confirm_preview_wallpaper_callback_|.
  // 2)  |always_on_top| is false but the current wallpaper is always-on-top.
  void ShowWallpaperImage(const gfx::ImageSkia& image,
                          WallpaperInfo info,
                          bool preview_mode,
                          bool always_on_top);

  // Returns whether a wallpaper policy is enforced for |account_id| (not
  // including device policy).
  bool IsPolicyControlled(const AccountId& account_id) const;

  // Update the blurred state of the current wallpaper. Applies blur if |blur|
  // is true and blur is allowed by the controller, otherwise any existing blur
  // is removed.
  void UpdateWallpaperBlur(bool blur);

  // Wallpaper should be dimmed for login, lock, OOBE and add user screens.
  bool ShouldApplyDimming() const;

  // Returns whether the current wallpaper is allowed to be blurred. See
  // https://crbug.com/775591.
  bool IsBlurAllowed() const;

  // Returns whether the current wallpaper is blurred.
  // Note: this returns false when there's no wallpaper yet. Check
  // |HasShownAnyWallpaper| if there's need to distinguish.
  bool IsWallpaperBlurred() const;

  // Sets wallpaper info for |account_id| and saves it to local state if the
  // user is not ephemeral. Returns false if it fails (which happens if local
  // state is not available).
  bool SetUserWallpaperInfo(const AccountId& account_id,
                            const WallpaperInfo& info);

  // Gets wallpaper info of |account_id| from local state, or memory if the user
  // is ephemeral. Returns false if wallpaper info is not found.
  bool GetUserWallpaperInfo(const AccountId& account_id,
                            WallpaperInfo* info) const;

  // Gets encoded wallpaper from cache. Returns true if success.
  bool GetWallpaperFromCache(const AccountId& account_id,
                             gfx::ImageSkia* image);

  // Gets path of encoded wallpaper from cache. Returns true if success.
  bool GetPathFromCache(const AccountId& account_id, base::FilePath* path);

  // Runs |callback| upon the completion of the first wallpaper animation that's
  // shown on |window|'s root window.
  void AddFirstWallpaperAnimationEndCallback(base::OnceClosure callback,
                                             aura::Window* window);

  // A wrapper of |ReadAndDecodeWallpaper| used in |SetWallpaperFromPath|.
  void StartDecodeFromPath(const AccountId& account_id,
                           const base::FilePath& wallpaper_path,
                           const WallpaperInfo& info,
                           bool show_wallpaper);

  // WallpaperController:
  void SetClient(WallpaperControllerClient* client) override;
  void Init(const base::FilePath& user_data,
            const base::FilePath& wallpapers,
            const base::FilePath& custom_wallpapers,
            const base::FilePath& device_policy_wallpaper) override;
  void SetCustomWallpaper(const AccountId& account_id,
                          const std::string& wallpaper_files_id,
                          const std::string& file_name,
                          WallpaperLayout layout,
                          const gfx::ImageSkia& image,
                          bool preview_mode) override;
  void SetOnlineWallpaperIfExists(
      const AccountId& account_id,
      const std::string& url,
      WallpaperLayout layout,
      bool preview_mode,
      SetOnlineWallpaperIfExistsCallback callback) override;
  void SetOnlineWallpaperFromData(
      const AccountId& account_id,
      const std::string& image_data,
      const std::string& url,
      WallpaperLayout layout,
      bool preview_mode,
      SetOnlineWallpaperFromDataCallback callback) override;
  void SetDefaultWallpaper(const AccountId& account_id,
                           const std::string& wallpaper_files_id,
                           bool show_wallpaper) override;
  void SetCustomizedDefaultWallpaperPaths(
      const base::FilePath& customized_default_small_path,
      const base::FilePath& customized_default_large_path) override;
  void SetPolicyWallpaper(const AccountId& account_id,
                          const std::string& wallpaper_files_id,
                          const std::string& data) override;
  void SetDevicePolicyWallpaperPath(
      const base::FilePath& device_policy_wallpaper_path) override;
  bool SetThirdPartyWallpaper(const AccountId& account_id,
                              const std::string& wallpaper_files_id,
                              const std::string& file_name,
                              WallpaperLayout layout,
                              const gfx::ImageSkia& image) override;
  void ConfirmPreviewWallpaper() override;
  void CancelPreviewWallpaper() override;
  void UpdateCustomWallpaperLayout(const AccountId& account_id,
                                   WallpaperLayout layout) override;
  void ShowUserWallpaper(const AccountId& account_id) override;
  void ShowSigninWallpaper() override;
  void ShowOneShotWallpaper(const gfx::ImageSkia& image) override;
  void ShowAlwaysOnTopWallpaper(const base::FilePath& image_path) override;
  void RemoveAlwaysOnTopWallpaper() override;
  void RemoveUserWallpaper(const AccountId& account_id,
                           const std::string& wallpaper_files_id) override;
  void RemovePolicyWallpaper(const AccountId& account_id,
                             const std::string& wallpaper_files_id) override;
  void GetOfflineWallpaperList(
      GetOfflineWallpaperListCallback callback) override;
  void SetAnimationDuration(base::TimeDelta animation_duration) override;
  void OpenWallpaperPickerIfAllowed() override;
  void MinimizeInactiveWindows(const std::string& user_id_hash) override;
  void RestoreMinimizedWindows(const std::string& user_id_hash) override;
  void AddObserver(WallpaperControllerObserver* observer) override;
  void RemoveObserver(WallpaperControllerObserver* observer) override;
  gfx::ImageSkia GetWallpaperImage() override;
  const std::vector<SkColor>& GetWallpaperColors() override;
  bool IsWallpaperBlurred() override;
  bool IsActiveUserWallpaperControlledByPolicy() override;
  WallpaperInfo GetActiveUserWallpaperInfo() override;
  bool ShouldShowWallpaperSetting() override;

  // WindowTreeHostManager::Observer:
  void OnDisplayConfigurationChanged() override;

  // ShellObserver:
  void OnRootWindowAdded(aura::Window* root_window) override;
  void OnShellInitialized() override;
  void OnShellDestroying() override;

  // WallpaperResizerObserver:
  void OnWallpaperResized() override;

  // WallpaperColorCalculatorObserver:
  void OnColorCalculationComplete() override;

  // SessionObserver:
  void OnSessionStateChanged(session_manager::SessionState state) override;

  // TabletModeObserver:
  void OnTabletModeStarted() override;
  void OnTabletModeEnded() override;

  // CompositorLockClient:
  void CompositorLockTimedOut() override;

  // Shows a default wallpaper for testing, without changing users' wallpaper
  // info.
  void ShowDefaultWallpaperForTesting();

  // Creates an empty wallpaper. Some tests require a wallpaper widget is ready
  // when running. However, the wallpaper widgets are created asynchronously. If
  // loading a real wallpaper, there are cases that these tests crash because
  // the required widget is not ready. This function synchronously creates an
  // empty widget for those tests to prevent crashes.
  void CreateEmptyWallpaperForTesting();

  void set_wallpaper_reload_no_delay_for_test() {
    wallpaper_reload_delay_ = base::TimeDelta::FromMilliseconds(0);
  }

 private:
  FRIEND_TEST_ALL_PREFIXES(WallpaperControllerTest, BasicReparenting);
  FRIEND_TEST_ALL_PREFIXES(WallpaperControllerTest,
                           WallpaperMovementDuringUnlock);
  friend class WallpaperControllerTest;
  friend class WallpaperControllerTestApi;

  enum WallpaperMode { WALLPAPER_NONE, WALLPAPER_IMAGE };

  // Cached default wallpaper image and file path. The file path can be used to
  // check if the image is outdated (i.e. when there's a new default wallpaper).
  struct CachedDefaultWallpaper {
    gfx::ImageSkia image;
    base::FilePath file_path;
  };

  struct OnlineWallpaperParams {
    AccountId account_id;
    std::string url;
    WallpaperLayout layout;
    bool preview_mode;
  };

  // Creates a WallpaperWidgetController for |root_window|.
  void InstallDesktopController(aura::Window* root_window);

  // Creates a WallpaperWidgetController for all root windows.
  void InstallDesktopControllerForAllWindows();

  // Moves the wallpaper to the specified container across all root windows.
  // Returns true if a wallpaper moved.
  bool ReparentWallpaper(int container);

  // Returns the wallpaper container id for unlocked and locked states.
  int GetWallpaperContainerId(bool locked);

  // Removes |account_id|'s wallpaper info and color cache if it exists.
  void RemoveUserWallpaperInfo(const AccountId& account_id);

  // Implementation of |RemoveUserWallpaper|, which deletes |account_id|'s
  // custom wallpapers and directories.
  void RemoveUserWallpaperImpl(const AccountId& account_id,
                               const std::string& wallpaper_files_id);

  // Implementation of |SetDefaultWallpaper|. Sets wallpaper to default if
  // |show_wallpaper| is true. Otherwise just save the defaut wallpaper to
  // cache.
  void SetDefaultWallpaperImpl(const AccountId& account_id,
                               bool show_wallpaper);

  // When kiosk app is running or policy is enforced, setting a user wallpaper
  // is not allowed.
  bool CanSetUserWallpaper(const AccountId& account_id) const;

  // Returns true if the specified wallpaper is already stored in
  // |current_wallpaper_|. If |compare_layouts| is false, layout is ignored.
  bool WallpaperIsAlreadyLoaded(const gfx::ImageSkia& image,
                                bool compare_layouts,
                                WallpaperLayout layout) const;

  // Reads image from |file_path| on disk, and calls |OnWallpaperDataRead|
  // with the result of |ReadFileToString|.
  void ReadAndDecodeWallpaper(
      LoadedCallback callback,
      scoped_refptr<base::SequencedTaskRunner> task_runner,
      const base::FilePath& file_path);

  // Initializes wallpaper info for the user to default and saves it to local
  // state the user is not ephemeral. Returns false if initialization fails.
  bool InitializeUserWallpaperInfo(const AccountId& account_id);

  // Used as the callback of checking ONLINE wallpaper existence in
  // |SetOnlineWallpaperIfExists|. Initiates reading and decoding the wallpaper
  // if |file_path| is not empty.
  void SetOnlineWallpaperFromPath(SetOnlineWallpaperIfExistsCallback callback,
                                  const OnlineWallpaperParams& params,
                                  const base::FilePath& file_path);

  // Used as the callback of decoding wallpapers of type ONLINE. Saves the image
  // to local file if |save_file| is true, and shows the wallpaper immediately
  // if |params.account_id| is the active user.
  void OnOnlineWallpaperDecoded(const OnlineWallpaperParams& params,
                                bool save_file,
                                SetOnlineWallpaperFromDataCallback callback,
                                const gfx::ImageSkia& image);

  // Implementation of |SetOnlineWallpaper|. Shows the wallpaper on screen if
  // |show_wallpaper| is true.
  void SetOnlineWallpaperImpl(const OnlineWallpaperParams& params,
                              const gfx::ImageSkia& image,
                              bool show_wallpaper);

  // Decodes |account_id|'s wallpaper. Shows the decoded wallpaper if
  // |show_wallpaper| is true.
  void SetWallpaperFromInfo(const AccountId& account_id,
                            const WallpaperInfo& info,
                            bool show_wallpaper);

  // Used as the callback of default wallpaper decoding. Sets default wallpaper
  // to be the decoded image, and shows the wallpaper now if |show_wallpaper|
  // is true.
  void OnDefaultWallpaperDecoded(const base::FilePath& path,
                                 WallpaperLayout layout,
                                 bool show_wallpaper,
                                 const gfx::ImageSkia& image);

  // Saves |image| to disk if the user's data is not ephemeral, or if it is a
  // policy wallpaper for public accounts. Shows the wallpaper immediately if
  // |show_wallpaper| is true, otherwise only sets the wallpaper info and
  // updates the cache.
  void SaveAndSetWallpaper(const AccountId& account_id,
                           const std::string& wallpaper_files_id,
                           const std::string& file_name,
                           WallpaperType type,
                           WallpaperLayout layout,
                           bool show_wallpaper,
                           const gfx::ImageSkia& image);

  // Used as the callback of wallpaper decoding. (Wallpapers of type ONLINE,
  // DEFAULT and DEVICE should use their corresponding |*Decoded|, and all other
  // types should use this.) Shows the wallpaper immediately if |show_wallpaper|
  // is true. Otherwise, only updates the cache.
  void OnWallpaperDecoded(const AccountId& account_id,
                          const base::FilePath& path,
                          const WallpaperInfo& info,
                          bool show_wallpaper,
                          const gfx::ImageSkia& image);

  // Reloads the current wallpaper. It may change the wallpaper size based on
  // the current display's resolution. If |clear_cache| is true, all wallpaper
  // cache should be cleared. This is required when the display's native
  // resolution changes to a larger resolution (e.g. when hooked up a large
  // external display) and we need to load a larger resolution wallpaper for the
  // display. All the previous small resolution wallpaper cache should be
  // cleared.
  void ReloadWallpaper(bool clear_cache);

  // Sets |prominent_colors_| and notifies the observers if there is a change.
  void SetProminentColors(const std::vector<SkColor>& prominent_colors);

  // Sets all elements of |prominent_colors| to |kInvalidWallpaperColor| via
  // SetProminentColors().
  void ResetProminentColors();

  // Calculates prominent colors based on the wallpaper image and notifies
  // |observers_| of the value, either synchronously or asynchronously. In some
  // cases the wallpaper image will not actually be processed (e.g. user isn't
  // logged in, feature isn't enabled).
  // If an existing calculation is in progress it is destroyed.
  void CalculateWallpaperColors();

  // Returns false when the color extraction algorithm shouldn't be run based on
  // system state (e.g. wallpaper image, SessionState, etc.).
  bool ShouldCalculateColors() const;

  // Caches color calculation results in the local state pref service.
  void CacheProminentColors(const std::vector<SkColor>& colors,
                            const std::string& current_location);

  // Gets prominent color cache from the local state pref service. Returns an
  // empty value if the cache is not available.
  base::Optional<std::vector<SkColor>> GetCachedColors(
      const std::string& current_location) const;

  // The callback when decoding of the always-on-top wallpaper completes.
  void OnAlwaysOnTopWallpaperDecoded(const WallpaperInfo& info,
                                     const gfx::ImageSkia& image);

  // Move all wallpaper widgets to the locked container.
  // Returns true if the wallpaper moved.
  bool MoveToLockedContainer();

  // Move all wallpaper widgets to unlocked container.
  // Returns true if the wallpaper moved.
  bool MoveToUnlockedContainer();

  // Returns whether the current wallpaper is set by device policy.
  bool IsDevicePolicyWallpaper() const;

  // Returns whether the current wallpaper has type of ONE_SHOT.
  bool IsOneShotWallpaper() const;

  // Returns true if device wallpaper policy is in effect and we are at the
  // login screen right now.
  bool ShouldSetDevicePolicyWallpaper() const;

  // Reads the device wallpaper file and sets it as the current wallpaper. Note
  // when it's called, it's guaranteed that ShouldSetDevicePolicyWallpaper()
  // should be true.
  void SetDevicePolicyWallpaper();

  // Called when the device policy controlled wallpaper has been decoded.
  void OnDevicePolicyWallpaperDecoded(const gfx::ImageSkia& image);

  // When wallpaper resizes, we can check which displays will be affected. For
  // simplicity, we only lock the compositor for the internal display.
  void GetInternalDisplayCompositorLock();

  // Schedules paint on all WallpaperViews owned by WallpaperWidgetControllers.
  // This is used when we want to change wallpaper dimming.
  void RepaintWallpaper();

  bool locked_ = false;

  WallpaperMode wallpaper_mode_ = WALLPAPER_NONE;

  // Client interface in chrome browser.
  WallpaperControllerClient* wallpaper_controller_client_ = nullptr;

  base::ObserverList<WallpaperControllerObserver>::Unchecked observers_;

  std::unique_ptr<WallpaperResizer> current_wallpaper_;

  // Asynchronous task to extract colors from the wallpaper.
  std::unique_ptr<WallpaperColorCalculator> color_calculator_;

  // Manages the states of the other windows when the wallpaper app window is
  // active.
  std::unique_ptr<WallpaperWindowStateManager> window_state_manager_;

  // The prominent colors extracted from the current wallpaper.
  // kInvalidWallpaperColor is used by default or if extracting colors fails.
  std::vector<SkColor> prominent_colors_;

  // Caches the color profiles that need to do wallpaper color extracting.
  const std::vector<color_utils::ColorProfile> color_profiles_;

  // The wallpaper info for ephemeral users, which is not stored to local state.
  // See |UserInfo::is_ephemeral| for details.
  std::map<AccountId, WallpaperInfo> ephemeral_users_wallpaper_info_;

  // Account id of the current user.
  AccountId current_user_;

  // Cached wallpapers of users.
  CustomWallpaperMap wallpaper_cache_map_;

  // Cached default wallpaper.
  CachedDefaultWallpaper cached_default_wallpaper_;

  // The paths of the customized default wallpapers, if they exist.
  base::FilePath customized_default_small_path_;
  base::FilePath customized_default_large_path_;

  gfx::Size current_max_display_size_;

  base::OneShotTimer timer_;

  base::TimeDelta wallpaper_reload_delay_;

  bool is_wallpaper_blurred_ = false;

  // The wallpaper animation duration. An empty value disables the animation.
  base::TimeDelta animation_duration_;

  base::FilePath device_policy_wallpaper_path_;

  // Whether the current wallpaper (if any) is the first wallpaper since the
  // controller initialization. Empty wallpapers for testing don't count.
  bool is_first_wallpaper_ = false;

  // If true, the current wallpaper should always stay on top.
  bool is_always_on_top_wallpaper_ = false;

  scoped_refptr<base::SequencedTaskRunner> sequenced_task_runner_;

  ScopedSessionObserver scoped_session_observer_{this};

  std::unique_ptr<ui::CompositorLock> compositor_lock_;

  // A non-empty value indicates the current wallpaper is in preview mode, which
  // expects either |ConfirmPreviewWallpaper| or |CancelPreviewWallpaper| to be
  // called to exit preview. In preview mode, other types of wallpaper requests
  // may still update wallpaper info for the user, but the preview wallpaper
  // cannot be replaced, except by another preview wallpaper.
  base::OnceClosure confirm_preview_wallpaper_callback_;

  // Called when the preview wallpaper needs to be reloaded (e.g. display size
  // change). Has the same lifetime with |confirm_preview_wallpaper_callback_|.
  base::RepeatingClosure reload_preview_wallpaper_callback_;

  // Called when the always-on-top wallpaper needs to be reloaded (e.g. display
  // size change). Non-empty if and only if |is_always_on_top_wallpaper_| is
  // true.
  base::RepeatingClosure reload_always_on_top_wallpaper_callback_;

  // If true, use a solid color wallpaper as if it is the decoded image.
  bool bypass_decode_for_testing_ = false;

  // Tracks how many wallpapers have been set.
  int wallpaper_count_for_testing_ = 0;

  // The file paths of decoding requests that have been initiated. Must be a
  // list because more than one decoding requests may happen during a single
  // 'set wallpaper' request. (e.g. when a custom wallpaper decoding fails, a
  // default wallpaper decoding is initiated.)
  std::vector<base::FilePath> decode_requests_for_testing_;

  // PrefService provided by Shell when constructing this.
  // Valid for the lifetime of ash::Shell which owns WallpaperControllerImpl.
  // May be null in tests.
  PrefService* local_state_ = nullptr;

  base::WeakPtrFactory<WallpaperControllerImpl> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(WallpaperControllerImpl);
};

}  // namespace ash

#endif  // ASH_WALLPAPER_WALLPAPER_CONTROLLER_IMPL_H_
