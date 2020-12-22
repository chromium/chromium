// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/privacy_sandbox/privacy_sandbox_settings.h"

#include "base/feature_list.h"
#include "base/time/time.h"
#include "chrome/common/chrome_features.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/prefs/pref_service.h"
#include "components/privacy_sandbox/privacy_sandbox_prefs.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace {

bool IsCookiesClearOnExitEnabled(HostContentSettingsMap* map) {
  return map->GetDefaultContentSetting(ContentSettingsType::COOKIES,
                                       /*provider_id=*/nullptr) ==
         ContentSetting::CONTENT_SETTING_SESSION_ONLY;
}

bool HasNonDefaultBlockSetting(const ContentSettingsForOneType& cookie_settings,
                               const GURL& url,
                               const GURL& top_frame_origin) {
  // APIs are allowed unless there is an effective non-default cookie content
  // setting block exception. A default cookie content setting is one that has a
  // wildcard pattern for both primary and secondary patterns. Content
  // settings are listed in descending order of priority such that the first
  // that matches is the effective content setting. A default setting can appear
  // anywhere in the list. Content settings which appear after a default content
  // setting are completely superseded by that content setting and are thus not
  // consulted. Default settings which appear before other settings are applied
  // from higher precedence sources, such as policy. The value of a default
  // content setting applied by a higher precedence provider is not consulted
  // here. For managed policies, the state will be reflected directly in the
  // privacy sandbox preference. Other providers (such as extensions) will have
  // been considered for the initial value of the privacy sandbox preference.
  for (const auto& setting : cookie_settings) {
    if (setting.primary_pattern == ContentSettingsPattern::Wildcard() &&
        setting.secondary_pattern == ContentSettingsPattern::Wildcard()) {
      return false;
    }
    if (setting.primary_pattern.Matches(url) &&
        setting.secondary_pattern.Matches(top_frame_origin)) {
      return setting.GetContentSetting() ==
             ContentSetting::CONTENT_SETTING_BLOCK;
    }
  }
  // ContentSettingsForOneType should always end with a default content setting
  // from the default provider.
  NOTREACHED();
  return false;
}

}  // namespace

PrivacySandboxSettings::PrivacySandboxSettings(
    HostContentSettingsMap* host_content_settings_map,
    content_settings::CookieSettings* cookie_settings,
    PrefService* pref_service)
    : host_content_settings_map_(host_content_settings_map),
      cookie_settings_(cookie_settings),
      pref_service_(pref_service) {
  DCHECK(pref_service_);
  DCHECK(host_content_settings_map_);
  DCHECK(cookie_settings_);

  // "Clear on exit" causes a cookie deletion on shutdown. But for practical
  // purposes, we're notifying the observers on startup (which should be
  // equivalent, as no cookie operations could have happened while the profile
  // was shut down).
  if (IsCookiesClearOnExitEnabled(host_content_settings_map_))
    OnCookiesCleared();
}

PrivacySandboxSettings::~PrivacySandboxSettings() = default;

bool PrivacySandboxSettings::IsFlocAllowed(
    const GURL& url,
    const base::Optional<url::Origin>& top_frame_origin) const {
  ContentSettingsForOneType cookie_settings;
  cookie_settings_->GetCookieSettings(&cookie_settings);

  return IsPrivacySandboxAllowedForContext(url, top_frame_origin,
                                           cookie_settings);
}

base::Time PrivacySandboxSettings::FlocDataAccessibleSince() const {
  return pref_service_->GetTime(prefs::kPrivacySandboxFlocDataAccessibleSince);
}

bool PrivacySandboxSettings::IsConversionMeasurementAllowed(
    const url::Origin& top_frame_origin,
    const url::Origin& reporting_origin) const {
  ContentSettingsForOneType cookie_settings;
  cookie_settings_->GetCookieSettings(&cookie_settings);

  return IsPrivacySandboxAllowedForContext(reporting_origin.GetURL(),
                                           top_frame_origin, cookie_settings);
}

bool PrivacySandboxSettings::ShouldSendConversionReport(
    const url::Origin& impression_origin,
    const url::Origin& conversion_origin,
    const url::Origin& reporting_origin) const {
  // Re-using the |cookie_settings| allows this function to be faster
  // than simply calling IsConversionMeasurementAllowed() twice
  ContentSettingsForOneType cookie_settings;
  cookie_settings_->GetCookieSettings(&cookie_settings);

  // The |reporting_origin| needs to have been accessible in both impression
  // and conversion contexts. These are both checked when they occur, but
  // user settings may have changed between then and when the conversion report
  // is sent.
  return IsPrivacySandboxAllowedForContext(
             reporting_origin.GetURL(), impression_origin, cookie_settings) &&
         IsPrivacySandboxAllowedForContext(reporting_origin.GetURL(),
                                           reporting_origin, cookie_settings);
}

bool PrivacySandboxSettings::IsPrivacySandboxAllowed() {
  if (!base::FeatureList::IsEnabled(features::kPrivacySandboxSettings)) {
    // Simply respect 3rd-party cookies blocking settings if the UI is not
    // available.
    return !cookie_settings_->ShouldBlockThirdPartyCookies();
  }

  return pref_service_->GetBoolean(prefs::kPrivacySandboxApisEnabled);
}

void PrivacySandboxSettings::OnCookiesCleared() {
  pref_service_->SetTime(prefs::kPrivacySandboxFlocDataAccessibleSince,
                         base::Time::Now());

  for (auto& observer : observers_) {
    observer.OnFlocDataAccessibleSinceUpdated();
  }
}

void PrivacySandboxSettings::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void PrivacySandboxSettings::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

bool PrivacySandboxSettings::IsPrivacySandboxAllowedForContext(
    const GURL& url,
    const base::Optional<url::Origin>& top_frame_origin,
    const ContentSettingsForOneType& cookie_settings) const {
  if (!base::FeatureList::IsEnabled(features::kPrivacySandboxSettings)) {
    // Simply respect cookie settings if the UI is not available. An empty site
    // for cookies is provided so the context is always as a third party.
    return cookie_settings_->IsCookieAccessAllowed(url, GURL(),
                                                   top_frame_origin);
  }

  if (!pref_service_->GetBoolean(prefs::kPrivacySandboxApisEnabled))
    return false;

  // TODO (crbug.com/1155504): Bypassing the CookieSettings class to access
  // content settings directly ignores allowlisted schemes and the storage
  // access API. These should be taken into account here.
  return !HasNonDefaultBlockSetting(
      cookie_settings, url,
      top_frame_origin ? top_frame_origin->GetURL() : GURL());
}
