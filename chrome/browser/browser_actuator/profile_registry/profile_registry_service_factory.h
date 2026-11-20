// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BROWSER_ACTUATOR_PROFILE_REGISTRY_PROFILE_REGISTRY_SERVICE_FACTORY_H_
#define CHROME_BROWSER_BROWSER_ACTUATOR_PROFILE_REGISTRY_PROFILE_REGISTRY_SERVICE_FACTORY_H_

#include <memory>

#include "chrome/browser/profiles/profile_keyed_service_factory.h"

namespace base {
template <typename T>
class NoDestructor;
}  // namespace base

namespace content {
class BrowserContext;
}  // namespace content

class KeyedService;
class Profile;

namespace browser_actuator {

class ProfileRegistryService;

// Creates a `ProfileRegistryService` for each non-ephemeral regular profile
// when `kBrowserActuatorProfileRegistry` is enabled, Chrome runs from its
// default user data directory, and the binary carries the
// `kLocalActuationKeychainAccessGroup` entitlement.
class ProfileRegistryServiceFactory : public ProfileKeyedServiceFactory {
 public:
  // Returns null if `profile` does not meet the conditions above.
  static ProfileRegistryService* GetForProfile(Profile* profile);
  static ProfileRegistryServiceFactory* GetInstance();

  ProfileRegistryServiceFactory(const ProfileRegistryServiceFactory&) = delete;
  ProfileRegistryServiceFactory& operator=(
      const ProfileRegistryServiceFactory&) = delete;

 private:
  friend base::NoDestructor<ProfileRegistryServiceFactory>;

  ProfileRegistryServiceFactory();
  ~ProfileRegistryServiceFactory() override;

  // BrowserContextKeyedServiceFactory:
  bool ServiceIsCreatedWithBrowserContext() const override;
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
};

}  // namespace browser_actuator

#endif  // CHROME_BROWSER_BROWSER_ACTUATOR_PROFILE_REGISTRY_PROFILE_REGISTRY_SERVICE_FACTORY_H_
