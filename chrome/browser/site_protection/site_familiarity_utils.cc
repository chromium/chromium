// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/site_protection/site_familiarity_utils.h"

#include "chrome/browser/content_settings/generated_javascript_optimizer_pref.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/features.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"

namespace site_protection {
namespace {
HostContentSettingsMap* GetHostContentSettingsMap(
    content::WebContents* web_contents) {
  if (!web_contents) {
    return nullptr;
  }
  content::BrowserContext* browser_context = web_contents->GetBrowserContext();
  Profile* profile = Profile::FromBrowserContext(browser_context);

  return HostContentSettingsMapFactory::GetForProfile(profile);
}
}  // namespace

bool AreV8OptimizationsDisabledOnUnfamiliarSites(Profile* profile) {
  return ComputeDefaultJavascriptOptimizerSetting(profile) ==
         content_settings::JavascriptOptimizerSetting::
             kBlockedForUnfamiliarSites;
}

content_settings::JavascriptOptimizerSetting
ComputeDefaultJavascriptOptimizerSetting(Profile* profile) {
  HostContentSettingsMap* host_content_settings_map =
      HostContentSettingsMapFactory::GetForProfile(profile);
  if (!host_content_settings_map) {
    // If there is no HostContentSettingsMap, assume that this is being called
    // during the startup sequence for the profile picker. Return that
    // JavaScript optimizers are allowed.
    return content_settings::JavascriptOptimizerSetting::kAllowed;
  }
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

std::optional<bool> AreV8OptimizationsDisabled(
    content::WebContents* web_contents) {
  if (!web_contents) {
    return std::nullopt;
  }

  content::RenderFrameHost* render_frame_host =
      web_contents->GetPrimaryMainFrame();
  if (!render_frame_host) {
    return std::nullopt;
  }

  content::RenderProcessHost* render_process_host =
      render_frame_host->GetProcess();
  if (!render_process_host) {
    return std::nullopt;
  }

  return render_process_host->AreV8OptimizationsDisabled();
}

std::optional<content_settings::SettingSource>
GetJavascriptOptimizerSettingSource(content::WebContents* web_contents) {
  if (!web_contents) {
    return std::nullopt;
  }

  HostContentSettingsMap* map = GetHostContentSettingsMap(web_contents);
  if (!map) {
    return std::nullopt;
  }

  const GURL& site_url = web_contents->GetURL();
  if (site_url.is_empty()) {
    return std::nullopt;
  }

  content_settings::SettingInfo setting_info;
  map->GetContentSetting(site_url, site_url,
                         ContentSettingsType::JAVASCRIPT_OPTIMIZER,
                         &setting_info);
  return setting_info.source;
}

void EnableV8Optimizations(content::WebContents* web_contents) {
  if (!web_contents) {
    return;
  }

  HostContentSettingsMap* map = GetHostContentSettingsMap(web_contents);
  if (!map) {
    return;
  }

  const GURL& site_url = web_contents->GetURL();
  if (site_url.is_empty()) {
    return;
  }

  map->SetContentSettingDefaultScope(site_url, site_url,
                                     ContentSettingsType::JAVASCRIPT_OPTIMIZER,
                                     ContentSetting::CONTENT_SETTING_ALLOW);
}

}  // namespace site_protection
