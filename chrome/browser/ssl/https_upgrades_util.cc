// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ssl/https_upgrades_util.h"

#include "base/feature_list.h"
#include "base/values.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/advanced_protection_status_manager.h"
#include "chrome/browser/safe_browsing/advanced_protection_status_manager_factory.h"
#include "chrome/browser/ssl/chrome_security_blocking_page_factory.h"
#include "chrome/browser/ssl/https_first_mode_settings_tracker.h"
#include "chrome/browser/ssl/https_upgrades_interceptor.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
#include "components/prefs/pref_service.h"
#include "components/security_interstitials/content/stateful_ssl_host_state_delegate.h"
#include "components/security_interstitials/core/https_only_mode_metrics.h"
#include "content/public/browser/web_contents.h"
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

security_interstitials::https_only_mode::HttpInterstitialState
ComputeInterstitialState(content::WebContents* web_contents, const GURL& url) {
  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  PrefService* prefs = profile->GetPrefs();
  security_interstitials::https_only_mode::HttpInterstitialState
      interstitial_state;
  interstitial_state.enabled_by_pref =
      prefs && prefs->GetBoolean(prefs::kHttpsOnlyModeEnabled);

  if (base::FeatureList::IsEnabled(features::kHttpsFirstModeIncognito)) {
    if (profile->IsIncognitoProfile() && prefs &&
        prefs->GetBoolean(prefs::kHttpsFirstModeIncognito)) {
      interstitial_state.enabled_by_incognito = true;
    }
  }

  StatefulSSLHostStateDelegate* state =
      static_cast<StatefulSSLHostStateDelegate*>(
          profile->GetSSLHostStateDelegate());

  if (IsBalancedModeEnabled(prefs) && state &&
      !state->HttpsFirstBalancedModeSuppressedForTesting()) {
    interstitial_state.enabled_in_balanced_mode = true;
  }

  auto* storage_partition =
      web_contents->GetPrimaryMainFrame()->GetStoragePartition();

  HttpsFirstModeService* hfm_service =
      HttpsFirstModeServiceFactory::GetForProfile(profile);
  if (hfm_service) {
    interstitial_state.enabled_by_typically_secure_browsing =
        hfm_service->IsInterstitialEnabledByTypicallySecureUserHeuristic();
  }

  // StatefulSSLHostStateDelegate can be null during tests.
  if (state && state->IsHttpsEnforcedForUrl(url, storage_partition) &&
      !MustDisableSiteEngagementHeuristic(profile)) {
    interstitial_state.enabled_by_engagement_heuristic = true;
  }

  auto* advanced_protection_manager =
      safe_browsing::AdvancedProtectionStatusManagerFactory::GetForProfile(
          profile);
  if (advanced_protection_manager) {
    interstitial_state.enabled_by_advanced_protection =
        advanced_protection_manager->IsUnderAdvancedProtection();
  }

  return interstitial_state;
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

bool IsBalancedModeInterstitialEnabledByHeuristics(
    const HttpInterstitialState& state) {
  return IsBalancedModeAvailable() &&
         (state.enabled_by_engagement_heuristic ||
          state.enabled_by_typically_secure_browsing);
}

bool IsBalancedModeUniquelyEnabled(const HttpInterstitialState& state) {
  // Balance mode is _uniquely_ enabled only when other HFM variants aren't
  // enabled.
  if (state.enabled_by_pref) {
    return false;
  }
  if (base::FeatureList::IsEnabled(features::kHttpsFirstModeIncognito) &&
      state.enabled_by_incognito) {
    return false;
  }

  // ...then ensure balanced mode is enabled.
  return (IsBalancedModeAvailable() && state.enabled_in_balanced_mode) ||
         IsBalancedModeInterstitialEnabledByHeuristics(state);
}

bool IsInterstitialEnabled(const HttpInterstitialState& state) {
  // Interstitials are enabled when "strict" interstitials are enabled...
  if (IsStrictInterstitialEnabled(state)) {
    return true;
  }
  if (IsBalancedModeAvailable() && state.enabled_in_balanced_mode) {
    return true;
  }
  return IsBalancedModeInterstitialEnabledByHeuristics(state);
}

bool IsStrictInterstitialEnabled(const HttpInterstitialState& state) {
  if (state.enabled_by_pref) {
    return true;
  }
  if (base::FeatureList::IsEnabled(features::kHttpsFirstModeIncognito) &&
      state.enabled_by_incognito) {
    return true;
  }
  if (state.enabled_by_advanced_protection) {
    return true;
  }
  return false;
}

bool ShouldExemptNonUniqueHostnames(const HttpInterstitialState& state) {
  // If strict mode is enabled by the pref, warn the user before any HTTP that
  // goes over the network. Any other mode ignores non-unique hostnames.
  return !state.enabled_by_pref;
}

bool ShouldExcludeUrlFromInterstitial(const HttpInterstitialState& state,
                                      const GURL& url) {
  // In balanced mode, single-label hostnames and URLs with non-default ports
  // are excluded from interstitials. This also applies if one of the HFM
  // heuristics enabled Balanced Mode.
  return IsBalancedModeUniquelyEnabled(state) &&
         (net::GetSuperdomain(url.GetHost()).empty() ||
          (url.has_port() &&
           url.IntPort() != HttpsUpgradesInterceptor::GetHttpPortForTesting()));
}

bool MustDisableSiteEngagementHeuristic(Profile* profile) {
  return !base::FeatureList::IsEnabled(
             features::kHttpsFirstModeV2ForEngagedSites) ||
         !IsBalancedModeAvailable() ||
         ChromeSecurityBlockingPageFactory::IsEnterpriseManaged(profile);
}

bool MustDisableTypicallySecureUserHeuristic(Profile* profile) {
  return !base::FeatureList::IsEnabled(
             features::kHttpsFirstModeV2ForTypicallySecureUsers) ||
         !IsBalancedModeAvailable() ||
         ChromeSecurityBlockingPageFactory::IsEnterpriseManaged(profile);
}

void RecordHttpsFirstModeUKM(
    ukm::SourceId source_id,
    security_interstitials::https_only_mode::BlockingResult result) {
  if (source_id == ukm::kInvalidSourceId) {
    return;
  }

  ukm::UkmRecorder* ukm_recorder = ukm::UkmRecorder::Get();
  CHECK(ukm_recorder);
  ukm::builders::HttpsFirstMode_Event(source_id)
      .SetResult(static_cast<int>(result))
      .Record(ukm_recorder);
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
