// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wallpaper/wallpaper_pref_manager.h"

#include <cstdint>
#include <string>
#include <vector>

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
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/rand_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chromeos/constants/chromeos_features.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace ash {

namespace {

constexpr bool IsWallpaperTypeSyncable(WallpaperType type) {
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

// Populates online wallpaper related info in |info|.
void PopulateOnlineWallpaperInfo(WallpaperInfo* info,
                                 const base::Value::Dict& info_dict) {
  const std::string* asset_id_str =
      info_dict.FindString(WallpaperPrefManager::kNewWallpaperAssetIdNodeName);
  const std::string* collection_id = info_dict.FindString(
      WallpaperPrefManager::kNewWallpaperCollectionIdNodeName);
  const std::string* dedup_key =
      info_dict.FindString(WallpaperPrefManager::kNewWallpaperDedupKeyNodeName);
  const std::string* unit_id_str =
      info_dict.FindString(WallpaperPrefManager::kNewWallpaperUnitIdNodeName);
  const base::Value::List* variant_list = info_dict.FindList(
      WallpaperPrefManager::kNewWallpaperVariantListNodeName);

  info->collection_id = collection_id ? *collection_id : std::string();
  info->dedup_key = dedup_key ? absl::make_optional(*dedup_key) : absl::nullopt;

  if (asset_id_str) {
    uint64_t asset_id;
    if (base::StringToUint64(*asset_id_str, &asset_id))
      info->asset_id = absl::make_optional(asset_id);
  }
  if (unit_id_str) {
    uint64_t unit_id;
    if (base::StringToUint64(*unit_id_str, &unit_id))
      info->unit_id = absl::make_optional(unit_id);
  }
  if (variant_list) {
    std::vector<OnlineWallpaperVariant> variants;
    for (const auto& variant_info_value : *variant_list) {
      if (!variant_info_value.is_dict()) {
        continue;
      }
      const base::Value::Dict& variant_info = variant_info_value.GetDict();
      const std::string* variant_asset_id_str = variant_info.FindString(
          WallpaperPrefManager::kNewWallpaperAssetIdNodeName);
      const std::string* url = variant_info.FindString(
          WallpaperPrefManager::kOnlineWallpaperUrlNodeName);
      absl::optional<int> type = variant_info.FindInt(
          WallpaperPrefManager::kOnlineWallpaperTypeNodeName);
      if (variant_asset_id_str && url && type.has_value()) {
        uint64_t variant_asset_id;
        if (base::StringToUint64(*variant_asset_id_str, &variant_asset_id))
          variants.emplace_back(
              variant_asset_id, GURL(*url),
              static_cast<backdrop::Image::ImageType>(type.value()));
      }
    }
    info->variants = std::move(variants);
  }
}

// Populates |info| with the data from |pref_name| sourced from |pref_service|.
bool GetWallpaperInfo(const AccountId& account_id,
                      const PrefService* const pref_service,
                      const std::string& pref_name,
                      WallpaperInfo* info) {
  if (!pref_service)
    return false;

  const base::Value::Dict& users_dict = pref_service->GetDict(pref_name);

  const base::Value::Dict* info_dict =
      users_dict.FindDict(account_id.GetUserEmail());
  if (!info_dict)
    return false;

  // Use temporary variables to keep |info| untouched in the error case.
  const std::string* location = info_dict->FindString(
      WallpaperPrefManager::kNewWallpaperLocationNodeName);
  const std::string* file_path = info_dict->FindString(
      WallpaperPrefManager::kNewWallpaperUserFilePathNodeName);
  absl::optional<int> layout =
      info_dict->FindInt(WallpaperPrefManager::kNewWallpaperLayoutNodeName);
  absl::optional<int> type =
      info_dict->FindInt(WallpaperPrefManager::kNewWallpaperTypeNodeName);
  const std::string* date_string =
      info_dict->FindString(WallpaperPrefManager::kNewWallpaperDateNodeName);

  if (!location || !layout || !type || !date_string)
    return false;

  if (type.value() >= static_cast<int>(WallpaperType::kCount))
    return false;

  WallpaperType wallpaper_type = static_cast<WallpaperType>(type.value());
  info->type = wallpaper_type;

  int64_t date_val;
  if (!base::StringToInt64(*date_string, &date_val))
    return false;

  info->location = *location;
  // The old wallpaper didn't include file path information. For migration,
  // check whether file_path is a null pointer before setting user_file_path.
  info->user_file_path = file_path ? *file_path : "";
  info->layout = static_cast<WallpaperLayout>(layout.value());
  // TODO(skau): Switch to TimeFromValue
  info->date =
      base::Time::FromDeltaSinceWindowsEpoch(base::Microseconds(date_val));
  PopulateOnlineWallpaperInfo(info, *info_dict);
  return true;
}

// Populates |pref_name| with data from |info|. Returns false if the
// pref_service was inaccessbile.
bool SetWallpaperInfo(const AccountId& account_id,
                      const WallpaperInfo& info,
                      PrefService* const pref_service,
                      const std::string& pref_name) {
  if (!pref_service)
    return false;

  ScopedDictPrefUpdate wallpaper_update(pref_service, pref_name);
  base::Value::Dict wallpaper_info_dict;
  if (info.asset_id.has_value()) {
    wallpaper_info_dict.Set(WallpaperPrefManager::kNewWallpaperAssetIdNodeName,
                            base::NumberToString(info.asset_id.value()));
  }
  if (info.unit_id.has_value()) {
    wallpaper_info_dict.Set(WallpaperPrefManager::kNewWallpaperUnitIdNodeName,
                            base::NumberToString(info.unit_id.value()));
  }
  base::Value::List online_wallpaper_variant_list;
  for (const auto& variant : info.variants) {
    base::Value::Dict online_wallpaper_variant_dict;
    online_wallpaper_variant_dict.Set(
        WallpaperPrefManager::kNewWallpaperAssetIdNodeName,
        base::NumberToString(variant.asset_id));
    online_wallpaper_variant_dict.Set(
        WallpaperPrefManager::kOnlineWallpaperUrlNodeName,
        variant.raw_url.spec());
    online_wallpaper_variant_dict.Set(
        WallpaperPrefManager::kOnlineWallpaperTypeNodeName,
        static_cast<int>(variant.type));
    online_wallpaper_variant_list.Append(
        std::move(online_wallpaper_variant_dict));
  }

  wallpaper_info_dict.Set(
      WallpaperPrefManager::kNewWallpaperVariantListNodeName,
      std::move(online_wallpaper_variant_list));
  wallpaper_info_dict.Set(
      WallpaperPrefManager::kNewWallpaperCollectionIdNodeName,
      info.collection_id);
  // TODO(skau): Change time representation to TimeToValue.
  wallpaper_info_dict.Set(
      WallpaperPrefManager::kNewWallpaperDateNodeName,
      base::NumberToString(
          info.date.ToDeltaSinceWindowsEpoch().InMicroseconds()));
  if (info.dedup_key) {
    wallpaper_info_dict.Set(WallpaperPrefManager::kNewWallpaperDedupKeyNodeName,
                            info.dedup_key.value());
  }
  wallpaper_info_dict.Set(WallpaperPrefManager::kNewWallpaperLocationNodeName,
                          info.location);
  wallpaper_info_dict.Set(
      WallpaperPrefManager::kNewWallpaperUserFilePathNodeName,
      info.user_file_path);
  wallpaper_info_dict.Set(WallpaperPrefManager::kNewWallpaperLayoutNodeName,
                          info.layout);
  wallpaper_info_dict.Set(WallpaperPrefManager::kNewWallpaperTypeNodeName,
                          static_cast<int>(info.type));
  wallpaper_update->Set(account_id.GetUserEmail(),
                        std::move(wallpaper_info_dict));
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
  base::raw_ptr<WallpaperControllerClient> wallpaper_controller_client_ =
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
    RemoveWallpaperInfo(account_id, local_state_, prefs::kUserWallpaperInfo);
    RemoveWallpaperInfo(account_id,
                        profile_helper_->GetUserPrefServiceSyncable(account_id),
                        prefs::kSyncableWallpaperInfo);
  }

  absl::optional<WallpaperCalculatedColors> GetCachedWallpaperColors(
      base::StringPiece location) const override {
    absl::optional<std::vector<SkColor>> cached_colors =
        GetCachedProminentColors(location);
    absl::optional<SkColor> cached_k_mean_color = GetCachedKMeanColor(location);
    if (!chromeos::features::IsJellyEnabled()) {
      if (cached_colors.has_value() && cached_k_mean_color.has_value()) {
        return WallpaperCalculatedColors(cached_colors.value(),
                                         cached_k_mean_color.value(),
                                         SK_ColorTRANSPARENT);
      }

      return absl::nullopt;
    }

    absl::optional<SkColor> cached_celebi_color = GetCelebiColor(location);
    if (cached_k_mean_color.has_value() && cached_celebi_color.has_value()) {
      return WallpaperCalculatedColors({}, cached_k_mean_color.value(),
                                       cached_celebi_color.value());
    }

    return absl::nullopt;
  }

  void CacheProminentColors(base::StringPiece location,
                            const std::vector<SkColor>& colors) override {
    if (location.empty()) {
      return;
    }

    ScopedDictPrefUpdate wallpaper_colors_update(local_state_,
                                                 prefs::kWallpaperColors);
    base::Value::List wallpaper_colors;
    for (SkColor color : colors)
      wallpaper_colors.Append(static_cast<double>(color));
    base::Value wallpaper_colors_value(std::move(wallpaper_colors));
    wallpaper_colors_update->Set(location, std::move(wallpaper_colors_value));
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

  absl::optional<std::vector<SkColor>> GetCachedProminentColors(
      const base::StringPiece location) const override {
    if (location.empty())
      return absl::nullopt;

    const base::Value::List* prominent_colors =
        local_state_->GetDict(prefs::kWallpaperColors).FindList(location);
    if (!prominent_colors)
      return absl::nullopt;

    absl::optional<std::vector<SkColor>> cached_colors_out;
    cached_colors_out = std::vector<SkColor>();
    cached_colors_out.value().reserve(prominent_colors->size());
    for (const auto& value : *prominent_colors) {
      cached_colors_out.value().push_back(
          static_cast<SkColor>(value.GetDouble()));
    }
    return cached_colors_out;
  }

  void CacheKMeanColor(base::StringPiece location,
                       SkColor k_mean_color) override {
    CacheSingleColor(prefs::kWallpaperMeanColors, location, k_mean_color);
  }

  absl::optional<SkColor> GetCachedKMeanColor(
      const base::StringPiece location) const override {
    return GetSingleCachedColor(prefs::kWallpaperMeanColors, location);
  }

  void RemoveKMeanColor(const AccountId& account_id) override {
    RemoveCachedColor(prefs::kWallpaperMeanColors, account_id);
  }

  void CacheCelebiColor(base::StringPiece location,
                        SkColor celebi_color) override {
    CacheSingleColor(prefs::kWallpaperCelebiColors, location, celebi_color);
  }
  absl::optional<SkColor> GetCelebiColor(
      const base::StringPiece location) const override {
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

    return GetWallpaperInfo(account_id, pref_service,
                            prefs::kSyncableWallpaperInfo, info);
  }

  // Store |info| into the syncable pref service for |account_id|.
  bool SetSyncedWallpaperInfo(const AccountId& account_id,
                              const WallpaperInfo& info) override {
    PrefService* pref_service =
        profile_helper_->GetUserPrefServiceSyncable(account_id);
    if (!pref_service)
      return false;

    return SetWallpaperInfo(account_id, info, pref_service,
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
                        base::StringPiece location,
                        SkColor color) {
    // Blank keys are not allowed and will not be stored.
    if (location.empty()) {
      return;
    }

    ScopedDictPrefUpdate color_dict(local_state_, pref_name);
    color_dict->Set(location, static_cast<double>(color));
  }

  // Returns the cached color for `location` in `pref_name` if it can be found.
  absl::optional<SkColor> GetSingleCachedColor(
      const std::string& pref_name,
      base::StringPiece location) const {
    // We don't support blank keys.
    if (location.empty()) {
      return absl::nullopt;
    }

    const base::Value::Dict& color_dict = local_state_->GetDict(pref_name);
    auto* color_value = color_dict.Find(location);
    if (!color_value) {
      return absl::nullopt;
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

  raw_ptr<PrefService, ExperimentalAsh> local_state_ = nullptr;
  std::unique_ptr<WallpaperProfileHelper> profile_helper_;

  // Cache of wallpapers for ephemeral users.
  base::flat_map<AccountId, WallpaperInfo> ephemeral_users_wallpaper_info_;
};

}  // namespace

const char WallpaperPrefManager::kNewWallpaperAssetIdNodeName[] = "asset_id";
const char WallpaperPrefManager::kNewWallpaperCollectionIdNodeName[] =
    "collection_id";
const char WallpaperPrefManager::kNewWallpaperDateNodeName[] = "date";
const char WallpaperPrefManager::kNewWallpaperDedupKeyNodeName[] = "dedup_key";
const char WallpaperPrefManager::kNewWallpaperLayoutNodeName[] = "layout";
const char WallpaperPrefManager::kNewWallpaperLocationNodeName[] = "file";
const char WallpaperPrefManager::kNewWallpaperUserFilePathNodeName[] =
    "file_path";
const char WallpaperPrefManager::kNewWallpaperTypeNodeName[] = "type";
const char WallpaperPrefManager::kNewWallpaperUnitIdNodeName[] = "unit_id";
const char WallpaperPrefManager::kNewWallpaperVariantListNodeName[] =
    "variants";
const char WallpaperPrefManager::kOnlineWallpaperTypeNodeName[] =
    "online_image_type";
const char WallpaperPrefManager::kOnlineWallpaperUrlNodeName[] = "url";

// static
bool WallpaperPrefManager::ShouldSyncOut(const WallpaperInfo& local_info) {
  if (IsTimeOfDayWallpaper(local_info)) {
    // Time Of Day wallpapers are not syncable.
    // TODO(b/277804153): Confirm the sync rules for time of day wallpapers.
    return false;
  }
  return IsWallpaperTypeSyncable(local_info.type);
}

// static
bool WallpaperPrefManager::ShouldSyncIn(const WallpaperInfo& synced_info,
                                        const WallpaperInfo& local_info) {
  if (!IsWallpaperTypeSyncable(synced_info.type)) {
    LOG(ERROR) << " wallpaper type " << static_cast<int>(synced_info.type)
               << " from remote prefs is not syncable.";
    return false;
  }
  if (synced_info.MatchesSelection(local_info)) {
    return false;
  }
  if (synced_info.date < local_info.date) {
    return false;
  }
  // TODO(b/277804153): Confirm the sync rules for time of day wallpapers.
  if (IsTimeOfDayWallpaper(local_info)) {
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
}

}  // namespace ash
