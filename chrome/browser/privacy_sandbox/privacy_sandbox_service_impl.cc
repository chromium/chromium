// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/privacy_sandbox/privacy_sandbox_service_impl.h"

#include <algorithm>
#include <iterator>
#include <optional>

#include "base/feature_list.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/string_util.h"
#include "base/time/time.h"
#include "chrome/browser/privacy_sandbox/profile_bucket_metrics.h"
#include "chrome/browser/profiles/profile.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/content_settings/core/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/privacy_sandbox/privacy_sandbox_features.h"
#include "components/privacy_sandbox/privacy_sandbox_prefs.h"
#include "components/url_formatter/url_formatter.h"
#include "content/public/browser/browsing_data_filter_builder.h"
#include "content/public/browser/browsing_data_remover.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "net/base/schemeful_site.h"
#include "net/first_party_sets/first_party_set_entry.h"
#include "third_party/blink/public/common/features.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/profiles/profiles_state.h"
#include "chromeos/components/kiosk/kiosk_utils.h"
#include "chromeos/components/mgs/managed_guest_session_utils.h"
#endif

namespace {

// Returns whether 3P cookies are blocked by |cookie_settings|. This can be
// either through blocking 3P cookies directly, or blocking all cookies.
// Blocking in this case also covers the "3P cookies limited" state.
bool ShouldBlockThirdPartyOrFirstPartyCookies(
    content_settings::CookieSettings* cookie_settings) {
  const auto default_content_setting =
      cookie_settings->GetDefaultCookieSetting();
  return cookie_settings->ShouldBlockThirdPartyCookies() ||
         default_content_setting == ContentSetting::CONTENT_SETTING_BLOCK;
}

// Returns whether |profile_type|, and the current browser session on CrOS,
// represent a regular (e.g. non guest, non system, non kiosk) profile.
bool IsRegularProfile(profile_metrics::BrowserProfileType profile_type) {
  if (profile_type != profile_metrics::BrowserProfileType::kRegular) {
    return false;
  }

#if BUILDFLAG(IS_CHROMEOS)
  // Any Device Local account, which is a CrOS concept powering things like
  // Kiosks and Managed Guest Sessions, is not considered regular.
  return !chromeos::IsManagedGuestSession() && !chromeos::IsKioskSession() &&
         !profiles::IsChromeAppKioskSession();
#else
  return true;
#endif
}

// Emits startup histograms relating to the user's topics enabled status on
// both client and profile level.
void RecordTopicsEnabledHistograms(Profile* profile, bool enabled) {
  std::optional<privacy_sandbox::ProfileEnabledState> profile_enabled_state =
      privacy_sandbox::GetProfileEnabledState(profile, enabled);

  if (profile_enabled_state) {
    base::UmaHistogramEnumeration(
        "Settings.PrivacySandbox.Topics.EnabledForProfile",
        profile_enabled_state.value());
  }
  base::UmaHistogramBoolean("Settings.PrivacySandbox.Topics.Enabled", enabled);
}

// Emits startup histograms relating to the user's fledge enabled status on
// both client and profile level.
void RecordProtectedAudienceEnabledHistograms(Profile* profile, bool enabled) {
  std::optional<privacy_sandbox::ProfileEnabledState> profile_enabled_state =
      privacy_sandbox::GetProfileEnabledState(profile, enabled);

  if (profile_enabled_state) {
    base::UmaHistogramEnumeration(
        "Settings.PrivacySandbox.Fledge.EnabledForProfile",
        profile_enabled_state.value());
  }
  base::UmaHistogramBoolean("Settings.PrivacySandbox.Fledge.Enabled", enabled);
}

// Emits startup histograms relating to the user's AdMeasurement enabled
// status on both client and profile level.
void RecordAdMeasurementEnabledHistograms(Profile* profile, bool enabled) {
  std::optional<privacy_sandbox::ProfileEnabledState> profile_enabled_state =
      privacy_sandbox::GetProfileEnabledState(profile, enabled);

  if (profile_enabled_state) {
    base::UmaHistogramEnumeration(

        "Settings.PrivacySandbox.AdMeasurement.EnabledForProfile",
        profile_enabled_state.value());
  }
  base::UmaHistogramBoolean("Settings.PrivacySandbox.AdMeasurement.Enabled",
                            enabled);
}

}  // namespace

