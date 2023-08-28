// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ssl/https_first_mode_settings_tracker.h"

#include "base/feature_list.h"
#include "base/json/values_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/time/default_clock.h"
#include "base/values.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/metrics/chrome_metrics_service_accessor.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/advanced_protection_status_manager_factory.h"
#include "chrome/browser/ssl/https_upgrades_interceptor.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/security_interstitials/content/https_only_mode_blocking_page.h"
#include "components/security_interstitials/content/stateful_ssl_host_state_delegate.h"
#include "components/site_engagement/content/site_engagement_service.h"
#include "components/variations/synthetic_trials.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/profiles/profile_helper.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

// Minimum score of an HTTPS origin to enable HFM on its hostname.
const base::FeatureParam<int> kHttpsAddThreshold{
    &features::kHttpsFirstModeV2ForEngagedSites, "https-add-threshold", 40};

// Maximum score of an HTTP origin to enable HFM on its hostname.
const base::FeatureParam<int> kHttpsRemoveThreshold{
    &features::kHttpsFirstModeV2ForEngagedSites, "https-remove-threshold", 30};

// If HTTPS score goes below kHttpsRemoveThreshold or HTTP score goes above
// kHttpRemoveThreshold, disable HFM on this hostname.
const base::FeatureParam<int> kHttpAddThreshold{
    &features::kHttpsFirstModeV2ForEngagedSites, "http-add-threshold", 5};
const base::FeatureParam<int> kHttpRemoveThreshold{
    &features::kHttpsFirstModeV2ForEngagedSites, "http-remove-threshold", 10};

// Parameters for Typically Secure User heuristic:

// The rolling window size during which we check for HTTPS-Upgrades fallback
// entries. If the number of fallback entries is smaller than
// kMaxRecentFallbackEntryCount, we may automatically enable HTTPS-First Mode.
const base::TimeDelta kFallbackEntriesRollingWindowSize = base::Days(7);

// Maximum number of would-be warnings to auto-enable HFM, including this
// warning.
const size_t kMaxRecentFallbackEntryCount = 2;

// Minimum age of the current browser profile to automatically enable HFM. This
// prevents auto-enabling HFM immediately upon first launch.
const base::TimeDelta kMinProfileAge = base::Days(7);

// Minimum total score for a user to be considered typically secure. If the user
// doesn't have at least this much engagement score over all sites, they might
// not have used Chrome sufficiently for us to auto-enable HFM.
const base::FeatureParam<int> kMinTotalEngagementPointsForTypicallySecureUser{
    &features::kHttpsFirstModeV2ForTypicallySecureUsers,
    "min-total-site-engagement-score", 25};

namespace {

using security_interstitials::https_only_mode::SiteEngagementHeuristicState;

const char kHttpsFirstModeServiceName[] = "HttpsFirstModeService";
const char kHttpsFirstModeSyntheticFieldTrialName[] =
    "HttpsFirstModeClientSetting";
const char kHttpsFirstModeSyntheticFieldTrialEnabledGroup[] = "Enabled";
const char kHttpsFirstModeSyntheticFieldTrialDisabledGroup[] = "Disabled";

// Returns the HTTP URL from `http_url` using the test port numbers, if any.
// TODO(crbug.com/1435222): Refactor and merge with UpgradeUrlToHttps().
GURL GetHttpUrlFromHttps(const GURL& https_url) {
  DCHECK(https_url.SchemeIsCryptographic());

  // Replace scheme with HTTP.
  GURL::Replacements upgrade_url;
  upgrade_url.SetSchemeStr(url::kHttpScheme);

  // For tests that use the EmbeddedTestServer, the server's port needs to be
  // specified as it can't use the default ports.
  int http_port_for_testing = HttpsUpgradesInterceptor::GetHttpPortForTesting();
  // `port_str` must be in scope for the call to ReplaceComponents() below.
  const std::string port_str = base::NumberToString(http_port_for_testing);
  if (http_port_for_testing) {
    // Only reached in testing, where the original URL will always have a
    // non-default port. One of the tests navigates to Google support pages, so
    // exclude that.
    // TODO(crbug.com/1435222): Remove this exception.
    if (https_url != GURL(security_interstitials::HttpsOnlyModeBlockingPage::
                              kLearnMoreLink)) {
      DCHECK(!https_url.port().empty());
      upgrade_url.SetPortStr(port_str);
    }
  }

  return https_url.ReplaceComponents(upgrade_url);
}

// Returns the HTTPS URL from `http_url` using the test port numbers, if any.
// TODO(crbug.com/1435222): Refactor and merge with UpgradeUrlToHttps().
GURL GetHttpsUrlFromHttp(const GURL& http_url) {
  DCHECK(!http_url.SchemeIsCryptographic());

  // Replace scheme with HTTPS.
  GURL::Replacements upgrade_url;
  upgrade_url.SetSchemeStr(url::kHttpsScheme);

  // For tests that use the EmbeddedTestServer, the server's port needs to be
  // specified as it can't use the default ports.
  int https_port_for_testing =
      HttpsUpgradesInterceptor::GetHttpsPortForTesting();
  // `port_str` must be in scope for the call to ReplaceComponents() below.
  const std::string port_str = base::NumberToString(https_port_for_testing);
  if (https_port_for_testing) {
    // Only reached in testing, where the original URL will always have a
    // non-default port.
    DCHECK(!http_url.port().empty());
    upgrade_url.SetPortStr(port_str);
  }

  return http_url.ReplaceComponents(upgrade_url);
}

std::unique_ptr<KeyedService> BuildService(content::BrowserContext* context) {
  Profile* profile = Profile::FromBrowserContext(context);
#if BUILDFLAG(IS_CHROMEOS_ASH)
  // Explicitly check for ChromeOS sign-in profiles (which would cause
  // double-counting of at-startup metrics for ChromeOS restarts) which are not
  // covered by the `IsRegularProfile()` check.
  if (ash::ProfileHelper::IsSigninProfile(profile)) {
    return nullptr;
  }
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
  return std::make_unique<HttpsFirstModeService>(
      profile, base::DefaultClock::GetInstance());
}

}  // namespace

