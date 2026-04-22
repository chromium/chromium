// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/omnibox/geolocation_header_service_factory.h"

#include "base/no_destructor.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "components/omnibox/browser/geolocation_header_service.h"
#include "components/omnibox/common/omnibox_features.h"

// static
GeolocationHeaderService* GeolocationHeaderServiceFactory::GetForProfile(
    Profile* profile) {
  return static_cast<GeolocationHeaderService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
GeolocationHeaderServiceFactory*
GeolocationHeaderServiceFactory::GetInstance() {
  static base::NoDestructor<GeolocationHeaderServiceFactory> instance;
  return instance.get();
}

GeolocationHeaderServiceFactory::GeolocationHeaderServiceFactory()
    : ProfileKeyedServiceFactory(
          "GeolocationHeaderService",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOriginalOnly)
              .WithGuest(ProfileSelection::kOriginalOnly)
              .Build()) {
  DependsOn(HostContentSettingsMapFactory::GetInstance());
  DependsOn(TemplateURLServiceFactory::GetInstance());
}

GeolocationHeaderServiceFactory::~GeolocationHeaderServiceFactory() = default;

std::unique_ptr<KeyedService>
GeolocationHeaderServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  if (!base::FeatureList::IsEnabled(omnibox::kPlatformAgnosticXGeo)) {
    return nullptr;
  }

  Profile* profile = Profile::FromBrowserContext(context);
  return std::make_unique<GeolocationHeaderService>(
      HostContentSettingsMapFactory::GetForProfile(profile),
      TemplateURLServiceFactory::GetForProfile(profile));
}
