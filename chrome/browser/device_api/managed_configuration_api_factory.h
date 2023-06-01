// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DEVICE_API_MANAGED_CONFIGURATION_API_FACTORY_H_
#define CHROME_BROWSER_DEVICE_API_MANAGED_CONFIGURATION_API_FACTORY_H_

#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class Profile;
class ManagedConfigurationAPI;

namespace base {
template <typename T>
class NoDestructor;
}  // namespace base

// Factory for BrowserKeyedService ManagedConfigurationAPI.
class ManagedConfigurationAPIFactory : public ProfileKeyedServiceFactory {
 public:
  static ManagedConfigurationAPI* GetForProfile(Profile* profile);

  static ManagedConfigurationAPIFactory* GetInstance();

  ManagedConfigurationAPIFactory(const ManagedConfigurationAPIFactory&) =
      delete;
  ManagedConfigurationAPIFactory& operator=(
      const ManagedConfigurationAPIFactory&) = delete;

 private:
  friend base::NoDestructor<ManagedConfigurationAPIFactory>;

  ManagedConfigurationAPIFactory();
  ~ManagedConfigurationAPIFactory() override;

  // BrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
  void RegisterProfilePrefs(
      user_prefs::PrefRegistrySyncable* registry) override;
  bool ServiceIsCreatedWithBrowserContext() const override;
};

#endif  // CHROME_BROWSER_DEVICE_API_MANAGED_CONFIGURATION_API_FACTORY_H_
