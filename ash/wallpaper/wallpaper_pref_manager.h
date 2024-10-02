// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WALLPAPER_WALLPAPER_PREF_MANAGER_H_
#define ASH_WALLPAPER_WALLPAPER_PREF_MANAGER_H_

#include <optional>
#include <string>
#include <string_view>

#include "ash/ash_export.h"
#include "ash/public/cpp/session/session_observer.h"
#include "ash/public/cpp/style/color_mode_observer.h"
#include "ash/public/cpp/wallpaper/wallpaper_controller.h"
#include "ash/public/cpp/wallpaper/wallpaper_info.h"
#include "ash/wallpaper/wallpaper_utils/wallpaper_calculated_colors.h"
#include "base/containers/flat_map.h"
#include "base/files/file_path.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "base/time/time.h"
#include "base/timer/wall_clock_timer.h"
#include "base/values.h"
#include "components/account_id/account_id.h"

class PrefService;
class PrefRegistrySimple;

namespace ash {

class WallpaperControllerClient;

// Interface that provides user profiles from an account id. Abstracts the
// details of the PrefService from clients and makes testing easier.
class WallpaperProfileHelper {
 public:
  virtual ~WallpaperProfileHelper() = default;

  virtual void SetClient(WallpaperControllerClient* client) = 0;

  // Returns the syncable pref service for the user with |id| if it's available.
  // Otherwise, returns null.
  virtual PrefService* GetUserPrefServiceSyncable(const AccountId& id) = 0;

  // Returns the AccountId for the currently active account.
  virtual AccountId GetActiveAccountId() const = 0;

  // Returns true iff wallpaper sync is enabled for |id|.
  virtual bool IsWallpaperSyncEnabled(const AccountId& id) const = 0;

  // Returns true if at least one user is logged in.
  virtual bool IsActiveUserSessionStarted() const = 0;

  // Returns true if the user should store data in memory only.
  virtual bool IsEphemeral(const AccountId& id) const = 0;
};

// Manages wallpaper preferences and tracks the currently configured wallpaper.
class ASH_EXPORT WallpaperPrefManager : public SessionObserver {
 public:
  // Returns the name of the syncable pref of the user's wallpaper info.
  static const char* GetSyncPrefName();
  // Determines whether the wallpaper info is syncable and should be stored in
  // synced prefs.
  static bool ShouldSyncOut(const WallpaperInfo& local_info);
  // Determines whether the local wallpaper info should by overriden by the
  // synced prefs.
  static bool ShouldSyncIn(const WallpaperInfo& synced_info,
                           const WallpaperInfo& local_info,
                           const bool is_oobe);

  static std::unique_ptr<WallpaperPrefManager> Create(PrefService* local_state);

  // Create a PrefManager where pref service retrieval can be modified through
  // |profile_helper|.
  static std::unique_ptr<WallpaperPrefManager> CreateForTesting(
      PrefService* local_state,
      std::unique_ptr<WallpaperProfileHelper> profile_helper);

  WallpaperPrefManager(const WallpaperPrefManager&) = delete;

  ~WallpaperPrefManager() override = default;

  static void RegisterLocalStatePrefs(PrefRegistrySimple* registry);
  static void RegisterProfilePrefs(PrefRegistrySimple* registry);

  virtual void SetClient(WallpaperControllerClient* client) = 0;

  // Retrieve the wallpaper preference value for |account_id| and use it to
  // populate |info|. Returns true if |info| was populated successfully.
  //
  // NOTE: WallpaperPrefManager does not enforce any checks for policy
  // enforcement. Callers must check that the user is allowed to commit the pref
  // change.
  virtual bool GetUserWallpaperInfo(const AccountId& account_id,
                                    WallpaperInfo* info) const = 0;
  virtual bool SetUserWallpaperInfo(const AccountId& account_id,
                                    const WallpaperInfo& info) = 0;

