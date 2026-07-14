// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ntp_customization/ntp_android_custom_background_service.h"

#include <climits>

#include "chrome/browser/ntp_customization/ntp_android_background_service_factory.h"
#include "chrome/browser/ntp_customization/ntp_synced_theme_bridge.h"
#include "chrome/browser/ntp_customization/ntp_theme_collection_bridge.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/themes/ntp_background_service.h"

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
    Profile* profile)
    : NtpCustomBackgroundServiceBase(
          profile->GetPrefs(),
          NtpAndroidBackgroundServiceFactory::GetForProfile(profile),
          prefs::kNtpAndroidCustomBackgroundDict,
          prefs::kNtpAndroidCustomBackgroundLocalToDevice) {
  active_custom_background_ = GetCustomBackground();
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
  active_custom_background_ = std::nullopt;
  pref_service_->SetBoolean(prefs::kNtpAndroidCustomBackgroundLocalToDevice,
                            true);
}

std::optional<int> NtpAndroidCustomBackgroundService::GetNextRefreshTimestamp()
    const {
  // Return a fake timestamp so that the base class correctly sets
  // daily_refresh_enabled to true. Actual daily refresh scheduling on Android
  // is handled by NtpThemeDailyRefreshManager.
  return INT_MAX;
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
  std::optional<CustomBackground> current = GetCustomBackground();
  if (!current) {
    // Background was reset.
    active_custom_background_ = std::nullopt;
    NtpCustomBackgroundServiceBase::NotifyAboutBackgrounds();
    return;
  }

  if (current->daily_refresh_enabled) {
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
