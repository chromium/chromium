// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/intent_helper/intent_chip_display_prefs.h"

#include "base/values.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/content_settings/core/common/content_settings_utils.h"

namespace {

// Show the Intent Chip as collapsed after it has been shown expanded
// |kIntentChipCollapseThreshold| times.
constexpr int kIntentChipCollapseThreshold = 3;

constexpr char kIntentChipCountKey[] = "intent_chip_display_count";

// Retrieves or creates a new dictionary for the specific |url|.
base::Value GetAutoDisplayDictForSettings(
    const HostContentSettingsMap* settings,
    const GURL& url) {
  if (!settings) {
    return base::Value(base::Value::Type::DICT);
  }

  base::Value value = settings->GetWebsiteSetting(
      url, url, ContentSettingsType::INTENT_PICKER_DISPLAY, /*info=*/nullptr);

  if (value.type() != base::Value::Type::DICT) {
    return base::Value(base::Value::Type::DICT);
  }

  // Remove obsolete keys, if they are found. These keys were recorded on CrOS
  // until M108.
  value.GetDict().Remove("picker_platform_key");
  value.GetDict().Remove("picker_auto_display_key");
  return value;
}

}  // namespace

// static
IntentChipDisplayPrefs::ChipState
IntentChipDisplayPrefs::GetChipStateAndIncrementCounter(Profile* profile,
                                                        const GURL& url) {
  auto* settings_map = HostContentSettingsMapFactory::GetForProfile(profile);
  base::Value pref_dict = GetAutoDisplayDictForSettings(settings_map, url);

  int display_count =
      pref_dict.GetDict().FindInt(kIntentChipCountKey).value_or(0);
  if (display_count >= kIntentChipCollapseThreshold) {
    // Exit before updating the counter so we don't keep counting indefinitely.
    return ChipState::kCollapsed;
  }

  pref_dict.GetDict().Set(kIntentChipCountKey, ++display_count);
  settings_map->SetWebsiteSettingDefaultScope(
      url, url, ContentSettingsType::INTENT_PICKER_DISPLAY,
      std::move(pref_dict));

  return ChipState::kExpanded;
}

// static
void IntentChipDisplayPrefs::ResetIntentChipCounter(Profile* profile,
                                                    const GURL& url) {
  auto* settings_map = HostContentSettingsMapFactory::GetForProfile(profile);
  base::Value pref_dict = GetAutoDisplayDictForSettings(settings_map, url);

  pref_dict.GetDict().Set(kIntentChipCountKey, 0);

  settings_map->SetWebsiteSettingDefaultScope(
      url, url, ContentSettingsType::INTENT_PICKER_DISPLAY,
      std::move(pref_dict));
}
