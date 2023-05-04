// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ssl/https_first_mode_settings_tracker.h"

#include "base/feature_list.h"
#include "base/metrics/histogram_functions.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/metrics/chrome_metrics_service_accessor.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/advanced_protection_status_manager_factory.h"
#include "chrome/browser/ssl/https_only_mode_tab_helper.h"
#include "chrome/browser/ssl/https_upgrades_interceptor.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/security_interstitials/content/https_only_mode_blocking_page.h"
#include "components/security_interstitials/content/stateful_ssl_host_state_delegate.h"
#include "components/site_engagement/content/site_engagement_service.h"
#include "components/variations/synthetic_trials.h"
#include "content/public/browser/browser_context.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/profiles/profile_helper.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

namespace {

using security_interstitials::https_only_mode::SiteEngagementHeuristicState;

const char kHttpsFirstModeServiceName[] = "HttpsFirstModeService";
const char kHttpsFirstModeSyntheticFieldTrialName[] =
    "HttpsFirstModeClientSetting";
const char kHttpsFirstModeSyntheticFieldTrialEnabledGroup[] = "Enabled";
const char kHttpsFirstModeSyntheticFieldTrialDisabledGroup[] = "Disabled";

// Minimum score of an HTTPS origin to enable HFM on its hostname.
// TODO(crbug.com/1435222): Convert these into feature params.
constexpr double kHttpsAddThreshold = 40;

// Maximum score of an HTTP origin to enable HFM on its hostname.
constexpr double kHttpAddThreshold = 5;

// If HTTPS score goes below kHttpsRemoveThreshold or HTTP score goes above
// kHttpRemoveThreshold, disable HFM on this hostname.
constexpr double kHttpsRemoveThreshold = 30;
constexpr double kHttpRemoveThreshold = 10;

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
  return std::make_unique<HttpsFirstModeService>(profile);
}

}  // namespace

HttpsFirstModeService::HttpsFirstModeService(Profile* profile)
    : profile_(profile) {
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

void HttpsFirstModeService::MaybeEnableHttpsFirstModeForUrl(Profile* profile,
                                                            const GURL& url) {
  StatefulSSLHostStateDelegate* state =
      static_cast<StatefulSSLHostStateDelegate*>(
          profile_->GetSSLHostStateDelegate());
  static_assert(kHttpsAddThreshold > kHttpsRemoveThreshold);
  static_assert(kHttpAddThreshold < kHttpRemoveThreshold);

  // StatefulSSLHostStateDelegate can be null during tests. In that case, we
  // can't save the site setting.
  if (!state) {
    return;
  }

  bool enforced = state->IsHttpsEnforcedForHost(
      url.host(), profile->GetDefaultStoragePartition());
  GURL https_url = url.SchemeIsCryptographic() ? url : GetHttpsUrlFromHttp(url);
  GURL http_url = !url.SchemeIsCryptographic() ? url : GetHttpUrlFromHttps(url);

  auto* engagement_svc = site_engagement::SiteEngagementService::Get(profile);

  double https_score = engagement_svc->GetScore(https_url);
  double http_score = engagement_svc->GetScore(http_url);
  bool should_enable =
      https_score >= kHttpsAddThreshold && http_score <= kHttpAddThreshold;
  if (!enforced && should_enable) {
    state->SetHttpsEnforcementForHost(url.host(),
                                      /*enforced=*/true,
                                      profile->GetDefaultStoragePartition());
    return;
  }

  bool should_disable = https_score <= kHttpsRemoveThreshold ||
                        http_score >= kHttpRemoveThreshold;
  if (enforced && should_disable) {
    state->SetHttpsEnforcementForHost(url.host(),
                                      /*enforced=*/false,
                                      profile->GetDefaultStoragePartition());
    return;
  }
  // Don't change the state otherwise.
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
