// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/private_network_access/private_network_device_permission_context_factory.h"

#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/private_network_access/private_network_device_permission_context.h"
#include "chrome/browser/profiles/profile.h"

PrivateNetworkDevicePermissionContextFactory::
    PrivateNetworkDevicePermissionContextFactory()
    : ProfileKeyedServiceFactory(
          "PrivateNetworkDevicePermissionContext",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOwnInstance)
              .WithGuest(ProfileSelection::kOwnInstance)
              // TODO(crbug.com/41488885): Check if this service is needed for
              // Ash Internals.
              .WithAshInternals(ProfileSelection::kOwnInstance)
              .Build()) {
  DependsOn(HostContentSettingsMapFactory::GetInstance());
}

PrivateNetworkDevicePermissionContextFactory::
    ~PrivateNetworkDevicePermissionContextFactory() = default;

KeyedService*
PrivateNetworkDevicePermissionContextFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  return new PrivateNetworkDevicePermissionContext(
      Profile::FromBrowserContext(context));
}

// static
PrivateNetworkDevicePermissionContextFactory*
PrivateNetworkDevicePermissionContextFactory::GetInstance() {
  static base::NoDestructor<PrivateNetworkDevicePermissionContextFactory>
      instance;
  return instance.get();
}

// static
PrivateNetworkDevicePermissionContext*
PrivateNetworkDevicePermissionContextFactory::GetForProfile(Profile* profile) {
  return static_cast<PrivateNetworkDevicePermissionContext*>(
      GetInstance()->GetServiceForBrowserContext(profile, /*create=*/true));
}

PrivateNetworkDevicePermissionContext*
PrivateNetworkDevicePermissionContextFactory::GetForProfileIfExists(
    Profile* profile) {
  return static_cast<PrivateNetworkDevicePermissionContext*>(
      GetInstance()->GetServiceForBrowserContext(profile, /*create=*/false));
}