HttpsFirstModeService::HttpsFirstModeService(Profile* profile,
                                             base::Clock* clock)
    : profile_(profile), clock_(clock) {
  pref_change_registrar_.Init(profile_->GetPrefs());
  // Using base::Unretained() here is safe as the PrefChangeRegistrar is owned
  // by `this`.
  pref_change_registrar_.Add(
      prefs::kHttpsOnlyModeEnabled,
      base::BindRepeating(&HttpsFirstModeService::OnHttpsFirstModePrefChanged,
                          base::Unretained(this)));

  // Track Advanced Protection status.
  if (base::FeatureList::IsEnabled(
          features::kHttpsFirstModeForAdvancedProtectionUsers)) {
    obs_.Observe(
        safe_browsing::AdvancedProtectionStatusManagerFactory::GetForProfile(
            profile_));
    // On startup, AdvancedProtectionStatusManager runs before this class so we
    // don't get called back. Run the callback to get the AP setting.
    OnAdvancedProtectionStatusChanged(
        safe_browsing::AdvancedProtectionStatusManagerFactory::GetForProfile(
            profile_)
            ->IsUnderAdvancedProtection());
  }

  // Make sure the pref state is logged and the synthetic field trial state is
  // created at startup (as the pref may never change over the session).
  bool enabled = profile_->GetPrefs()->GetBoolean(prefs::kHttpsOnlyModeEnabled);
  base::UmaHistogramBoolean("Security.HttpsFirstMode.SettingEnabledAtStartup",
                            enabled);
  ChromeMetricsServiceAccessor::RegisterSyntheticFieldTrial(
      kHttpsFirstModeSyntheticFieldTrialName,
      enabled ? kHttpsFirstModeSyntheticFieldTrialEnabledGroup
              : kHttpsFirstModeSyntheticFieldTrialDisabledGroup,
      variations::SyntheticTrialAnnotationMode::kCurrentLog);
}

HttpsFirstModeService::~HttpsFirstModeService() = default;

void HttpsFirstModeService::OnHttpsFirstModePrefChanged() {
  bool enabled = profile_->GetPrefs()->GetBoolean(prefs::kHttpsOnlyModeEnabled);
  base::UmaHistogramBoolean("Security.HttpsFirstMode.SettingChanged", enabled);
  // Update synthetic field trial group registration.
  ChromeMetricsServiceAccessor::RegisterSyntheticFieldTrial(
      kHttpsFirstModeSyntheticFieldTrialName,
      enabled ? kHttpsFirstModeSyntheticFieldTrialEnabledGroup
              : kHttpsFirstModeSyntheticFieldTrialDisabledGroup);

  // Reset the allowlist when the pref changes. A user going from HTTPS-Upgrades
  // to HTTPS-First Mode shouldn't inherit the set of allowlisted sites (or
  // vice versa).
  StatefulSSLHostStateDelegate* state =
      static_cast<StatefulSSLHostStateDelegate*>(
          profile_->GetSSLHostStateDelegate());
  state->ClearHttpsOnlyModeAllowlist();

  // Since the user modified the UI pref, explicitly disable any automatic
  // HTTPS-First Mode heuristic.
  profile_->GetPrefs()->SetBoolean(prefs::kHttpsOnlyModeAutoEnabled, false);
}

