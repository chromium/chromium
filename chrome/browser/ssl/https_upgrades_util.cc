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
#include "components/security_interstitials/core/https_only_mode_metrics.h"
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
  if (base::FeatureList::IsEnabled(
          features::kHttpsFirstModeV2ForEngagedSites) &&
      state.enabled_by_engagement_heuristic) {
    return true;
  }
  if (base::FeatureList::IsEnabled(features::kHttpsFirstModeIncognito) &&
      state.enabled_by_incognito) {
    return true;
  }
  return base::FeatureList::IsEnabled(
             features::kHttpsFirstModeV2ForTypicallySecureUsers) &&
         state.enabled_by_typically_secure_browsing;
}

bool ShouldExemptNonUniqueHostnames(
    const security_interstitials::https_only_mode::HttpInterstitialState&
        state) {
  // Full HTTPS-First Mode, HFM-for-engaged-sites, and
  // HFM-for-Typically-Secure-Users apply strict HTTPS enforcement, and warn
  // the user before any HTTP that goes over the network.
  if (state.enabled_by_pref) {
    return false;
  }
  if (base::FeatureList::IsEnabled(
          features::kHttpsFirstModeV2ForEngagedSites) &&
      state.enabled_by_engagement_heuristic) {
    return false;
  }
  if (base::FeatureList::IsEnabled(
          features::kHttpsFirstModeV2ForTypicallySecureUsers) &&
      state.enabled_by_typically_secure_browsing) {
    return false;
  }
  // HFM-in-Incognito is default-enabled and has looser exemptions to reduce
  // the amount of warnings shown.
  if (base::FeatureList::IsEnabled(features::kHttpsFirstModeIncognito) &&
      state.enabled_by_incognito) {
    return true;
  }
  // If no interstitial state is set, then the default is HTTPS-Upgrades which
  // does exempt non-unique hostnames.
  return true;
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