PrivacySandboxServiceImpl::PrivacySandboxServiceImpl(
    Profile* profile,
    privacy_sandbox::PrivacySandboxSettings* privacy_sandbox_settings,
    scoped_refptr<content_settings::CookieSettings> cookie_settings,
    PrefService* pref_service,
    profile_metrics::BrowserProfileType profile_type,
    content::BrowsingDataRemover* browsing_data_remover,
    HostContentSettingsMap* host_content_settings_map,
    first_party_sets::FirstPartySetsPolicyService* first_party_sets_service)
    : profile_(profile),
      privacy_sandbox_settings_(privacy_sandbox_settings),
      cookie_settings_(cookie_settings),
      pref_service_(pref_service),
      profile_type_(profile_type),
      browsing_data_remover_(browsing_data_remover),
      host_content_settings_map_(host_content_settings_map),
      first_party_sets_policy_service_(first_party_sets_service) {
  DCHECK(privacy_sandbox_settings_);
  DCHECK(pref_service_);
  DCHECK(cookie_settings_);
  // Register observers for the Privacy Sandbox preferences.
  user_prefs_registrar_.Init(pref_service_);

  // Clears the Topics, Fledge, and Measurement Privacy Sandbox API Prefs, if
  // the PrivacySandboxAdPrivacyUxDeprecation feature is enabled.
  privacy_sandbox::MaybeClearAdPrivacyPrefs(pref_service_);

  // Check for FPS pref init at each startup.
  // TODO(crbug.com/40234448): Remove this logic when most users have run init.
  MaybeInitializeRelatedWebsiteSetsPref();

  // Record preference state for UMA at each startup.
  LogPrivacySandboxState();
}

PrivacySandboxServiceImpl::~PrivacySandboxServiceImpl() = default;

void PrivacySandboxServiceImpl::Shutdown() {
  user_prefs_registrar_.RemoveAll();
  first_party_sets_policy_service_ = nullptr;
  host_content_settings_map_ = nullptr;
  browsing_data_remover_ = nullptr;
  pref_service_ = nullptr;
  cookie_settings_ = nullptr;
  privacy_sandbox_settings_ = nullptr;
  profile_ = nullptr;
}

void PrivacySandboxServiceImpl::ForceChromeBuildForTests(
    bool force_chrome_build) {
  force_chrome_build_for_tests_ = force_chrome_build;
}

void PrivacySandboxServiceImpl::SetRelatedWebsiteSetsDataAccessEnabled(
    bool enabled) {
  pref_service_->SetBoolean(prefs::kPrivacySandboxRelatedWebsiteSetsEnabled,
                            enabled);
}

bool PrivacySandboxServiceImpl::IsRelatedWebsiteSetsDataAccessEnabled() const {
  return privacy_sandbox_settings_->AreRelatedWebsiteSetsEnabled();
}

bool PrivacySandboxServiceImpl::IsRelatedWebsiteSetsDataAccessManaged() const {
  return pref_service_->IsManagedPreference(
      prefs::kPrivacySandboxRelatedWebsiteSetsEnabled);
}

std::optional<net::SchemefulSite>
PrivacySandboxServiceImpl::GetRelatedWebsiteSetOwner(
    const GURL& site_url) const {
  // If RWS is not affecting cookie access, then there are effectively no
  // related website sets.
  if (!cookie_settings_->ShouldBlockThirdPartyCookies() ||
      cookie_settings_->GetDefaultCookieSetting() == CONTENT_SETTING_BLOCK) {
    return std::nullopt;
  }

  std::optional<net::FirstPartySetEntry> site_entry =
      first_party_sets_policy_service_->FindEntry(net::SchemefulSite(site_url));
  if (!site_entry.has_value()) {
    return std::nullopt;
  }

  return site_entry->primary();
}

