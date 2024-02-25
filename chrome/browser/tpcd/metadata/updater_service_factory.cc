// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/tpcd/metadata/updater_service_factory.h"

#include "chrome/browser/content_settings/cookie_settings_factory.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/tpcd/metadata/updater_service.h"
#include "net/base/features.h"

namespace tpcd::metadata {
// static
UpdaterServiceFactory* UpdaterServiceFactory::GetInstance() {
  static base::NoDestructor<UpdaterServiceFactory> factory;
  return factory.get();
}

// static
UpdaterService* UpdaterServiceFactory::GetForProfile(Profile* profile) {
  return static_cast<UpdaterService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
ProfileSelections UpdaterServiceFactory::CreateProfileSelections() {
  if (!base::FeatureList::IsEnabled(net::features::kTpcdMetadataGrants)) {
    return ProfileSelections::BuildNoProfilesSelected();
  }

  return ProfileSelections::Builder()
      .WithRegular(ProfileSelection::kOriginalOnly)
      .WithGuest(ProfileSelection::kOwnInstance)
      // The Following will be completely unselected as users do not "browse"
      // within this profiles.
      .WithSystem(ProfileSelection::kNone)
      .WithAshInternals(ProfileSelection::kNone)
      .Build();
  ;
}

UpdaterServiceFactory::UpdaterServiceFactory()
    : ProfileKeyedServiceFactory("UpdaterService", CreateProfileSelections()) {
  DependsOn(CookieSettingsFactory::GetInstance());
  DependsOn(HostContentSettingsMapFactory::GetInstance());
}

UpdaterServiceFactory::~UpdaterServiceFactory() = default;

KeyedService* UpdaterServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  return new UpdaterService(context);
}

bool UpdaterServiceFactory::ServiceIsCreatedWithBrowserContext() const {
  // The `UpdaterService` needs to be created with `BrowserContext`, as it needs
  // to be ready to update the ContentSettingsForOneType, right away, once the
  // parser parses a metadata component.
  return true;
}

}  // namespace tpcd::metadata
