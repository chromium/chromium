// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ntp_customization/ntp_android_custom_background_service.h"

#include <climits>

#include "base/auto_reset.h"
#include "base/debug/dump_without_crashing.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "build/android_buildflags.h"
#include "chrome/browser/ntp_customization/ntp_android_background_service_factory.h"
#include "chrome/browser/ntp_customization/ntp_android_theme_sync_bridge.h"
#include "chrome/browser/ntp_customization/ntp_synced_theme_bridge.h"
#include "chrome/browser/ntp_customization/ntp_theme_collection_bridge.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/sync/base/features.h"
#include "components/sync/model/client_tag_based_data_type_processor.h"
#include "components/sync/protocol/theme_android_specifics.pb.h"
#include "components/sync/protocol/theme_types.pb.h"
#include "components/themes/ntp_background_service.h"
#include "components/themes/ntp_custom_background_service_constants.h"
#include "components/themes/theme_utils.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/webui/buildflags.h"

// static
void NtpAndroidCustomBackgroundService::RegisterProfilePrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterDictionaryPref(
      prefs::kNtpAndroidCustomBackgroundDict,
      NtpCustomBackgroundServiceBase::NtpCustomBackgroundDefaults());
  registry->RegisterBooleanPref(prefs::kNtpAndroidCustomBackgroundLocalToDevice,
                                false);
}

NtpAndroidCustomBackgroundService::NtpAndroidCustomBackgroundService(
    Profile* profile,
    syncer::OnceDataTypeStoreFactory store_factory)
    : NtpCustomBackgroundServiceBase(
          profile->GetPrefs(),
          NtpAndroidBackgroundServiceFactory::GetForProfile(profile),
          prefs::kNtpAndroidCustomBackgroundDict,
          prefs::kNtpAndroidCustomBackgroundLocalToDevice) {
  active_custom_background_ = GetCustomBackground();

  // TODO(crbug.com/488439751): Desktop Android devices manage NTP themes using
  // desktop UI/theme mechanisms. Skip initializing theme sync bridge on
  // Desktop Android devices until Desktop Android theme sync requirements are
  // finalized.
#if !BUILDFLAG(ENABLE_WEBUI_NTP)
  if (base::FeatureList::IsEnabled(syncer::kNewTabPageCustomizationThemeSync)) {
    theme_sync_bridge_ =
        std::make_unique<ntp_customization::NtpAndroidThemeSyncBridge>(
            std::make_unique<syncer::ClientTagBasedDataTypeProcessor>(
                syncer::THEMES_ANDROID,
                /*dump_stack=*/base::BindRepeating(
                    []() { base::debug::DumpWithoutCrashing(); })),
            std::move(store_factory),
            base::BindRepeating(
                &NtpAndroidCustomBackgroundService::OnThemeChangedFromSync,
                weak_ptr_factory_.GetWeakPtr()));
  }
#endif
}

NtpAndroidCustomBackgroundService::~NtpAndroidCustomBackgroundService() {
  if (theme_collection_bridge_) {
    theme_collection_bridge_->DisconnectCustomBackgroundService();
  }
  if (synced_theme_bridge_) {
    synced_theme_bridge_->DisconnectCustomBackgroundService();
  }
}

void NtpAndroidCustomBackgroundService::SelectLocalBackgroundImage(
    const base::FilePath& path) {
  processing_sync_update_ = false;
  active_custom_background_ = std::nullopt;
  pref_service_->SetBoolean(prefs::kNtpAndroidCustomBackgroundLocalToDevice,
                            true);
  NotifySyncBridge();
}

std::optional<int> NtpAndroidCustomBackgroundService::GetNextRefreshTimestamp()
    const {
  // Return a fake timestamp so that the base class correctly sets
  // daily_refresh_enabled to true. Actual daily refresh scheduling on Android
  // is handled by NtpThemeDailyRefreshManager.
  return INT_MAX;
}

