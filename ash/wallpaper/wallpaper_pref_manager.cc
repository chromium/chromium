// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wallpaper/wallpaper_pref_manager.h"

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/public/cpp/session/session_types.h"
#include "ash/public/cpp/wallpaper/wallpaper_controller_client.h"
#include "ash/public/cpp/wallpaper/wallpaper_info.h"
#include "ash/public/cpp/wallpaper/wallpaper_types.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/wallpaper/wallpaper_utils/wallpaper_ephemeral_user.h"
#include "ash/wallpaper/wallpaper_utils/wallpaper_online_variant_utils.h"
#include "base/check.h"
#include "base/containers/adapters.h"
#include "base/containers/flat_map.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"
#include "base/types/cxx23_to_underlying.h"
#include "base/values.h"
#include "chromeos/constants/chromeos_features.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/scoped_user_pref_update.h"

namespace ash {

namespace {

// Populates |info| with the data from |pref_name| sourced from |pref_service|.
bool GetWallpaperInfo(const AccountId& account_id,
                      const PrefService* const pref_service,
                      const std::string& pref_name,
                      WallpaperInfo* info) {
  if (!pref_service) {
    return false;
  }
  const base::Value::Dict& users_dict = pref_service->GetDict(pref_name);

  const base::Value::Dict* info_dict =
      users_dict.FindDict(account_id.GetUserEmail());
  if (!info_dict) {
    return false;
  }
  std::optional<WallpaperInfo> opt_info = WallpaperInfo::FromDict(*info_dict);
  if (!opt_info) {
    return false;
  }
  *info = opt_info.value();
  return true;
}

// Populates |pref_name| with data from |info|. Returns false if the
// pref_service was inaccessbile.
bool SetWallpaperInfo(const AccountId& account_id,
                      const WallpaperInfo& info,
                      PrefService* const pref_service,
                      const std::string& pref_name) {
  if (features::IsVersionWallpaperInfoEnabled()) {
    CHECK(info.version.IsValid());
  }

  if (!pref_service) {
    return false;
  }

  DCHECK(IsAllowedInPrefs(info.type))
      << "Cannot save WallpaperType=" << base::to_underlying(info.type)
      << " to prefs";

  ScopedDictPrefUpdate wallpaper_update(pref_service, pref_name);
  wallpaper_update->Set(account_id.GetUserEmail(), info.ToDict());
  return true;
}

// Deletes the entry in |pref_name| for |account_id| from |pref_service|.
void RemoveWallpaperInfo(const AccountId& account_id,
                         PrefService* const pref_service,
                         const std::string& pref_name) {
  if (!pref_service)
    return;
  ScopedDictPrefUpdate prefs_wallpapers_info_update(pref_service, pref_name);
  prefs_wallpapers_info_update->Remove(account_id.GetUserEmail());
}

// Wrapper around SessionControllerImpl and WallpaperControllerClient to make
// this easier to test. Also, the objects can't be provided at construction
// because they're created after WallpaperControllerImpl.
class WallpaperProfileHelperImpl : public WallpaperProfileHelper {
 public:
  WallpaperProfileHelperImpl() = default;
  ~WallpaperProfileHelperImpl() override = default;

  void SetClient(WallpaperControllerClient* client) override {
    wallpaper_controller_client_ = client;
  }

  PrefService* GetUserPrefServiceSyncable(const AccountId& id) override {
    if (!IsWallpaperSyncEnabled(id))
      return nullptr;

    return Shell::Get()->session_controller()->GetUserPrefServiceForUser(id);
  }

  AccountId GetActiveAccountId() const override {
    const UserSession* const session =
        Shell::Get()->session_controller()->GetUserSession(/*user index=*/0);
    DCHECK(session);
    return session->user_info.account_id;
  }

  bool IsWallpaperSyncEnabled(const AccountId& id) const override {
    DCHECK(wallpaper_controller_client_);
    return wallpaper_controller_client_->IsWallpaperSyncEnabled(id);
  }

  bool IsActiveUserSessionStarted() const override {
    return Shell::Get()->session_controller()->IsActiveUserSessionStarted();
  }

  bool IsEphemeral(const AccountId& account_id) const override {
    return IsEphemeralUser(account_id);
  }

 private:
  raw_ptr<WallpaperControllerClient> wallpaper_controller_client_ =
      nullptr;  // not owned
};

class WallpaperPrefManagerImpl : public WallpaperPrefManager {
 public:
  WallpaperPrefManagerImpl(
      PrefService* local_state,
      std::unique_ptr<WallpaperProfileHelper> profile_helper)
      : local_state_(local_state), profile_helper_(std::move(profile_helper)) {
    // local_state is null for tests under AshTestHelper
    DCHECK(profile_helper_);
  }