void HttpsFirstModeService::OnAdvancedProtectionStatusChanged(bool enabled) {
  DCHECK(base::FeatureList::IsEnabled(
      features::kHttpsFirstModeForAdvancedProtectionUsers));
  // Override the pref if AP is enabled. We explicitly don't unset the pref if
  // the user is no longer under Advanced Protection.
  if (enabled &&
      !profile_->GetPrefs()->GetBoolean(prefs::kHttpsOnlyModeEnabled)) {
    profile_->GetPrefs()->SetBoolean(prefs::kHttpsOnlyModeEnabled, true);
  }
}

bool HttpsFirstModeService::MaybeEnableHttpsFirstModeForUser(
    bool add_fallback_entry) {
  // The key for the fallback events in the base preference.
  constexpr char kFallbackEventsKey[] = "fallback_events";
  // The key in each fallback event for the event timestamp.
  constexpr char kTimestampKey[] = "timestamp";
  base::Time now = clock_->Now();

  if (!base::FeatureList::IsEnabled(
          features::kHttpsFirstModeV2ForTypicallySecureUsers)) {
    // Temporary fix for users impacted by crbug.com/1475747:
    // HFM-for-typically-secure-users has never been enabled intentionally. If
    // we see that the preference has been set, that was by accident. Unset the
    // relevant preferences to undo the damage.
    if (profile_->GetPrefs()->HasPrefPath(prefs::kHttpsOnlyModeAutoEnabled)) {
      profile_->GetPrefs()->SetBoolean(prefs::kHttpsOnlyModeAutoEnabled, false);
      // If HFM had already been enabled, the code wouldn't have toggled
      // kHttpsOnlyModeAutoEnabled. That means it's safe to disable HFM here --
      // HFM can only be enabled because we set it that way.
      profile_->GetPrefs()->SetBoolean(prefs::kHttpsOnlyModeEnabled, false);
    }

    return false;
  }

  const base::Value::Dict& base_pref =
      profile_->GetPrefs()->GetDict(prefs::kHttpsUpgradeFallbacks);

  base::Value::List new_entries;
  const base::Value::List* fallback_events =
      base_pref.FindList(kFallbackEventsKey);
  if (fallback_events) {
    for (const auto& event : *fallback_events) {
      const base::Value::Dict* fallback_event = event.GetIfDict();
      if (!fallback_event) {
        continue;
      }
      auto* event_timestamp_string = fallback_event->Find(kTimestampKey);
      if (!event_timestamp_string) {
        continue;
      }
      auto event_timestamp = base::ValueToTime(event_timestamp_string);
      if (!event_timestamp.has_value()) {
        // Invalid entry, ignore.
        continue;
      }
      if (event_timestamp.value() > now) {
        // Invalid timestamp, ignore.
        continue;
      }
      if (event_timestamp.value() < now - kFallbackEntriesRollingWindowSize) {
        // Old event, ignore.
        continue;
      }
      new_entries.Append(fallback_event->Clone());
    }
  }

  if (add_fallback_entry) {
    base::Value::Dict new_event;
    new_event.Set(kTimestampKey, base::TimeToValue(now));
    new_entries.Append(std::move(new_event));
  }

  size_t recent_warning_count = new_entries.size();

  auto* engagement_svc = site_engagement::SiteEngagementService::Get(profile_);
  bool enable_https_first_mode =
      ((now - profile_->GetCreationTime()) > kMinProfileAge) &&
      (recent_warning_count <= kMaxRecentFallbackEntryCount) &&
      (engagement_svc->GetTotalEngagementPoints() >=
       kMinTotalEngagementPointsForTypicallySecureUser.Get());

  // Update the pref with the new fallback events.
  base::Value::Dict new_base_pref;
  new_base_pref.Set(kFallbackEventsKey, std::move(new_entries));
  profile_->GetPrefs()->SetDict(prefs::kHttpsUpgradeFallbacks,
                                std::move(new_base_pref));

  // Automatically enable HTTPS-First Mode only if the HFM pref and the
  // auto-enable pref haven't been set before to avoid overriding user's
  // preferences.
  if (enable_https_first_mode &&
      !profile_->GetPrefs()->HasPrefPath(prefs::kHttpsOnlyModeEnabled) &&
      !profile_->GetPrefs()->HasPrefPath(prefs::kHttpsOnlyModeAutoEnabled)) {
    profile_->GetPrefs()->SetBoolean(prefs::kHttpsOnlyModeEnabled,
                                     enable_https_first_mode);
    profile_->GetPrefs()->SetBoolean(prefs::kHttpsOnlyModeAutoEnabled,
                                     enable_https_first_mode);
  }
  return enable_https_first_mode;
}

