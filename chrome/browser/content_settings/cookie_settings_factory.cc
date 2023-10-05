// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/content_settings/cookie_settings_factory.h"

#include "base/check_op.h"
#include "base/metrics/histogram_functions.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/privacy_sandbox/tracking_protection_settings_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/content_settings/core/common/pref_names.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "components/privacy_sandbox/tracking_protection_settings.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/buildflags/buildflags.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "extensions/common/constants.h"
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/profiles/profile_helper.h"
#endif

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
  static base::NoDestructor<CookieSettingsFactory> instance;
  return instance.get();
}

CookieSettingsFactory::CookieSettingsFactory()
    : RefcountedProfileKeyedServiceFactory(
          "CookieSettings",
          // The incognito profile has its own content settings map. Therefore,
          // it should get its own CookieSettings.
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOwnInstance)
              // TODO(crbug.com/1418376): Check if this service is needed in
              // Guest mode.
              .WithGuest(ProfileSelection::kOwnInstance)
              .Build()) {
  DependsOn(HostContentSettingsMapFactory::GetInstance());
  DependsOn(TrackingProtectionSettingsFactory::GetInstance());
}

CookieSettingsFactory::~CookieSettingsFactory() = default;

void CookieSettingsFactory::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  content_settings::CookieSettings::RegisterProfilePrefs(registry);
}

scoped_refptr<RefcountedKeyedService>
CookieSettingsFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);
  PrefService* prefs = profile->GetPrefs();

  bool should_record_metrics = profile->IsRegularProfile();
#if BUILDFLAG(IS_CHROMEOS_ASH)
  // ChromeOS creates various irregular profiles (login, lock screen...); they
  // are of type kRegular (returns true for `Profile::IsRegular()`), that aren't
  // used to browse the web and users can't configure. Don't collect metrics
  // about them.
  should_record_metrics =
      should_record_metrics && ash::ProfileHelper::IsUserProfile(profile);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
  if (should_record_metrics) {
    // Record cookie setting histograms.
    auto cookie_controls_mode = static_cast<CookieControlsMode>(
        prefs->GetInteger(prefs::kCookieControlsMode));
    base::UmaHistogramBoolean(
        "Privacy.ThirdPartyCookieBlockingSetting.RegularProfile",
        cookie_controls_mode == CookieControlsMode::kBlockThirdParty);
    base::UmaHistogramEnumeration(
        "Privacy.CookieControlsSetting.RegularProfile", cookie_controls_mode);
  }

  const char* extension_scheme =
#if BUILDFLAG(ENABLE_EXTENSIONS)
      extensions::kExtensionScheme;
#else
      content_settings::kDummyExtensionScheme;
#endif

  auto* cookie_settings = new content_settings::CookieSettings(
      HostContentSettingsMapFactory::GetForProfile(profile), prefs,
      profile->IsIncognitoProfile(), extension_scheme);

  privacy_sandbox::TrackingProtectionSettings* tps =
      TrackingProtectionSettingsFactory::GetForProfile(profile);
  tps->AddObserver(cookie_settings);

  return cookie_settings;
}
