// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lookalikes/lookalike_url_service_factory.h"

#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/lookalikes/lookalike_url_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_selections.h"

// static
LookalikeUrlService* LookalikeUrlServiceFactory::GetForProfile(
    Profile* profile) {
  return static_cast<LookalikeUrlService*>(
      GetInstance()->GetServiceForBrowserContext(profile,
                                                 /*create=*/true));
}

// static
LookalikeUrlServiceFactory* LookalikeUrlServiceFactory::GetInstance() {
  return base::Singleton<LookalikeUrlServiceFactory>::get();
}

LookalikeUrlServiceFactory::LookalikeUrlServiceFactory()
    : ProfileKeyedServiceFactory(
          "LookalikeUrlServiceFactory",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOwnInstance)
              // TODO(crbug.com/40257657): Check if this service is needed in
              // Guest mode.
              .WithGuest(ProfileSelection::kOwnInstance)
              // TODO(crbug.com/41488885): Check if this service is needed for
              // Ash Internals.
              .WithAshInternals(ProfileSelection::kOwnInstance)
              .Build()) {
  DependsOn(HostContentSettingsMapFactory::GetInstance());
}

LookalikeUrlServiceFactory::~LookalikeUrlServiceFactory() = default;

// BrowserContextKeyedServiceFactory:
KeyedService* LookalikeUrlServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  auto* profile = static_cast<Profile*>(context);
  return new LookalikeUrlService(
      profile->GetPrefs(),
      HostContentSettingsMapFactory::GetForProfile(profile));
}
