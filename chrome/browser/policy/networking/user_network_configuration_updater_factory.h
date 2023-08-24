// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_NETWORKING_USER_NETWORK_CONFIGURATION_UPDATER_FACTORY_H_
#define CHROME_BROWSER_POLICY_NETWORKING_USER_NETWORK_CONFIGURATION_UPDATER_FACTORY_H_

#include "chrome/browser/profiles/profile_keyed_service_factory.h"

namespace base {
template <typename T>
class NoDestructor;
}  // namespace base

namespace content {
class BrowserContext;
}

namespace policy {

class UserNetworkConfigurationUpdater;

// Factory to create UserNetworkConfigurationUpdater for the the per-user
// OpenNetworkConfiguration policy.
class UserNetworkConfigurationUpdaterFactory
    : public ProfileKeyedServiceFactory {
 public:
  // Returns an existing or creates a new UserNetworkConfigurationUpdater for
  // |browser_context|. Will return nullptr if this service isn't allowed for
  // |browser_context|, i.e. for all but the BrowserContext which refers to the
  // primary user's profile.
  static UserNetworkConfigurationUpdater* GetForBrowserContext(
      content::BrowserContext* browser_context);

  static UserNetworkConfigurationUpdaterFactory* GetInstance();

  UserNetworkConfigurationUpdaterFactory(
      const UserNetworkConfigurationUpdaterFactory&) = delete;
  UserNetworkConfigurationUpdaterFactory& operator=(
      const UserNetworkConfigurationUpdaterFactory&) = delete;

 private:
  friend base::NoDestructor<UserNetworkConfigurationUpdaterFactory>;

  UserNetworkConfigurationUpdaterFactory();
  ~UserNetworkConfigurationUpdaterFactory() override;

  // BrowserContextKeyedServiceFactory:
  bool ServiceIsCreatedWithBrowserContext() const override;
  bool ServiceIsNULLWhileTesting() const override;
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
};

}  // namespace policy

#endif  // CHROME_BROWSER_POLICY_NETWORKING_USER_NETWORK_CONFIGURATION_UPDATER_FACTORY_H_