base::WeakPtr<syncer::DataTypeControllerDelegate>
NtpAndroidCustomBackgroundService::GetSyncControllerDelegate() {
  if (!theme_sync_bridge_) {
    return nullptr;
  }
  return theme_sync_bridge_->change_processor()->GetControllerDelegate();
}

void NtpAndroidCustomBackgroundService::SetThemeCollectionBridge(
    NtpThemeCollectionBridge* bridge) {
  theme_collection_bridge_ = bridge;
}

void NtpAndroidCustomBackgroundService::SetSyncedThemeBridge(
    NtpSyncedThemeBridge* bridge) {
  synced_theme_bridge_ = bridge;
}

void NtpAndroidCustomBackgroundService::SetCustomBackgroundInfo(
    const GURL& background_url,
    const GURL& thumbnail_url,
    const std::string& attribution_line_1,
    const std::string& attribution_line_2,
    const GURL& action_url,
    const std::string& collection_id) {
  processing_sync_update_ = false;
  if (!background_url.is_valid() && !collection_id.empty()) {
    // Daily refresh setup.
    CustomBackground active;
    active.daily_refresh_enabled = true;
    active.collection_id = collection_id;
    active.custom_background_url = GURL();
    active_custom_background_ = active;
  } else if (background_url.is_valid()) {
    // Static image selection.
    CustomBackground active;
    active.daily_refresh_enabled = false;
    active.collection_id = collection_id;
    active.custom_background_url = background_url;
    active_custom_background_ = active;
  } else {
    // Reset.
    active_custom_background_ = std::nullopt;
  }
  NtpCustomBackgroundServiceBase::SetCustomBackgroundInfo(
      background_url, thumbnail_url, attribution_line_1, attribution_line_2,
      action_url, collection_id);
  // TODO(crbug.com/488439751): For daily refresh setup, NotifySyncBridge pushes
  // stale background data from PrefService because the first daily image fetch
  // is asynchronous. Defer sync notification until the new daily image and its
  // primary color are fetched and updated.
  NotifySyncBridge();
}

void NtpAndroidCustomBackgroundService::ResetCustomBackgroundInfo() {
  processing_sync_update_ = false;
  NtpCustomBackgroundServiceBase::ResetCustomBackgroundInfo();
  NotifySyncBridge();
}

void NtpAndroidCustomBackgroundService::OnNextCollectionImageAvailable() {
  auto image = background_service_->next_image();
  if (!active_custom_background_ ||
      !active_custom_background_->daily_refresh_enabled ||
      active_custom_background_->collection_id != image.collection_id) {
    // Stale update or daily refresh disabled. Ignore.
    return;
  }
  NtpCustomBackgroundServiceBase::OnNextCollectionImageAvailable();
}

void NtpAndroidCustomBackgroundService::NotifyAboutBackgrounds() {
  // When only the primary color is updated in the preference dictionary, skip
  // notifying observers to avoid triggering redundant background image
  // re-fetches and color recalculation loops, since Android calculates the
  // primary color on the Java side after fetching the bitmap.
  if (updating_color_pref_) {
    return;
  }

  std::optional<CustomBackground> current = GetCustomBackground();
  if (!current) {
    // Background was reset.
    // TODO(crbug.com/488439751): Handle the transition back to the default
    // background and Chrome colors when theme is reset from sync.
    active_custom_background_ = std::nullopt;
    NtpCustomBackgroundServiceBase::NotifyAboutBackgrounds();
    return;
  }

  if (processing_sync_update_) {
    if (synced_theme_bridge_) {
      synced_theme_bridge_->OnCustomBackgroundImageUpdated();
    }
  } else if (current->daily_refresh_enabled) {
    // Route the update based on whether this is a background pre-fetch for
    // the next daily refresh cycle or the initial daily refresh setup.
    if (IsNextThemeCollectionImage(*current)) {
      // Pre-fetch update for the next cycle: notify the synced theme bridge
      // to pre-cache the image without applying it to the current NTP UI
      // immediately.
      if (synced_theme_bridge_) {
        synced_theme_bridge_->OnCustomBackgroundImageUpdated();
      }
    } else {
      // Initial setup for daily refresh: notify the theme collection bridge
      // so the UI updates to show the selected image immediately.
      if (theme_collection_bridge_) {
        theme_collection_bridge_->OnCustomBackgroundImageUpdated();
      }
    }
  } else {
    // Static image selection: notify the theme collection bridge to update
    // the UI immediately.
    if (theme_collection_bridge_) {
      theme_collection_bridge_->OnCustomBackgroundImageUpdated();
    }
  }

  // Update the cached background state.
  if (active_custom_background_ &&
      active_custom_background_->collection_id == current->collection_id) {
    // If we are in the same collection, only update the URL to avoid
    // overwriting the daily refresh state during subsequent image fetches.
    active_custom_background_->custom_background_url =
        current->custom_background_url;
  } else {
    active_custom_background_ = current;
  }

  NtpCustomBackgroundServiceBase::NotifyAboutBackgrounds();
}

