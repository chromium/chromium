// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ssl/https_upgrades_util.h"

#include "base/feature_list.h"
#include "base/values.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
#include "components/prefs/pref_service.h"
#include "url/gurl.h"

bool IsHostnameInHttpAllowlist(const GURL& url, PrefService* prefs) {
  const base::Value::List& allowed_hosts =
      prefs->GetList(prefs::kHttpAllowlist);

  // Though this is not technically a Content Setting, ContentSettingsPattern
  // aligns better than URLMatcher with the rules from
  // https://chromeenterprise.google/policies/url-patterns/.
  for (const auto& value : allowed_hosts) {
    if (!value.is_string()) {
      continue;
    }
    auto pattern = ContentSettingsPattern::FromString(value.GetString());
    // Blanket host wildcard patterns are not allowed (matching every host),
    // because admins should instead explicitly disable upgrades using the
    // HttpsOnlyMode policy.
    if (pattern.IsValid() && !pattern.MatchesAllHosts() &&
        pattern.Matches(url)) {
      return true;
    }
  }
  return false;
}

void AllowHttpForHostnamesForTesting(const std::vector<std::string>& hostnames,
                                     PrefService* prefs) {
  DCHECK(prefs->GetList(prefs::kHttpAllowlist).empty());

  base::Value::List allowed_hosts;
  for (const std::string& hostname : hostnames) {
    allowed_hosts.Append(hostname);
  }
  prefs->SetList(prefs::kHttpAllowlist, std::move(allowed_hosts));
}

void ClearHttpAllowlistForHostnamesForTesting(PrefService* prefs) {
  base::Value::List empty_list;
  prefs->SetList(prefs::kHttpAllowlist, std::move(empty_list));
}

bool IsInterstitialEnabled(
    const security_interstitials::https_only_mode::HttpInterstitialState&
        state) {
  if (state.enabled_by_pref) {
    return true;
  }
  return state.enabled_by_engagement_heuristic &&
         base::FeatureList::IsEnabled(
             features::kHttpsFirstModeV2ForEngagedSites);
}

ScopedAllowHttpForHostnamesForTesting::ScopedAllowHttpForHostnamesForTesting(
    const std::vector<std::string>& hostnames,
    PrefService* prefs)
    : prefs_(prefs) {
  AllowHttpForHostnamesForTesting(hostnames, prefs);
}

ScopedAllowHttpForHostnamesForTesting::
    ~ScopedAllowHttpForHostnamesForTesting() {
  ClearHttpAllowlistForHostnamesForTesting(prefs_);
}
