// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/content_settings/generated_javascript_optimizer_pref.h"

#include <optional>

#include "base/types/cxx23_to_underlying.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/site_protection/site_familiarity_utils.h"
#include "chrome/common/extensions/api/settings_private.h"
#include "components/content_settings/core/common/features.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"

using extensions::api::settings_private::Enforcement;
using extensions::api::settings_private::PrefObject;
using extensions::settings_private::SetPrefResult;

namespace content_settings {

const char kGeneratedJavascriptOptimizerPref[] =
    "generated.javascript_optimizer";

GeneratedJavascriptOptimizerPref::GeneratedJavascriptOptimizerPref(
    Profile* profile)
    : profile_(profile) {
  user_prefs_registrar_.Init(profile->GetPrefs());
  user_prefs_registrar_.AddMultiple(
      {prefs::kJavascriptOptimizerBlockedForUnfamiliarSites,
       prefs::kSafeBrowsingEnabled},
      base::BindRepeating(
          &GeneratedJavascriptOptimizerPref::OnPreferencesChanged,
          base::Unretained(this)));

  host_content_settings_map_ =
      HostContentSettingsMapFactory::GetForProfile(profile_);
  content_setting_observation_.Observe(host_content_settings_map_.get());
}

GeneratedJavascriptOptimizerPref::~GeneratedJavascriptOptimizerPref() = default;

void GeneratedJavascriptOptimizerPref::OnContentSettingChanged(
    const ContentSettingsPattern& primary_pattern,
    const ContentSettingsPattern& secondary_pattern,
    ContentSettingsTypeSet content_type_set) {
  if (content_type_set.Contains(ContentSettingsType::JAVASCRIPT_OPTIMIZER)) {
    OnPreferencesChanged();
  }
}

SetPrefResult GeneratedJavascriptOptimizerPref::SetPref(
    const base::Value* value) {
  if (!value->is_int()) {
    return SetPrefResult::PREF_TYPE_MISMATCH;
  }

  auto selection = static_cast<int>(value->GetInt());
  if (selection != static_cast<int>(JavascriptOptimizerSetting::kBlocked) &&
      selection != static_cast<int>(JavascriptOptimizerSetting::kAllowed) &&
      selection !=
          static_cast<int>(
              JavascriptOptimizerSetting::kBlockedForUnfamiliarSites)) {
    return SetPrefResult::PREF_TYPE_MISMATCH;
  }

  host_content_settings_map_->SetDefaultContentSetting(
      ContentSettingsType::JAVASCRIPT_OPTIMIZER,
      selection == static_cast<int>(JavascriptOptimizerSetting::kBlocked)
          ? ContentSetting::CONTENT_SETTING_BLOCK
          : ContentSetting::CONTENT_SETTING_ALLOW);
  profile_->GetPrefs()->SetBoolean(
      prefs::kJavascriptOptimizerBlockedForUnfamiliarSites,
      selection == static_cast<int>(
                       JavascriptOptimizerSetting::kBlockedForUnfamiliarSites));

  return SetPrefResult::SUCCESS;
}

PrefObject GeneratedJavascriptOptimizerPref::GetPrefObject() const {
  PrefObject pref_object;
  pref_object.key = kGeneratedJavascriptOptimizerPref;
  pref_object.type = extensions::api::settings_private::PrefType::kNumber;
  pref_object.value = base::Value(static_cast<int>(
      site_protection::ComputeDefaultJavascriptOptimizerSetting(profile_)));

  content_settings::ProviderType content_setting_provider;
  host_content_settings_map_->GetDefaultContentSetting(
      ContentSettingsType::JAVASCRIPT_OPTIMIZER, &content_setting_provider);
  auto content_setting_source =
      content_settings::GetSettingSourceFromProviderType(
          content_setting_provider);
  if (content_setting_source != content_settings::SettingSource::kUser) {
    pref_object.enforcement = Enforcement::kEnforced;
    GeneratedPref::ApplyControlledByFromContentSettingSource(
        &pref_object, SettingSource::kPolicy);
  }

  if (!safe_browsing::IsSafeBrowsingEnabled(*profile_->GetPrefs())) {
    pref_object.enforcement =
        extensions::api::settings_private::Enforcement::kEnforced;
    pref_object.controlled_by =
        extensions::api::settings_private::ControlledBy::kSafeBrowsingOff;
    base::Value::List user_selectable_values;
    user_selectable_values.Append(
        base::Value(static_cast<int>(JavascriptOptimizerSetting::kAllowed)));
    user_selectable_values.Append(
        base::Value(static_cast<int>(JavascriptOptimizerSetting::kBlocked)));
    pref_object.user_selectable_values = std::move(user_selectable_values);
  }

  return pref_object;
}

void GeneratedJavascriptOptimizerPref::OnPreferencesChanged() {
  NotifyObservers(kGeneratedJavascriptOptimizerPref);
}

}  // namespace content_settings