std::optional<std::u16string>
PrivacySandboxServiceImpl::GetRelatedWebsiteSetOwnerForDisplay(
    const GURL& site_url) const {
  std::optional<net::SchemefulSite> site_owner =
      GetRelatedWebsiteSetOwner(site_url);
  if (!site_owner.has_value()) {
    return std::nullopt;
  }

  return url_formatter::IDNToUnicode(site_owner->GetURL().GetHost());
}

bool PrivacySandboxServiceImpl::IsPartOfManagedRelatedWebsiteSet(
    const net::SchemefulSite& site) const {
  return first_party_sets_policy_service_->IsSiteInManagedSet(site);
}


void PrivacySandboxServiceImpl::RecordFirstPartySetsStateHistogram() {
  auto rws_status = FirstPartySetsState::kFpsNotRelevant;
  if (cookie_settings_->ShouldBlockThirdPartyCookies() &&
      cookie_settings_->GetDefaultCookieSetting() != CONTENT_SETTING_BLOCK) {
    rws_status = privacy_sandbox_settings_->AreRelatedWebsiteSetsEnabled()
                     ? FirstPartySetsState::kFpsEnabled
                     : FirstPartySetsState::kFpsDisabled;
  }
  base::UmaHistogramEnumeration("Settings.FirstPartySets.State", rws_status);
}

void PrivacySandboxServiceImpl::RecordTrackingProtectionStateHistogram() {
  base::UmaHistogramBoolean(
      "Settings.TrackingProtection.Enabled",
      pref_service_->GetBoolean(prefs::kTrackingProtection3pcdEnabled));
}

void PrivacySandboxServiceImpl::LogPrivacySandboxState() {
  // Do not record metrics for non-regular profiles.
  if (!IsRegularProfile(profile_type_)) {
    return;
  }
  RecordFirstPartySetsStateHistogram();
  RecordTrackingProtectionStateHistogram();

  // Record the status of the APIs.
  const bool topics_enabled =
      pref_service_->GetBoolean(prefs::kPrivacySandboxM1TopicsEnabled);
  const bool fledge_enabled =
      pref_service_->GetBoolean(prefs::kPrivacySandboxM1FledgeEnabled);
  const bool ad_measurement_enabled =
      pref_service_->GetBoolean(prefs::kPrivacySandboxM1AdMeasurementEnabled);

  RecordTopicsEnabledHistograms(profile_, topics_enabled);
  RecordProtectedAudienceEnabledHistograms(profile_, fledge_enabled);
  RecordAdMeasurementEnabledHistograms(profile_, ad_measurement_enabled);
}

void PrivacySandboxServiceImpl::MaybeInitializeRelatedWebsiteSetsPref() {
  // If initialization has already run, it is not required.
  if (pref_service_->GetBoolean(
          prefs::
              kPrivacySandboxRelatedWebsiteSetsDataAccessAllowedInitialized)) {
    return;
  }

  // If the user blocks 3P cookies, disable the RWS data access preference.
  // As this logic relies on checking synced preference state, it is possible
  // that synced state is available when this decision is made. To err on the
  // side of privacy, this init logic is run per-device (the pref recording
  // that init has been run is not synced). If any of the user's devices local
  // state would disable the pref, it is disabled across all devices.
  if (ShouldBlockThirdPartyOrFirstPartyCookies(cookie_settings_.get())) {
    pref_service_->SetBoolean(prefs::kPrivacySandboxRelatedWebsiteSetsEnabled,
                              false);
  }

  pref_service_->SetBoolean(
      prefs::kPrivacySandboxRelatedWebsiteSetsDataAccessAllowedInitialized,
      true);
}