  ~WallpaperPrefManagerImpl() override = default;

  void SetClient(WallpaperControllerClient* client) override {
    profile_helper_->SetClient(client);
  }

  bool GetUserWallpaperInfo(const AccountId& account_id,
                            WallpaperInfo* info) const override {
    const bool is_ephemeral = profile_helper_->IsEphemeral(account_id);
    return GetUserWallpaperInfo(account_id, is_ephemeral, info);
  }

  bool SetUserWallpaperInfo(const AccountId& account_id,
                            const WallpaperInfo& info) override {
    const bool is_ephemeral = profile_helper_->IsEphemeral(account_id);
    return SetUserWallpaperInfo(account_id, is_ephemeral, info);
  }

  bool GetUserWallpaperInfo(const AccountId& account_id,
                            bool is_ephemeral,
                            WallpaperInfo* info) const override {
    if (is_ephemeral) {
      // Ephemeral users do not save anything to local state. Return true if the
      // info can be found in the map, otherwise return false.
      auto it = ephemeral_users_wallpaper_info_.find(account_id);
      if (it == ephemeral_users_wallpaper_info_.end())
        return false;

      *info = it->second;
      return true;
    }

    return GetLocalWallpaperInfo(account_id, info);
  }

  bool SetUserWallpaperInfo(const AccountId& account_id,
                            bool is_ephemeral,
                            const WallpaperInfo& info) override {
    if (is_ephemeral) {
      ephemeral_users_wallpaper_info_.insert_or_assign(account_id, info);
      return true;
    }

    RemoveProminentColors(account_id);
    RemoveKMeanColor(account_id);

    bool success = SetLocalWallpaperInfo(account_id, info);
    // Although `WallpaperType::kCustomized` typed wallpapers are syncable, we
    // don't set synced info until the image is stored in drivefs, so we know
    // when to retry saving it on failure.
    if (ShouldSyncOut(info) && info.type != WallpaperType::kCustomized) {
      SetSyncedWallpaperInfo(account_id, info);
    }

    return success;
  }

  void RemoveUserWallpaperInfo(const AccountId& account_id) override {
    if (profile_helper_->IsEphemeral(account_id)) {
      ephemeral_users_wallpaper_info_.erase(account_id);
      return;
    }

    RemoveWallpaperInfo(account_id, local_state_, prefs::kUserWallpaperInfo);
    RemoveWallpaperInfo(account_id,
                        profile_helper_->GetUserPrefServiceSyncable(account_id),
                        GetSyncPrefName());
  }

  std::optional<WallpaperCalculatedColors> GetCachedWallpaperColors(
      std::string_view location) const override {
    std::optional<SkColor> cached_k_mean_color = GetCachedKMeanColor(location);
    std::optional<SkColor> cached_celebi_color = GetCelebiColor(location);
    if (cached_k_mean_color.has_value() && cached_celebi_color.has_value()) {
      return WallpaperCalculatedColors(cached_k_mean_color.value(),
                                       cached_celebi_color.value());
    }

    return std::nullopt;
  }

  void RemoveProminentColors(const AccountId& account_id) override {
    WallpaperInfo old_info;
    if (!GetLocalWallpaperInfo(account_id, &old_info)) {
      return;
    }

    // Remove the color cache of the previous wallpaper if it exists.
    ScopedDictPrefUpdate wallpaper_colors_update(local_state_,
                                                 prefs::kWallpaperColors);
    wallpaper_colors_update->Remove(old_info.location);
  }

  void CacheKMeanColor(std::string_view location,
                       SkColor k_mean_color) override {
    CacheSingleColor(prefs::kWallpaperMeanColors, location, k_mean_color);
  }

  std::optional<SkColor> GetCachedKMeanColor(
      std::string_view location) const override {
    return GetSingleCachedColor(prefs::kWallpaperMeanColors, location);
  }

  void RemoveKMeanColor(const AccountId& account_id) override {
    RemoveCachedColor(prefs::kWallpaperMeanColors, account_id);
  }

  void CacheCelebiColor(std::string_view location,
                        SkColor celebi_color) override {
    CacheSingleColor(prefs::kWallpaperCelebiColors, location, celebi_color);
  }
  std::optional<SkColor> GetCelebiColor(
      std::string_view location) const override {
    return GetSingleCachedColor(prefs::kWallpaperCelebiColors, location);
  }
  void RemoveCelebiColor(const AccountId& account_id) override {
    RemoveCachedColor(prefs::kWallpaperCelebiColors, account_id);
  }

