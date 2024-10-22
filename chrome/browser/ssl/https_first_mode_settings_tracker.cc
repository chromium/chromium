// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ssl/https_first_mode_settings_tracker.h"

#include <string_view>

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/json/values_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/task/thread_pool.h"
#include "base/time/default_clock.h"
#include "base/values.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/metrics/chrome_metrics_service_accessor.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/advanced_protection_status_manager_factory.h"
#include "chrome/browser/ssl/https_upgrades_interceptor.h"
#include "chrome/browser/ssl/https_upgrades_util.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/security_interstitials/content/https_only_mode_blocking_page.h"
#include "components/security_interstitials/content/stateful_ssl_host_state_delegate.h"
#include "components/site_engagement/content/site_engagement_service.h"
#include "components/variations/synthetic_trials.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"
#include "net/base/url_util.h"

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

// Maximum number of past HTTPS-Upgrade fallback events (i.e. would-be warnings)
// to auto-enable HFM, including the current fallback event that's being added
// to the events list.
const size_t kMaxRecentFallbackEntryCount = 2;

// Minimum age of the current browser profile to automatically enable HFM. This
// prevents auto-enabling HFM immediately upon first launch.
const base::TimeDelta kMinTypicallySecureProfileAge = base::Days(15);

// We should observe HTTPS-Upgrade and HFM navigations at least for this long
// before enabling HFM.
const base::TimeDelta kMinTypicallySecureObservationTime = base::Days(7);

// Minimum total score for a user to be considered typically secure. If the user
// doesn't have at least this much engagement score over all sites, they might
// not have used Chrome sufficiently for us to auto-enable HFM.
const base::FeatureParam<int> kMinTotalEngagementPointsForTypicallySecureUser{
    &features::kHttpsFirstModeV2ForTypicallySecureUsers,
    "min-total-site-engagement-score", 50};

// Rolling window size in days to count recent navigations. Navigations within
// this window will be counted to be used for the Typically Secure heuristic.
// Navigations older than this many days will be discarded from the count.
const base::FeatureParam<int> kNavigationCounterRollingWindowSizeInDays{
    &features::kHttpsFirstModeV2ForTypicallySecureUsers,
    "navigation-counts-rolling-window-size-in-days", 15};

// Minimum number of main frame navigations counted in this profile during a
// rolling window of kNavigationCounterDefaultRollingWindowSizeInDays for a user
// to be considered typically secure. If the user doesn't have at least this
// many navigations counted, they might not have used Chrome sufficiently for us
// to auto-enable HFM. A default value of 1500 is 100 navigations per day during
// the 15 day rolling window.
const base::FeatureParam<int> kMinRecentNavigationsForTypicallySecureUser{
    &features::kHttpsFirstModeV2ForTypicallySecureUsers,
    "min-recent-navigations", 1500};

// The key for the fallback events in the base preference.
constexpr char kFallbackEventsKey[] = "fallback_events";

// The key for the start timestamp in the base preference. This is the time
// when we started observing the profile with the Typically Secure User
// heuristic.
constexpr char kHeuristicStartTimestampKey[] = "heuristic_start_timestamp";

// The key in each fallback event for the fallback event timestamp. A fallback
// event is evicted from the list if this timestamp is older than
// kFallbackEntriesRollingWindowSize.
constexpr char kFallbackEventsPrefTimestampKey[] = "timestamp";

constexpr int kNavigationCounterDefaultSaveInterval = 10;

