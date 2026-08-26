// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/selection/inline_cue_blocklist_utils.h"

#include <string>
#include <vector>

#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/glic/public/features.h"
#include "chrome/browser/profiles/profile.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "url/gurl.h"

namespace glic {

bool IsSiteInDefaultBlocklistForInlineCue(const std::string& pattern_str) {
  return features::GetGlicSelectionDefaultBlockedSites().contains(pattern_str);
}

bool IsSiteBlockedForInlineCue(Profile* profile, const GURL& url) {
  if (!url.is_valid() || !profile) {
    return false;
  }

  HostContentSettingsMap* settings_map =
      HostContentSettingsMapFactory::GetForProfile(profile);
  content_settings::SettingInfo info;
  ContentSetting setting = settings_map->GetContentSetting(
      url, url, ContentSettingsType::INLINE_CUE_MENU, &info);

  if (!info.primary_pattern.MatchesAllHosts()) {
    return setting == CONTENT_SETTING_BLOCK;
  }

  // If matching the global default, fall back to the default blocklist.
  for (const std::string& site_str :
       features::GetGlicSelectionDefaultBlockedSites()) {
    ContentSettingsPattern pattern =
        ContentSettingsPattern::FromString(site_str);
    if (pattern.IsValid() && pattern.Matches(url)) {
      return true;
    }
  }

  return false;
}

std::vector<std::string> GetActiveDefaultBlockedSitePatternsForInlineCue(
    Profile* profile) {
  if (!profile) {
    return {};
  }

  HostContentSettingsMap* settings_map =
      HostContentSettingsMapFactory::GetForProfile(profile);

  std::vector<std::string> active_blocked_sites;
  for (const std::string& site_str :
       features::GetGlicSelectionDefaultBlockedSites()) {
    ContentSettingsPattern pattern =
        ContentSettingsPattern::FromString(site_str);
    if (!pattern.IsValid()) {
      continue;
    }

    // Only return patterns that have not been explicitly modified by the user.
    content_settings::SettingInfo info;
    GURL site_url(site_str);
    settings_map->GetContentSetting(
        site_url, site_url, ContentSettingsType::INLINE_CUE_MENU, &info);
    if (info.primary_pattern.MatchesAllHosts()) {
      active_blocked_sites.push_back(site_str);
    }
  }
  return active_blocked_sites;
}

bool UnblockDefaultSiteForInlineCue(Profile* profile,
                                    const std::string& pattern_str) {
  if (!profile || !IsSiteInDefaultBlocklistForInlineCue(pattern_str)) {
    return false;
  }

  ContentSettingsPattern pattern =
      ContentSettingsPattern::FromString(pattern_str);
  if (!pattern.IsValid()) {
    return false;
  }

  HostContentSettingsMapFactory::GetForProfile(profile)
      ->SetContentSettingCustomScope(
          pattern, ContentSettingsPattern::Wildcard(),
          ContentSettingsType::INLINE_CUE_MENU, CONTENT_SETTING_ALLOW);
  return true;
}

}  // namespace glic