  bool SetDailyGooglePhotosWallpaperIdCache(
      const AccountId& account_id,
      const WallpaperController::DailyGooglePhotosIdCache& ids) override {
    if (!local_state_)
      return false;
    ScopedDictPrefUpdate daily_google_photos_ids_update(
        local_state_, prefs::kRecentDailyGooglePhotosWallpapers);
    base::Value::List id_list;
    for (const auto& id : base::Reversed(ids)) {
      id_list.Append(base::NumberToString(id));
    }
    base::Value id_list_value(std::move(id_list));
    daily_google_photos_ids_update->Set(account_id.GetUserEmail(),
                                        std::move(id_list_value));
    return true;
  }

  bool GetDailyGooglePhotosWallpaperIdCache(
      const AccountId& account_id,
      WallpaperController::DailyGooglePhotosIdCache& ids_out) const override {
    if (!local_state_)
      return false;

    const base::Value::Dict& dict =
        local_state_->GetDict(prefs::kRecentDailyGooglePhotosWallpapers);

    const base::Value::List* id_list = dict.FindList(account_id.GetUserEmail());
    if (!id_list)
      return false;

    for (auto& id_str : *id_list) {
      uint32_t id;
      if (base::StringToUint(id_str.GetString(), &id)) {
        ids_out.Put(std::move(id));
      }
    }
    return true;
  }

  bool GetLocalWallpaperInfo(const AccountId& account_id,
                             WallpaperInfo* info) const override {
    if (!local_state_)
      return false;

    return GetWallpaperInfo(account_id, local_state_, prefs::kUserWallpaperInfo,
                            info);
  }

  bool SetLocalWallpaperInfo(const AccountId& account_id,
                             const WallpaperInfo& info) override {
    if (!local_state_)
      return false;

    return SetWallpaperInfo(account_id, info, local_state_,
                            prefs::kUserWallpaperInfo);
  }

  bool GetSyncedWallpaperInfo(const AccountId& account_id,
                              WallpaperInfo* info) const override {
    PrefService* pref_service =
        profile_helper_->GetUserPrefServiceSyncable(account_id);
    if (!pref_service)
      return false;

    return GetWallpaperInfo(account_id, pref_service, GetSyncPrefName(), info);
  }

  // Store |info| into the syncable pref service for |account_id|.
  bool SetSyncedWallpaperInfo(const AccountId& account_id,
                              const WallpaperInfo& info) override {
    PrefService* pref_service =
        profile_helper_->GetUserPrefServiceSyncable(account_id);
    if (!pref_service)
      return false;

    DCHECK(IsWallpaperTypeSyncable(info.type));

    return SetWallpaperInfo(account_id, info, pref_service, GetSyncPrefName());
  }

  bool GetSyncedWallpaperInfoFromDeprecatedPref(
      const AccountId& account_id,
      WallpaperInfo* info) const override {
    CHECK(features::IsVersionWallpaperInfoEnabled());
    PrefService* pref_service =
        profile_helper_->GetUserPrefServiceSyncable(account_id);
    if (!pref_service) {
      return false;
    }

    return GetWallpaperInfo(account_id, pref_service,
                            prefs::kSyncableWallpaperInfo, info);
  }

  void ClearDeprecatedPref(const AccountId& account_id) override {
    RemoveWallpaperInfo(account_id,
                        profile_helper_->GetUserPrefServiceSyncable(account_id),
                        prefs::kSyncableWallpaperInfo);
  }

  base::TimeDelta GetTimeToNextDailyRefreshUpdate(
      const AccountId& account_id) const override {
    WallpaperInfo info;
    if (!GetUserWallpaperInfo(account_id, &info)) {
      // Default to 1 day to avoid a continuous refresh situation.
      return base::Days(1);
    }
    base::TimeDelta delta = (info.date + base::Days(1)) - base::Time::Now();
    // Guarantee the delta is always 0 or positive.
    return delta.is_positive() ? delta : base::TimeDelta();
  }

 private:
  // Caches a single `color` in the dictionary for `pref_name`.
  void CacheSingleColor(const std::string& pref_name,
                        std::string_view location,
                        SkColor color) {
    // Blank keys are not allowed and will not be stored.
    if (location.empty()) {
      return;
    }

    ScopedDictPrefUpdate color_dict(local_state_, pref_name);
    color_dict->Set(location, static_cast<double>(color));
  }

  // Returns the cached color for `location` in `pref_name` if it can be found.
  std::optional<SkColor> GetSingleCachedColor(const std::string& pref_name,
                                              std::string_view location) const {
    // We don't support blank keys.
    if (location.empty()) {
      return std::nullopt;
    }

    const base::Value::Dict& color_dict = local_state_->GetDict(pref_name);
    auto* color_value = color_dict.Find(location);
    if (!color_value) {
      return std::nullopt;
    }
    return static_cast<SkColor>(color_value->GetDouble());
  }

