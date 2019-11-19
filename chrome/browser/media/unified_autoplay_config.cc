// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/unified_autoplay_config.h"

#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"

// static
void UnifiedAutoplayConfig::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterBooleanPref(prefs::kBlockAutoplayEnabled, true);
}

// static
bool UnifiedAutoplayConfig::ShouldBlockAutoplay(Profile* profile) {
  return !IsBlockAutoplayUserModifiable(profile) ||
         profile->GetPrefs()->GetBoolean(prefs::kBlockAutoplayEnabled);
}

// static
bool UnifiedAutoplayConfig::IsBlockAutoplayUserModifiable(Profile* profile) {
  HostContentSettingsMap* settings_map =
      HostContentSettingsMapFactory::GetForProfile(profile);
  return settings_map->GetDefaultContentSetting(
             ContentSettingsType::SOUND, nullptr) != CONTENT_SETTING_BLOCK;
}
