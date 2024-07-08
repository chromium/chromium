// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRIVATE_NETWORK_ACCESS_PRIVATE_NETWORK_DEVICE_PERMISSION_CONTEXT_FACTORY_H_
#define CHROME_BROWSER_PRIVATE_NETWORK_ACCESS_PRIVATE_NETWORK_DEVICE_PERMISSION_CONTEXT_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class PrivateNetworkDevicePermissionContext;
class Profile;

// This class gets the correct `PrivateNetworkDevicePermissionContext` for the
// current profile.
class PrivateNetworkDevicePermissionContextFactory
    : public ProfileKeyedServiceFactory {
 public:
  // Gets the private network device permission context for the current
  // user profile.
  // Returns nullptr if the `profile` is null or invalid.
  // Otherwise, if no context exists for the profile, creates one and returns
  // it.
  static PrivateNetworkDevicePermissionContext* GetForProfile(Profile* profile);

  // Returns nullptr if the `profile` is null, invalid or no context exists for
  // the profile. Otherwise, returns the existing profile.
  static PrivateNetworkDevicePermissionContext* GetForProfileIfExists(
      Profile* profile);

  // Gets the `PrivateNetworkDevicePermissionContextFactory` singleton.
  static PrivateNetworkDevicePermissionContextFactory* GetInstance();

  PrivateNetworkDevicePermissionContextFactory(
      const PrivateNetworkDevicePermissionContextFactory&) = delete;
  PrivateNetworkDevicePermissionContextFactory& operator=(
      const PrivateNetworkDevicePermissionContextFactory&) = delete;

 private:
  friend base::NoDestructor<PrivateNetworkDevicePermissionContextFactory>;

  PrivateNetworkDevicePermissionContextFactory();
  ~PrivateNetworkDevicePermissionContextFactory() override;

  // BrowserContextKeyedServiceFactory methods:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* profile) const override;
};

#endif  // CHROME_BROWSER_PRIVATE_NETWORK_ACCESS_PRIVATE_NETWORK_DEVICE_PERMISSION_CONTEXT_FACTORY_H_
