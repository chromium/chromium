// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/content_settings/cookie_settings_factory.h"

#include "base/check_op.h"
#include "base/metrics/histogram_functions.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/net/prediction_options.h"
#include "chrome/browser/profiles/incognito_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/content_settings/core/common/pref_names.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "components/spellcheck/browser/pref_names.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/common/constants.h"

using content_settings::CookieControlsMode;

// static
scoped_refptr<content_settings::CookieSettings>
CookieSettingsFactory::GetForProfile(Profile* profile) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  return static_cast<content_settings::CookieSettings*>(
      GetInstance()->GetServiceForBrowserContext(profile, true).get());
}

// static
CookieSettingsFactory* CookieSettingsFactory::GetInstance() {
  return base::Singleton<CookieSettingsFactory>::get();
}

CookieSettingsFactory::CookieSettingsFactory()
    : RefcountedBrowserContextKeyedServiceFactory(
          "CookieSettings",
          BrowserContextDependencyManager::GetInstance()) {
  DependsOn(HostContentSettingsMapFactory::GetInstance());
}

CookieSettingsFactory::~CookieSettingsFactory() = default;

void CookieSettingsFactory::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  content_settings::CookieSettings::RegisterProfilePrefs(registry);
}

content::BrowserContext* CookieSettingsFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  // The incognito profile has its own content settings map. Therefore, it
  // should get its own CookieSettings.
  return chrome::GetBrowserContextOwnInstanceInIncognito(context);
}

scoped_refptr<RefcountedKeyedService>
CookieSettingsFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);
  PrefService* prefs = profile->GetPrefs();

  // Record cookie setting histograms.
  auto cookie_controls_mode = static_cast<CookieControlsMode>(
      prefs->GetInteger(prefs::kCookieControlsMode));
  base::UmaHistogramBoolean(
      "Privacy.ThirdPartyCookieBlockingSetting",
      cookie_controls_mode == CookieControlsMode::kBlockThirdParty);
  base::UmaHistogramEnumeration("Privacy.CookieControlsSetting",
                                cookie_controls_mode);
  // The DNT setting is only vaguely cookie-related. However, there is currently
  // no DNT-related code that is executed once per Profile lifetime, and
  // creating a new BrowserContextKeyedService to record this metric would be
  // an overkill. Hence, we put it here.
  // TODO(crbug.com/1228614): Find a better place for this metric.
  base::UmaHistogramBoolean("Privacy.DoNotTrackSetting",
                            prefs->GetBoolean(prefs::kEnableDoNotTrack));
  // The preload setting exists on the cookie page, to avoid creating a new
  // BrowserContextKeyedService to record this metric it will live here.
  // TODO(crbug.com/1228614): Find a better place for this metric.
  auto preload_setting_status =
      static_cast<chrome_browser_net::NetworkPredictionOptions>(
          prefs->GetInteger(prefs::kNetworkPredictionOptions));
  base::UmaHistogramBoolean(
      "Settings.PreloadStatus.OnStartup",
      (preload_setting_status != chrome_browser_net::NETWORK_PREDICTION_NEVER));

  // The advanced spellcheck setting exists on the sync setup page, not the
  // cookies page, but to avoid creating a new BrowserContextKeyedService to
  // record this metric it will live here.
  // TODO(crbug.com/1228614): Find a better place for this metric.
  base::UmaHistogramBoolean("Settings.AutocompleteSearches.OnStartup",
                            prefs->GetBoolean(::prefs::kSearchSuggestEnabled));

  // The autocomplete searches setting exists on the sync setup page, not the
  // cookies page, but to avoid creating a new BrowserContextKeyedService to
  // record this metric it will live here.
  // TODO(crbug.com/1228614): Find a better place for this metric.
  base::UmaHistogramBoolean(
      "Settings.AdvancedSpellcheck.OnStartup",
      prefs->GetBoolean(::spellcheck::prefs::kSpellCheckUseSpellingService));

  return new content_settings::CookieSettings(
      HostContentSettingsMapFactory::GetForProfile(profile), prefs,
      profile->IsIncognitoProfile(), extensions::kExtensionScheme);
}
