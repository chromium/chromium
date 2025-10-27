// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/site_protection/site_familiarity_utils.h"

#include "chrome/browser/content_settings/generated_javascript_optimizer_pref.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/features.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "content/public/common/content_features.h"

namespace site_protection {

content_settings::JavascriptOptimizerSetting
ComputeDefaultJavascriptOptimizerSetting(Profile* profile) {
  HostContentSettingsMap* host_content_settings_map =
      HostContentSettingsMapFactory::GetForProfile(profile);
  content_settings::ProviderType content_setting_provider;
  const auto default_content_setting =
      host_content_settings_map->GetDefaultContentSetting(
          ContentSettingsType::JAVASCRIPT_OPTIMIZER, &content_setting_provider);
  if (default_content_setting == ContentSetting::CONTENT_SETTING_BLOCK) {
    return content_settings::JavascriptOptimizerSetting::kBlocked;
  }

  auto content_setting_source =
      content_settings::GetSettingSourceFromProviderType(
          content_setting_provider);
  if (content_setting_source != content_settings::SettingSource::kUser) {
    // Respect content setting provided by enterprise policy. Currently the
    // JavascriptOptimizerSetting::kBlockedForUnfamiliarSites value cannot be
    // set via enterprise policy.
    return content_settings::JavascriptOptimizerSetting::kAllowed;
  }

  if (!base::FeatureList::IsEnabled(
          features::kProcessSelectionDeferringConditions) ||
      !base::FeatureList::IsEnabled(
          content_settings::features::
              kBlockV8OptimizerOnUnfamiliarSitesSetting)) {
    // The "Setting the v8-optimizer enabled state based on site-familiarity"
    // feature is disabled.
    return content_settings::JavascriptOptimizerSetting::kAllowed;
  }

  if (!safe_browsing::IsSafeBrowsingEnabled(*profile->GetPrefs())) {
    return content_settings::JavascriptOptimizerSetting::kAllowed;
  }

  return profile->GetPrefs()->GetBoolean(
             prefs::kJavascriptOptimizerBlockedForUnfamiliarSites)
             ? content_settings::JavascriptOptimizerSetting::
                   kBlockedForUnfamiliarSites
             : content_settings::JavascriptOptimizerSetting::kAllowed;
}

}  // namespace site_protection
