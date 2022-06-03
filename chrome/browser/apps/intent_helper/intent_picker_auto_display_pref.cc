// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/intent_helper/intent_picker_auto_display_pref.h"

#include <utility>

#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_types.h"

namespace {

// Stop auto-displaying the picker UI if the user has dismissed it
// |kDismissThreshold|+ times.
constexpr int kDismissThreshold = 2;

constexpr char kAutoDisplayKey[] = "picker_auto_display_key";
constexpr char kPlatformKey[] = "picker_platform_key";

// Retrieves or creates a new dictionary for the specific |origin|.
std::unique_ptr<base::DictionaryValue> GetAutoDisplayDictForSettings(
    const HostContentSettingsMap* settings,
    const GURL& origin) {
  if (!settings)
    return std::make_unique<base::DictionaryValue>();

  std::unique_ptr<base::DictionaryValue> value =
      base::DictionaryValue::From(settings->GetWebsiteSetting(
          origin, origin, ContentSettingsType::INTENT_PICKER_DISPLAY, nullptr));

  if (value.get())
    return value;
  return std::make_unique<base::DictionaryValue>();
}

}  // namespace

IntentPickerAutoDisplayPref::IntentPickerAutoDisplayPref(
    const GURL& origin,
    HostContentSettingsMap* settings)
    : IntentPickerAutoDisplayPref(
          origin,
          GetAutoDisplayDictForSettings(settings, origin)) {
  settings_map_ = settings;
}

IntentPickerAutoDisplayPref::~IntentPickerAutoDisplayPref() = default;

void IntentPickerAutoDisplayPref::IncrementCounter() {
  if (ui_dismissed_counter_ >= kDismissThreshold)
    return;

  SetDismissedCounter(ui_dismissed_counter_ + 1);
  Commit();
}

bool IntentPickerAutoDisplayPref::HasExceededThreshold() const {
  return ui_dismissed_counter_ < kDismissThreshold;
}

IntentPickerAutoDisplayPref::Platform
IntentPickerAutoDisplayPref::GetPlatform() {
  return platform_;
}

void IntentPickerAutoDisplayPref::UpdatePlatform(Platform platform) {
  if (!pref_dict_)
    return;

  DCHECK_GE(static_cast<int>(platform), static_cast<int>(Platform::kNone));
  DCHECK_LE(static_cast<int>(platform), static_cast<int>(Platform::kMaxValue));
  platform_ = platform;
  pref_dict_->SetInteger(kPlatformKey, static_cast<int>(platform_));
  Commit();
}

IntentPickerAutoDisplayPref::IntentPickerAutoDisplayPref(
    const GURL& origin,
    std::unique_ptr<base::DictionaryValue> pref_dict)
    : origin_(origin), pref_dict_(pref_dict.release()) {
  ui_dismissed_counter_ = QueryDismissedCounter();
  platform_ = QueryPlatform();
}

int IntentPickerAutoDisplayPref::QueryDismissedCounter() {
  if (!pref_dict_)
    return 0;

  int counter = 0;
  pref_dict_->GetInteger(kAutoDisplayKey, &counter);
  DCHECK_GE(counter, static_cast<int>(Platform::kNone));
  DCHECK_LE(counter, static_cast<int>(Platform::kMaxValue));

  return counter;
}

// Used to force both the local counter |ui_dismissed_counter_| and the
// respecive dictionary to be synced.
void IntentPickerAutoDisplayPref::SetDismissedCounter(int new_counter) {
  if (!pref_dict_)
    return;

  ui_dismissed_counter_ = new_counter;
  pref_dict_->SetInteger(kAutoDisplayKey, ui_dismissed_counter_);
}

IntentPickerAutoDisplayPref::Platform
IntentPickerAutoDisplayPref::QueryPlatform() {
  if (!pref_dict_)
    return Platform::kNone;

  int platform = 0;
  pref_dict_->GetInteger(kPlatformKey, &platform);
  DCHECK_GE(platform, static_cast<int>(Platform::kNone));
  DCHECK_LE(platform, static_cast<int>(Platform::kMaxValue));

  return static_cast<Platform>(platform);
}

void IntentPickerAutoDisplayPref::Commit() {
  settings_map_->SetWebsiteSettingDefaultScope(
      origin_, origin_, ContentSettingsType::INTENT_PICKER_DISPLAY,
      std::move(pref_dict_));
}