bool NtpAndroidCustomBackgroundService::IsNextThemeCollectionImage(
    const CustomBackground& new_info) {
  return active_custom_background_ &&
         active_custom_background_->daily_refresh_enabled &&
         new_info.daily_refresh_enabled &&
         !active_custom_background_->custom_background_url.is_empty() &&
         active_custom_background_->collection_id == new_info.collection_id;
}

bool NtpAndroidCustomBackgroundService::UpdateCustomBackgroundPrefsWithColor(
    const GURL& image_url,
    SkColor color) {
  base::AutoReset<bool> auto_reset(&updating_color_pref_, true);
  if (!NtpCustomBackgroundServiceBase::UpdateCustomBackgroundPrefsWithColor(
          image_url, color)) {
    return false;
  }
  NotifySyncBridge();
  return true;
}

void NtpAndroidCustomBackgroundService::OnThemeChangedFromSync(
    const sync_pb::ThemeAndroidSpecifics& specifics) {
  // TODO(crbug.com/488439751): Skip applying sync theme changes on Desktop
  // Android devices.
#if BUILDFLAG(ENABLE_WEBUI_NTP)
  return;
#else
  processing_sync_update_ = true;
  if (specifics.has_ntp_background()) {
    base::DictValue dict =
        themes::GetBackgroundDictFromProto(specifics.ntp_background());
    if (!dict.empty()) {
      pref_service_->SetDict(prefs::kNtpAndroidCustomBackgroundDict,
                             std::move(dict));
      return;
    }
  }
  pref_service_->ClearPref(prefs::kNtpAndroidCustomBackgroundDict);
#endif
}

void NtpAndroidCustomBackgroundService::NotifySyncBridge() {
  // TODO(crbug.com/488439751): Skip pushing theme updates to sync bridge on
  // Desktop Android devices.
#if BUILDFLAG(ENABLE_WEBUI_NTP)
  return;
#else
  if (!theme_sync_bridge_) {
    return;
  }
  sync_pb::ThemeAndroidSpecifics specifics;
  if (!pref_service_->GetBoolean(
          prefs::kNtpAndroidCustomBackgroundLocalToDevice)) {
    const base::Value* pref =
        pref_service_->GetUserPrefValue(prefs::kNtpAndroidCustomBackgroundDict);
    if (pref && pref->is_dict() && !pref->GetDict().empty()) {
      *specifics.mutable_ntp_background() =
          themes::GetProtoFromBackgroundDict(pref->GetDict());

      if (std::optional<int> main_color =
              pref->GetDict().FindInt(kNtpCustomBackgroundMainColor)) {
        sync_pb::UserColorTheme* user_color_theme =
            specifics.mutable_user_color_theme();
        user_color_theme->set_color(static_cast<uint32_t>(*main_color));
        user_color_theme->set_browser_color_variant(
            sync_pb::UserColorTheme::TONAL_SPOT);
      }
    }
  }
  theme_sync_bridge_->UpdateTheme(specifics);
#endif
}
