// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/privacy_sandbox/privacy_sandbox_settings_factory.h"

#include "base/no_destructor.h"
#include "chrome/browser/content_settings/cookie_settings_factory.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/privacy_sandbox/privacy_sandbox_settings_delegate.h"
#include "chrome/browser/privacy_sandbox/tracking_protection_settings_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/tpcd/experiment/experiment_manager_impl.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/privacy_sandbox/privacy_sandbox_settings_impl.h"

PrivacySandboxSettingsFactory* PrivacySandboxSettingsFactory::GetInstance() {
  static base::NoDestructor<PrivacySandboxSettingsFactory> instance;
  return instance.get();
}

privacy_sandbox::PrivacySandboxSettings*
PrivacySandboxSettingsFactory::GetForProfile(Profile* profile) {
  return static_cast<privacy_sandbox::PrivacySandboxSettings*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

PrivacySandboxSettingsFactory::PrivacySandboxSettingsFactory()
    : ProfileKeyedServiceFactory(
          "PrivacySandboxSettings",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOwnInstance)
              // TODO(crbug.com/1418376): Check if this service is needed in
              // Guest mode.
              .WithGuest(ProfileSelection::kOwnInstance)
              .Build()) {
  // This service implicitly DependsOn the CookieSettingsFactory,
  // HostContentSettingsMapFactory, and through the delegate, the
  // IdentityManagerFactory but for reasons, cannot explicitly depend on them
  // here. Instead, a scoped_refptr is held on CookieSettings, which itself
  // holds a scoped_refptr for the HostContentSettingsMap (and so this service
  // holds a raw ptr).
  // TODO (crbug.com/1400663): Unwind these "reasons" and improve this so that
  // the services can be explicitly depended on.
  DependsOn(TrackingProtectionSettingsFactory::GetInstance());
}

std::unique_ptr<KeyedService>
PrivacySandboxSettingsFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);

  return std::make_unique<privacy_sandbox::PrivacySandboxSettingsImpl>(
      std::make_unique<PrivacySandboxSettingsDelegate>(
          profile,
          tpcd::experiment::ExperimentManagerImpl::GetForProfile(profile)),
      HostContentSettingsMapFactory::GetForProfile(profile),
      CookieSettingsFactory::GetForProfile(profile),
      TrackingProtectionSettingsFactory::GetForProfile(profile),
      profile->GetPrefs());
}
