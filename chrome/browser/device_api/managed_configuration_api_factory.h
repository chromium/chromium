// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DEVICE_API_MANAGED_CONFIGURATION_API_FACTORY_H_
#define CHROME_BROWSER_DEVICE_API_MANAGED_CONFIGURATION_API_FACTORY_H_

#include "base/macros.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

class Profile;
class ManagedConfigurationAPI;

namespace base {
template <typename T>
struct DefaultSingletonTraits;
}  // namespace base

// Factory for BrowserKeyedService ManagedConfigurationAPI.
class ManagedConfigurationAPIFactory
    : public BrowserContextKeyedServiceFactory {
 public:
  static ManagedConfigurationAPI* GetForProfile(Profile* profile);

  static ManagedConfigurationAPIFactory* GetInstance();

  ManagedConfigurationAPIFactory(const ManagedConfigurationAPIFactory&) =
      delete;
  ManagedConfigurationAPIFactory& operator=(
      const ManagedConfigurationAPIFactory&) = delete;

 private:
  friend struct base::DefaultSingletonTraits<ManagedConfigurationAPIFactory>;

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