  // Overload for |GetUserWallpaperInfo| that allow callers to specify
  // whether |account_id| is ephemeral. Used for callers before signin has
  // occurred and |is_ephemeral| cannot be determined by session controller.
  virtual bool GetUserWallpaperInfo(const AccountId& account_id,
                                    bool is_ephemeral,
                                    WallpaperInfo* info) const = 0;
  // Overload for |SetUserWallpaperInfo| that allow callers to specify
  // whether |account_id| is ephemeral. Used for callers before signin has
  // occurred and |is_ephemeral| cannot be determined by session controller.
  virtual bool SetUserWallpaperInfo(const AccountId& account_id,
                                    bool is_ephemeral,
                                    const WallpaperInfo& info) = 0;

  // Remove the wallpaper entry for |account_id|.
  virtual void RemoveUserWallpaperInfo(const AccountId& account_id) = 0;

  // Returns a WallpaperCalculatedColors for a wallpaper with the corresponding
  // `location`, if one can be found. The result is synthesized from Prominent
  // and KMean colors.
  virtual std::optional<WallpaperCalculatedColors> GetCachedWallpaperColors(
      std::string_view location) const = 0;

  // DEPRECATED: Will be removed soon.
  virtual void RemoveProminentColors(const AccountId& account_id) = 0;

  virtual void CacheKMeanColor(std::string_view location,
                               SkColor k_mean_color) = 0;

  // Returns the cached KMeans color value for the wallpaper at `location`.
  virtual std::optional<SkColor> GetCachedKMeanColor(
      std::string_view location) const = 0;

  virtual void RemoveKMeanColor(const AccountId& account_id) = 0;

  // Cache the prominent color sampled with the 'Celebi' algorithm.
  virtual void CacheCelebiColor(std::string_view location,
                                SkColor celebi_color) = 0;
  // Returns the cached celebi color for the wallpaper at `location`.
  virtual std::optional<SkColor> GetCelebiColor(
      std::string_view location) const = 0;
  virtual void RemoveCelebiColor(const AccountId& account_id) = 0;

  virtual bool SetDailyGooglePhotosWallpaperIdCache(
      const AccountId& account_id,
      const WallpaperController::DailyGooglePhotosIdCache& ids) = 0;
  virtual bool GetDailyGooglePhotosWallpaperIdCache(
      const AccountId& account_id,
      WallpaperController::DailyGooglePhotosIdCache& ids_out) const = 0;

  // Get/Set the wallpaper info in local storage.
  // TODO(crbug.com/1298586): Remove these functions from the interface when
  // this can be abstracted from callers.
  virtual bool GetLocalWallpaperInfo(const AccountId& account_id,
                                     WallpaperInfo* info) const = 0;
  virtual bool SetLocalWallpaperInfo(const AccountId& account_id,
                                     const WallpaperInfo& info) = 0;

  // Get/Set the wallpaper info from synced prefs.
  // TODO(crbug.com/1298586): Remove these functions from the interface when
  // this can be abstracted from callers.
  virtual bool GetSyncedWallpaperInfo(const AccountId& account_id,
                                      WallpaperInfo* info) const = 0;
  virtual bool SetSyncedWallpaperInfo(const AccountId& account_id,
                                      const WallpaperInfo& info) = 0;

  // Gets the wallpaper info from the deprecated synced prefs
  // `kSyncableWallpaperInfo`.
  virtual bool GetSyncedWallpaperInfoFromDeprecatedPref(
      const AccountId& account_id,
      WallpaperInfo* info) const = 0;

  // Clears the deprecated synced prefs `kSyncableWallpaperInfo`.
  virtual void ClearDeprecatedPref(const AccountId& account_id) = 0;

  // Returns the delta for the next daily refresh update for `account_id`.
  virtual base::TimeDelta GetTimeToNextDailyRefreshUpdate(
      const AccountId& account_id) const = 0;

 protected:
  WallpaperPrefManager() = default;
};

}  // namespace ash

#endif  // ASH_WALLPAPER_WALLPAPER_PREF_MANAGER_H_
