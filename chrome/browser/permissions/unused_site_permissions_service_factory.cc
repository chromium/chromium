// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/permissions/unused_site_permissions_service_factory.h"

#include "base/no_destructor.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/permissions/unused_site_permissions_service.h"

// static
UnusedSitePermissionsServiceFactory*
UnusedSitePermissionsServiceFactory::GetInstance() {
  static base::NoDestructor<UnusedSitePermissionsServiceFactory> instance;
  return instance.get();
}

// static
permissions::UnusedSitePermissionsService*
UnusedSitePermissionsServiceFactory::GetForProfile(Profile* profile) {
  return static_cast<permissions::UnusedSitePermissionsService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

UnusedSitePermissionsServiceFactory::UnusedSitePermissionsServiceFactory()
    : ProfileKeyedServiceFactory(
          "UnusedSitePermissionsService",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOriginalOnly)
              // TODO(crbug.com/1418376): Check if this service is needed in
              // Guest mode.
              .WithGuest(ProfileSelection::kOriginalOnly)
              .Build()) {
  DependsOn(HostContentSettingsMapFactory::GetInstance());
}

UnusedSitePermissionsServiceFactory::~UnusedSitePermissionsServiceFactory() =
    default;

KeyedService* UnusedSitePermissionsServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  auto* service = new permissions::UnusedSitePermissionsService(
      HostContentSettingsMapFactory::GetForProfile(context));
  service->StartRepeatedUpdates();
  return service;
}
