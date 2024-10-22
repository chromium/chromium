// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ssl/https_upgrades_util.h"

#include "base/feature_list.h"
#include "base/values.h"
#include "chrome/browser/ssl/https_upgrades_interceptor.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
#include "components/prefs/pref_service.h"
#include "components/security_interstitials/core/https_only_mode_metrics.h"
#include "net/base/url_util.h"
#include "url/gurl.h"

using HttpInterstitialState =
    security_interstitials::https_only_mode::HttpInterstitialState;

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

bool IsBalancedModeAvailable() {
  return base::FeatureList::IsEnabled(features::kHttpsFirstBalancedMode);
}

bool IsBalancedModeEnabled(PrefService* prefs) {
  if (!prefs || !IsBalancedModeAvailable()) {
    return false;
  }
  bool user_has_modified_settings =
      prefs->HasPrefPath(prefs::kHttpsOnlyModeEnabled) ||
      prefs->HasPrefPath(prefs::kHttpsFirstBalancedMode);
  if (!user_has_modified_settings) {
    return base::FeatureList::IsEnabled(
        features::kHttpsFirstBalancedModeAutoEnable);
  }
  return prefs->GetBoolean(prefs::kHttpsFirstBalancedMode);
}

bool IsBalancedModeInterstitialEnabledBySiteEngagementHeuristic(
    const HttpInterstitialState& state) {
  if (!base::FeatureList::IsEnabled(
          features::kHttpsFirstModeV2ForEngagedSites) ||
      !IsBalancedModeAvailable()) {
    return false;
  }
  return state.enabled_by_engagement_heuristic;
}

bool IsBalancedModeUniquelyEnabled(const HttpInterstitialState& state) {
  // Balance mode is _uniquely_ enabled only when other HFM variants aren't
  // enabled (except for Site Engagement heuristic which enables Balanced Mode
  // when it's available).
  if (state.enabled_by_pref) {
    return false;
  }
  if (IsBalancedModeInterstitialEnabledBySiteEngagementHeuristic(state)) {
    return false;
  }
  if (base::FeatureList::IsEnabled(
          features::kHttpsFirstModeV2ForTypicallySecureUsers) &&
      state.enabled_by_typically_secure_browsing) {
    return false;
  }
  if (base::FeatureList::IsEnabled(features::kHttpsFirstModeIncognito) &&
      state.enabled_by_incognito) {
    return false;
  }

  // ...then ensure balanced mode is enabled.
  return IsBalancedModeAvailable() && state.enabled_in_balanced_mode;
}

bool IsNewHttpsFirstModeInterstitialEnabled() {
  return base::FeatureList::IsEnabled(
      features::kHttpsFirstModeInterstitialAugust2024Refresh);
}

bool IsInterstitialEnabled(const HttpInterstitialState& state) {
  // Interstitials are enabled when "strict" interstitials are enabled...
  if (IsStrictInterstitialEnabled(state)) {
    return true;
  }
  if (IsBalancedModeAvailable() && state.enabled_in_balanced_mode) {
    return true;
  }
  return IsBalancedModeInterstitialEnabledBySiteEngagementHeuristic(state);
}

bool IsStrictInterstitialEnabled(const HttpInterstitialState& state) {
  if (state.enabled_by_pref) {
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

bool ShouldExemptNonUniqueHostnames(const HttpInterstitialState& state) {
  // Full HTTPS-First Mode, HFM-for-engaged-sites, and
  // HFM-for-Typically-Secure-Users apply strict HTTPS enforcement, and warn
  // the user before any HTTP that goes over the network.
  if (state.enabled_by_pref) {
    return false;
  }

  if (IsBalancedModeInterstitialEnabledBySiteEngagementHeuristic(state)) {
    return true;
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
  // Balanced mode HFM exempts non-unique hostnames to reduce warning volume.
  if (IsBalancedModeAvailable() && state.enabled_in_balanced_mode) {
    return true;
  }
  // If no interstitial state is set, then the default is HTTPS-Upgrades which
  // does exempt non-unique hostnames.
  return true;
}

bool ShouldExcludeUrlFromInterstitial(const HttpInterstitialState& state,
                                      const GURL& url) {
  // In balanced mode, single-label hostnames and URLs with non-default ports
  // are excluded from interstitials.
  return IsBalancedModeUniquelyEnabled(state) &&
         (net::GetSuperdomain(url.host()).empty() ||
          (url.has_port() &&
           url.IntPort() != HttpsUpgradesInterceptor::GetHttpPortForTesting()));
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