namespace {

using security_interstitials::https_only_mode::SiteEngagementHeuristicState;

const char kHttpsFirstModeServiceName[] = "HttpsFirstModeService";
const char kHttpsFirstModeSyntheticFieldTrialName[] =
    "HttpsFirstModeClientSetting";
const char kHttpsFirstModeSyntheticFieldTrialEnabledGroup[] = "Enabled";
const char kHttpsFirstModeSyntheticFieldTrialBalancedGroup[] = "Balanced";
const char kHttpsFirstModeSyntheticFieldTrialDisabledGroup[] = "Disabled";

// We don't need to protect this with a lock since it's only set while
// single-threaded in tests.
base::Clock* g_clock = nullptr;

base::Clock* GetClock() {
  return g_clock ? g_clock : base::DefaultClock::GetInstance();
}

// Returns the HTTP URL from `http_url` using the test port numbers, if any.
// TODO(crbug.com/40904694): Refactor and merge with UpgradeUrlToHttps().
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
    // TODO(crbug.com/40904694): Remove this exception.
    if (https_url != GURL(security_interstitials::HttpsOnlyModeBlockingPage::
                              kLearnMoreLink)) {
      DCHECK(!https_url.port().empty());
      upgrade_url.SetPortStr(port_str);
    }
  }

  return https_url.ReplaceComponents(upgrade_url);
}

// Returns the HTTPS URL from `http_url` using the test port numbers, if any.
// TODO(crbug.com/40904694): Refactor and merge with UpgradeUrlToHttps().
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
  return std::make_unique<HttpsFirstModeService>(profile, GetClock());
}

base::Time GetTimestamp(const base::Value::Dict& dict, const char* key) {
  const auto* timestamp_string = dict.Find(key);
  if (timestamp_string) {
    const auto timestamp = base::ValueToTime(timestamp_string);
    if (timestamp) {
      return *timestamp;
    }
  }
  return base::Time();
}