  // Deletes the cached color for the current wallpaper of `account_id` in
  // `pref_name`.
  void RemoveCachedColor(const std::string& pref_name,
                         const AccountId& account_id) {
    WallpaperInfo old_info;
    if (!GetLocalWallpaperInfo(account_id, &old_info)) {
      return;
    }

    // Remove the color cache of the previous wallpaper if it exists.
    ScopedDictPrefUpdate color_dict(local_state_, pref_name);
    color_dict->Remove(old_info.location);
  }

  raw_ptr<PrefService> local_state_ = nullptr;
  std::unique_ptr<WallpaperProfileHelper> profile_helper_;

  // Cache of wallpapers for ephemeral users.
  base::flat_map<AccountId, WallpaperInfo> ephemeral_users_wallpaper_info_;
};

}  // namespace

// static
const char* WallpaperPrefManager::GetSyncPrefName() {
  return features::IsVersionWallpaperInfoEnabled()
             ? prefs::kSyncableVersionedWallpaperInfo
             : prefs::kSyncableWallpaperInfo;
}

// static
bool WallpaperPrefManager::ShouldSyncOut(const WallpaperInfo& local_info) {
  if (features::IsVersionWallpaperInfoEnabled() &&
      !local_info.version.IsValid()) {
    return false;
  }
  if (IsTimeOfDayWallpaper(local_info.collection_id)) {
    // Time Of Day wallpapers are not syncable.
    return false;
  }
  return IsWallpaperTypeSyncable(local_info.type);
}

// static
bool WallpaperPrefManager::ShouldSyncIn(const WallpaperInfo& synced_info,
                                        const WallpaperInfo& local_info,
                                        const bool is_oobe) {
  if (!IsWallpaperTypeSyncable(synced_info.type)) {
    LOG(ERROR) << " wallpaper type " << static_cast<int>(synced_info.type)
               << " from remote prefs is not syncable.";
    return false;
  }

  if (features::IsVersionWallpaperInfoEnabled()) {
    base::Version sync_version = synced_info.version;
    base::Version local_version = GetSupportedVersion(synced_info.type);
    if (!sync_version.IsValid()) {
      LOG(WARNING) << __func__ << " invalid sync version";
      return false;
    }
    if (sync_version.IsValid() && local_version.IsValid()) {
      const int remote_major_version = sync_version.components()[0];
      const int local_major_version = local_version.components()[0];
      if (remote_major_version > local_major_version) {
        return false;
      }
    }
  }

  if (synced_info.MatchesSelection(local_info)) {
    return false;
  }
  if (is_oobe) {
    // synced-in wallpaper during OOBE should always be honored. The user is
    // setting up a new device and should see the wallpaper they last set on
    // their account if it exists.
    return true;
  }
  if (synced_info.date < local_info.date) {
    return false;
  }
  if (IsTimeOfDayWallpaper(local_info.collection_id)) {
    // Time Of Day wallpapers cannot be overwritten by other wallpapers.
    return false;
  }
  if (local_info.type == WallpaperType::kSeaPen) {
    // SeaPen wallpapers cannot be overwritten by other wallpapers.
    return false;
  }
  return true;
}

// static
std::unique_ptr<WallpaperPrefManager> WallpaperPrefManager::Create(
    PrefService* local_state) {
  std::unique_ptr<WallpaperProfileHelper> profile_helper =
      std::make_unique<WallpaperProfileHelperImpl>();
  return std::make_unique<WallpaperPrefManagerImpl>(local_state,
                                                    std::move(profile_helper));
}

// static
std::unique_ptr<WallpaperPrefManager> WallpaperPrefManager::CreateForTesting(
    PrefService* local_service,
    std::unique_ptr<WallpaperProfileHelper> profile_helper) {
  return std::make_unique<WallpaperPrefManagerImpl>(local_service,
                                                    std::move(profile_helper));
}

// static
void WallpaperPrefManager::RegisterLocalStatePrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterDictionaryPref(prefs::kUserWallpaperInfo);
  registry->RegisterDictionaryPref(prefs::kWallpaperColors);
  registry->RegisterDictionaryPref(prefs::kWallpaperMeanColors);
  registry->RegisterDictionaryPref(prefs::kWallpaperCelebiColors);
  registry->RegisterDictionaryPref(prefs::kRecentDailyGooglePhotosWallpapers);
}

// static
void WallpaperPrefManager::RegisterProfilePrefs(PrefRegistrySimple* registry) {
  using user_prefs::PrefRegistrySyncable;

  registry->RegisterDictionaryPref(prefs::kSyncableWallpaperInfo,
                                   PrefRegistrySyncable::SYNCABLE_OS_PREF);
  registry->RegisterDictionaryPref(prefs::kSyncableVersionedWallpaperInfo,
                                   PrefRegistrySyncable::SYNCABLE_OS_PREF);
}

}  // namespace ash
