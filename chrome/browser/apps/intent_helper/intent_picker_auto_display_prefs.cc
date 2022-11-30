// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/intent_helper/intent_picker_auto_display_prefs.h"

#include "base/values.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/content_settings/core/common/content_settings_utils.h"

namespace {

// Stop auto-displaying the picker UI if the user has dismissed it
// |kDismissThreshold|+ times.
constexpr int kDismissThreshold = 2;

// Show the Intent Chip as collapsed after it has been shown expanded
// |kIntentChipCollapseThreshold| times.
constexpr int kIntentChipCollapseThreshold = 3;

constexpr char kAutoDisplayKey[] = "picker_auto_display_key";
constexpr char kPlatformKey[] = "picker_platform_key";
constexpr char kIntentChipCountKey[] = "intent_chip_display_count";

// Retrieves or creates a new dictionary for the specific |url|.
base::Value GetAutoDisplayDictForSettings(
    const HostContentSettingsMap* settings,
    const GURL& url) {
  if (!settings)
    return base::Value(base::Value::Type::DICTIONARY);

  base::Value value = settings->GetWebsiteSetting(
      url, url, ContentSettingsType::INTENT_PICKER_DISPLAY, /*info=*/nullptr);

  return value.type() == base::Value::Type::DICTIONARY
             ? std::move(value)
             : base::Value(base::Value::Type::DICTIONARY);
}

}  // namespace

// static
bool IntentPickerAutoDisplayPrefs::ShouldAutoDisplayUi(Profile* profile,
                                                       const GURL& url) {
  base::Value pref_dict = GetAutoDisplayDictForSettings(
      HostContentSettingsMapFactory::GetForProfile(profile), url);

  return pref_dict.FindIntKey(kAutoDisplayKey).value_or(0) < kDismissThreshold;
}

// static
void IntentPickerAutoDisplayPrefs::IncrementPickerUICounter(Profile* profile,
                                                            const GURL& url) {
  auto* settings_map = HostContentSettingsMapFactory::GetForProfile(profile);
  base::Value pref_dict = GetAutoDisplayDictForSettings(settings_map, url);

  int dismissed_count = pref_dict.FindIntKey(kAutoDisplayKey).value_or(0);
  pref_dict.SetIntKey(kAutoDisplayKey, dismissed_count + 1);

  settings_map->SetWebsiteSettingDefaultScope(
      url, url, ContentSettingsType::INTENT_PICKER_DISPLAY,
      std::move(pref_dict));
}

// static
IntentPickerAutoDisplayPrefs::ChipState
IntentPickerAutoDisplayPrefs::GetChipStateAndIncrementCounter(Profile* profile,
                                                              const GURL& url) {
  auto* settings_map = HostContentSettingsMapFactory::GetForProfile(profile);
  base::Value pref_dict = GetAutoDisplayDictForSettings(settings_map, url);

  int display_count = pref_dict.FindIntKey(kIntentChipCountKey).value_or(0);
  if (display_count >= kIntentChipCollapseThreshold) {
    // Exit before updating the counter so we don't keep counting indefinitely.
    return ChipState::kCollapsed;
  }

  pref_dict.SetIntKey(kIntentChipCountKey, ++display_count);
  settings_map->SetWebsiteSettingDefaultScope(
      url, url, ContentSettingsType::INTENT_PICKER_DISPLAY,
      std::move(pref_dict));

  return ChipState::kExpanded;
}

// static
void IntentPickerAutoDisplayPrefs::ResetIntentChipCounter(Profile* profile,
                                                          const GURL& url) {
  auto* settings_map = HostContentSettingsMapFactory::GetForProfile(profile);
  base::Value pref_dict = GetAutoDisplayDictForSettings(settings_map, url);

  pref_dict.SetIntKey(kIntentChipCountKey, 0);

  settings_map->SetWebsiteSettingDefaultScope(
      url, url, ContentSettingsType::INTENT_PICKER_DISPLAY,
      std::move(pref_dict));
}

// static
IntentPickerAutoDisplayPrefs::Platform
IntentPickerAutoDisplayPrefs::GetLastUsedPlatformForTablets(Profile* profile,
                                                            const GURL& url) {
  base::Value pref_dict = GetAutoDisplayDictForSettings(
      HostContentSettingsMapFactory::GetForProfile(profile), url);
  int platform = pref_dict.FindIntKey(kPlatformKey).value_or(0);

  DCHECK_GE(platform, static_cast<int>(Platform::kNone));
  DCHECK_LE(platform, static_cast<int>(Platform::kMaxValue));

  return static_cast<Platform>(platform);
}

// static
void IntentPickerAutoDisplayPrefs::UpdatePlatformForTablets(Profile* profile,
                                                            const GURL& url,
                                                            Platform platform) {
  auto* settings_map = HostContentSettingsMapFactory::GetForProfile(profile);
  base::Value pref_dict = GetAutoDisplayDictForSettings(settings_map, url);

  DCHECK_GE(static_cast<int>(platform), static_cast<int>(Platform::kNone));
  DCHECK_LE(static_cast<int>(platform), static_cast<int>(Platform::kMaxValue));
  pref_dict.SetIntKey(kPlatformKey, static_cast<int>(platform));

  settings_map->SetWebsiteSettingDefaultScope(
      url, url, ContentSettingsType::INTENT_PICKER_DISPLAY,
      std::move(pref_dict));
}