std::string GetSyntheticFieldTrialGroupName(HttpsFirstModeSetting setting) {
  switch (setting) {
    case HttpsFirstModeSetting::kEnabledFull:
      return kHttpsFirstModeSyntheticFieldTrialEnabledGroup;
    case HttpsFirstModeSetting::kEnabledBalanced:
      return kHttpsFirstModeSyntheticFieldTrialBalancedGroup;
    case HttpsFirstModeSetting::kDisabled:
      return kHttpsFirstModeSyntheticFieldTrialDisabledGroup;
    default:
      NOTREACHED_IN_MIGRATION();
      return "";
  }
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
  pref_change_registrar_.Add(
      prefs::kHttpsFirstBalancedMode,
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
  HttpsFirstModeSetting setting = GetCurrentSetting();
  base::UmaHistogramEnumeration(
      "Security.HttpsFirstMode.SettingEnabledAtStartup2", setting);
  ChromeMetricsServiceAccessor::RegisterSyntheticFieldTrial(
      kHttpsFirstModeSyntheticFieldTrialName,
      GetSyntheticFieldTrialGroupName(setting));

  // Restore navigation counts from the pref to be used in the Typically Secure
  // heuristic.
  navigation_counts_dict_ =
      profile_->GetPrefs()->GetDict(prefs::kHttpsUpgradeNavigations).Clone();
  navigation_counter_ = std::make_unique<DailyNavigationCounter>(
      &navigation_counts_dict_, clock_,
      kNavigationCounterRollingWindowSizeInDays.Get(),
      kNavigationCounterDefaultSaveInterval);

  content::GetUIThreadTaskRunner({base::TaskPriority::BEST_EFFORT})
      ->PostTask(FROM_HERE, base::BindOnce(&HttpsFirstModeService::AfterStartup,
                                           weak_factory_.GetWeakPtr()));
}

void HttpsFirstModeService::AfterStartup() {
  CheckUserIsTypicallySecureAndMaybeEnableHttpsFirstBalancedMode();
  MaybeEnableHttpsFirstModeForEngagedSites(base::OnceClosure());
}

void HttpsFirstModeService::
    CheckUserIsTypicallySecureAndMaybeEnableHttpsFirstBalancedMode() {
  if (!base::FeatureList::IsEnabled(
          features::kHttpsFirstModeV2ForTypicallySecureUsers) ||
      !IsBalancedModeAvailable()) {
    return;
  }

  // If HFM or the auto-enable prefs were previously set, do not modify them.
  if (profile_->GetPrefs()->HasPrefPath(prefs::kHttpsOnlyModeEnabled) ||
      profile_->GetPrefs()->HasPrefPath(prefs::kHttpsFirstBalancedMode) ||
      profile_->GetPrefs()->HasPrefPath(prefs::kHttpsOnlyModeAutoEnabled)) {
    return;
  }
  if (!IsUserTypicallySecure()) {
    return;
  }
  // The prefs must be set in this order, as setting kHttpsFirstBalancedMode
  // will cause kHttpsFirstBalancedModeEnabledByTypicallySecureHeuristic to be
  // reset to false.
  // TODO(crbug.com/349860796): Consider having the typically-secure heuristic
  // turn on Balanced Mode instead.
  keep_http_allowlist_on_next_pref_change_ = true;
  profile_->GetPrefs()->SetBoolean(prefs::kHttpsFirstBalancedMode, true);
  profile_->GetPrefs()->SetBoolean(prefs::kHttpsOnlyModeAutoEnabled, true);
}

HttpsFirstModeService::~HttpsFirstModeService() = default;

void HttpsFirstModeService::OnHttpsFirstModePrefChanged() {
  HttpsFirstModeSetting setting = GetCurrentSetting();
  // Update synthetic field trial group registration.
  ChromeMetricsServiceAccessor::RegisterSyntheticFieldTrial(
      kHttpsFirstModeSyntheticFieldTrialName,
      GetSyntheticFieldTrialGroupName(setting));

  // Reset the HTTP allowlist and HTTPS enforcelist when the pref changes.
  // A user going from HTTPS-Upgrades to HTTPS-First Mode shouldn't inherit the
  // set of allowlisted sites (or vice versa).
  if (!keep_http_allowlist_on_next_pref_change_) {
    StatefulSSLHostStateDelegate* state =
        static_cast<StatefulSSLHostStateDelegate*>(
            profile_->GetSSLHostStateDelegate());
    state->ClearHttpsOnlyModeAllowlist();
    state->ClearHttpsEnforcelist();
  }
  keep_http_allowlist_on_next_pref_change_ = false;

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

bool HttpsFirstModeService::
    IsInterstitialEnabledByTypicallySecureUserHeuristic() const {
  return base::FeatureList::IsEnabled(
             features::kHttpsFirstModeV2ForTypicallySecureUsers) &&
         IsBalancedModeAvailable() &&
         profile_->GetPrefs()->GetBoolean(prefs::kHttpsOnlyModeAutoEnabled) &&
         profile_->GetPrefs()->GetBoolean(prefs::kHttpsFirstBalancedMode);
}

void HttpsFirstModeService::RecordHttpsUpgradeFallbackEvent() {
  UpdateFallbackEntries(/*add_new_entry=*/true);
}

bool HttpsFirstModeService::IsUserTypicallySecure() {
  return UpdateFallbackEntries(/*add_new_entry=*/false);
}

bool HttpsFirstModeService::UpdateFallbackEntries(bool add_new_entry) {
  if (!base::FeatureList::IsEnabled(
          features::kHttpsFirstModeV2ForTypicallySecureUsers)) {
    return false;
  }
  // Profile shouldn't be too new.
  if ((clock_->Now() - profile_->GetCreationTime()) <
      kMinTypicallySecureProfileAge) {
    return false;
  }
  base::Time now = clock_->Now();
  const base::Value::Dict& base_pref =
      profile_->GetPrefs()->GetDict(prefs::kHttpsUpgradeFallbacks);

  base::Value::List new_entries;
  const base::Value::List* fallback_events =
      base_pref.FindList(kFallbackEventsKey);
  base::Time latest_fallback_timestamp;
  if (fallback_events) {
    for (const auto& event : *fallback_events) {
      const base::Value::Dict* fallback_event = event.GetIfDict();
      if (!fallback_event) {
        continue;
      }
      auto* event_timestamp_string =
          fallback_event->Find(kFallbackEventsPrefTimestampKey);
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
      if (event_timestamp.value() > latest_fallback_timestamp) {
        latest_fallback_timestamp = event_timestamp.value();
      }
    }
  }

  // Add the new fallback entry.
  if (add_new_entry) {
    base::Value::Dict new_event;
    new_event.Set(kFallbackEventsPrefTimestampKey, base::TimeToValue(now));
    new_entries.Append(std::move(new_event));
  }

  size_t recent_warning_count = new_entries.size();

  base::Time heuristic_start_timestamp =
      GetTimestamp(base_pref, kHeuristicStartTimestampKey);
  if (heuristic_start_timestamp.is_null()) {
    // This can happen in a new profile or if a previous version of Chrome
    // wrote the pref but didn't have this value.
    heuristic_start_timestamp = now;
  }

  auto* engagement_svc = site_engagement::SiteEngagementService::Get(profile_);
  bool enable_https_first_mode =
      ((now - heuristic_start_timestamp) >
       kMinTypicallySecureObservationTime) &&
      (recent_warning_count <= kMaxRecentFallbackEntryCount) &&
      (engagement_svc->GetTotalEngagementPoints() >=
       kMinTotalEngagementPointsForTypicallySecureUser.Get()) &&
      (now - latest_fallback_timestamp > base::Days(1)) &&
      (static_cast<int>(GetRecentNavigationCount()) >=
       kMinRecentNavigationsForTypicallySecureUser.Get());

  // Update the pref with the new fallback events.
  base::Value::Dict new_base_pref;
  new_base_pref.Set(kFallbackEventsKey, std::move(new_entries));
  new_base_pref.Set(kHeuristicStartTimestampKey,
                    base::TimeToValue(heuristic_start_timestamp));
  profile_->GetPrefs()->SetDict(prefs::kHttpsUpgradeFallbacks,
                                std::move(new_base_pref));
  return enable_https_first_mode;
}

void HttpsFirstModeService::MaybeEnableHttpsFirstModeForEngagedSites(
    base::OnceClosure done_callback) {
  // If HFM or the auto-enable prefs were previously set, do not modify HFM
  // status.
  if (!base::FeatureList::IsEnabled(
          features::kHttpsFirstModeV2ForEngagedSites) ||
      !IsBalancedModeAvailable() ||
      profile_->GetPrefs()->HasPrefPath(prefs::kHttpsOnlyModeEnabled) ||
      profile_->GetPrefs()->HasPrefPath(prefs::kHttpsFirstBalancedMode) ||
      profile_->GetPrefs()->HasPrefPath(prefs::kHttpsOnlyModeAutoEnabled)) {
    if (!done_callback.is_null()) {
      std::move(done_callback).Run();
    }
    return;
  }
  // Ideal parameter order is kHttpsAddThreshold > kHttpsRemoveThreshold >
  // kHttpRemoveThreshold > kHttpAddThreshold.
  if (!(kHttpsAddThreshold.Get() > kHttpsRemoveThreshold.Get() &&
        kHttpsRemoveThreshold.Get() > kHttpRemoveThreshold.Get() &&
        kHttpRemoveThreshold.Get() > kHttpAddThreshold.Get())) {
    if (!done_callback.is_null()) {
      std::move(done_callback).Run();
    }
    return;
  }
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::TaskPriority::USER_BLOCKING,
       base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN},
      base::BindOnce(
          &site_engagement::SiteEngagementService::GetAllDetailsInBackground,
          clock_->Now(),
          base::WrapRefCounted(
              HostContentSettingsMapFactory::GetForProfile(profile_)),
          site_engagement::SiteEngagementService::URLSets::HTTP),
      base::BindOnce(&HttpsFirstModeService::ProcessEngagedSitesList,
                     weak_factory_.GetWeakPtr(), std::move(done_callback)));
}