void HttpsFirstModeService::MaybeEnableHttpsFirstModeForUrl(const GURL& url) {
  // Ideal parameter order is kHttpsAddThreshold > kHttpsRemoveThreshold >
  // kHttpRemoveThreshold > kHttpAddThreshold.
  if (!(kHttpsAddThreshold.Get() > kHttpsRemoveThreshold.Get() &&
        kHttpsRemoveThreshold.Get() > kHttpRemoveThreshold.Get() &&
        kHttpRemoveThreshold.Get() > kHttpAddThreshold.Get())) {
    return;
  }

  StatefulSSLHostStateDelegate* state =
      static_cast<StatefulSSLHostStateDelegate*>(
          profile_->GetSSLHostStateDelegate());

  // StatefulSSLHostStateDelegate can be null during tests. In that case, we
  // can't save the site setting.
  if (!state) {
    return;
  }

  bool enforced = state->IsHttpsEnforcedForHost(
      url.host(), profile_->GetDefaultStoragePartition());
  GURL https_url = url.SchemeIsCryptographic() ? url : GetHttpsUrlFromHttp(url);
  GURL http_url = !url.SchemeIsCryptographic() ? url : GetHttpUrlFromHttps(url);

  auto* engagement_svc = site_engagement::SiteEngagementService::Get(profile_);

  double https_score = engagement_svc->GetScore(https_url);
  double http_score = engagement_svc->GetScore(http_url);
  bool should_enable = https_score >= kHttpsAddThreshold.Get() &&
                       http_score <= kHttpAddThreshold.Get();
  if (!enforced && should_enable) {
    state->SetHttpsEnforcementForHost(url.host(),
                                      /*enforced=*/true,
                                      profile_->GetDefaultStoragePartition());
    return;
  }

  bool should_disable = https_score <= kHttpsRemoveThreshold.Get() ||
                        http_score >= kHttpRemoveThreshold.Get();
  if (enforced && should_disable) {
    state->SetHttpsEnforcementForHost(url.host(),
                                      /*enforced=*/false,
                                      profile_->GetDefaultStoragePartition());
    return;
  }
  // Don't change the state otherwise.
}

void HttpsFirstModeService::SetClockForTesting(base::Clock* clock) {
  clock_ = clock;
}

// static
HttpsFirstModeService* HttpsFirstModeServiceFactory::GetForProfile(
    Profile* profile) {
  return static_cast<HttpsFirstModeService*>(
      GetInstance()->GetServiceForBrowserContext(profile, /*create=*/true));
}

// static
HttpsFirstModeServiceFactory* HttpsFirstModeServiceFactory::GetInstance() {
  return base::Singleton<HttpsFirstModeServiceFactory>::get();
}

// static
BrowserContextKeyedServiceFactory::TestingFactory
HttpsFirstModeServiceFactory::GetDefaultFactoryForTesting() {
  return base::BindRepeating(&BuildService);
}

HttpsFirstModeServiceFactory::HttpsFirstModeServiceFactory()
    : ProfileKeyedServiceFactory(
          kHttpsFirstModeServiceName,
          // Don't create a service for non-regular profiles. This includes
          // Incognito (which uses the settings of the main profile) and Guest
          // Mode.
          ProfileSelections::BuildForRegularProfile()) {
  DependsOn(
      safe_browsing::AdvancedProtectionStatusManagerFactory::GetInstance());
}

HttpsFirstModeServiceFactory::~HttpsFirstModeServiceFactory() = default;

KeyedService* HttpsFirstModeServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  return BuildService(context).release();
}