void HttpsFirstModeService::ProcessEngagedSitesList(
    base::OnceClosure done_callback,
    const std::vector<site_engagement::mojom::SiteEngagementDetails>& details) {
  DCHECK(IsBalancedModeAvailable());

  StatefulSSLHostStateDelegate* state =
      static_cast<StatefulSSLHostStateDelegate*>(
          profile_->GetSSLHostStateDelegate());
  // StatefulSSLHostStateDelegate can be null during tests. In that case, we
  // can't save the site setting.
  if (!state) {
    return;
  }
  auto* engagement_service =
      site_engagement::SiteEngagementService::Get(profile_);

  // Get all hostnames that have HTTPS enforced on them at some point. Some
  // hostnames may no longer have a site engagement score thus be missing from
  // `details`. We still want to process those hostnames because we want to
  // unenforce HTTPS on these hostnames if the conditions no longer hold.
  std::set<GURL> origins =
      state->GetHttpsEnforcedHosts(profile_->GetDefaultStoragePartition());
  for (const site_engagement::mojom::SiteEngagementDetails& detail : details) {
    origins.insert(detail.origin);
  }

  for (const GURL& origin : origins) {
    if (origin.SchemeIsHTTPOrHTTPS() && origin.port().empty()) {
      MaybeEnableHttpsFirstModeForUrl(origin, engagement_service, state);
    }
  }

  if (!done_callback.is_null()) {
    std::move(done_callback).Run();
  }
}

void HttpsFirstModeService::MaybeEnableHttpsFirstModeForUrl(
    const GURL& url,
    site_engagement::SiteEngagementService* engagement_service,
    StatefulSSLHostStateDelegate* state) {
  DCHECK(IsBalancedModeAvailable());

  DCHECK(url.port().empty()) << "Url should have a default port";
  bool enforced =
      state->IsHttpsEnforcedForUrl(url, profile_->GetDefaultStoragePartition());
  GURL https_url = url.SchemeIsCryptographic() ? url : GetHttpsUrlFromHttp(url);
  GURL http_url = !url.SchemeIsCryptographic() ? url : GetHttpUrlFromHttps(url);

  // If a non-unique hostname is in the enforcement list, it must have been
  // added by a previous version of Chrome, so remove it. Otherwise, ignore
  // non-unique hostnames.
  if (net::IsHostnameNonUnique(url.host())) {
    if (enforced) {
      state->SetHttpsEnforcementForHost(url.host(),
                                        /*enforced=*/false,
                                        profile_->GetDefaultStoragePartition());
    }
    return;
  }

  double https_score = engagement_service->GetScore(https_url);
  double http_score = engagement_service->GetScore(http_url);
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

HttpsFirstModeSetting HttpsFirstModeService::GetCurrentSetting() const {
  if (profile_->GetPrefs()->GetBoolean(prefs::kHttpsOnlyModeEnabled)) {
    return HttpsFirstModeSetting::kEnabledFull;
  }
  if (IsBalancedModeEnabled(profile_->GetPrefs())) {
    return HttpsFirstModeSetting::kEnabledBalanced;
  }
  return HttpsFirstModeSetting::kDisabled;
}

void HttpsFirstModeService::IncrementRecentNavigationCount() {
  if (navigation_counter_->Increment()) {
    profile_->GetPrefs()->SetDict(prefs::kHttpsUpgradeNavigations,
                                  navigation_counts_dict_.Clone());
  }
}

size_t HttpsFirstModeService::GetRecentNavigationCount() const {
  return navigation_counter_->GetTotal();
}

void HttpsFirstModeService::SetClockForTesting(base::Clock* clock) {
  clock_ = clock;
}

size_t HttpsFirstModeService::GetFallbackEntryCountForTesting() const {
  const base::Value::Dict& base_pref =
      profile_->GetPrefs()->GetDict(prefs::kHttpsUpgradeFallbacks);
  const base::Value::List* fallback_events =
      base_pref.FindList(kFallbackEventsKey);
  return fallback_events->size();
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
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOriginalOnly)
              // TODO(crbug.com/41488885): Check if this service is needed for
              // Ash Internals.
              .WithAshInternals(ProfileSelection::kOriginalOnly)
              .Build()) {
  DependsOn(
      safe_browsing::AdvancedProtectionStatusManagerFactory::GetInstance());
}

HttpsFirstModeServiceFactory::~HttpsFirstModeServiceFactory() = default;

std::unique_ptr<KeyedService>
HttpsFirstModeServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  return BuildService(context);
}

// static
base::Clock* HttpsFirstModeServiceFactory::SetClockForTesting(
    base::Clock* clock) {
  return std::exchange(g_clock, clock);
}
