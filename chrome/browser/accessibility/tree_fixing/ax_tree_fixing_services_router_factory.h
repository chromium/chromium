// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACCESSIBILITY_TREE_FIXING_AX_TREE_FIXING_SERVICES_ROUTER_FACTORY_H_
#define CHROME_BROWSER_ACCESSIBILITY_TREE_FIXING_AX_TREE_FIXING_SERVICES_ROUTER_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class Profile;

namespace user_prefs {
class PrefRegistrySyncable;
}  // namespace user_prefs

namespace tree_fixing {

class AXTreeFixingServicesRouter;

// Used to get or create an AXTreeFixingServicesRouter from a Profile.
class AXTreeFixingServicesRouterFactory : public ProfileKeyedServiceFactory {
 public:
  static AXTreeFixingServicesRouter* GetForProfile(Profile* profile);
  static AXTreeFixingServicesRouterFactory* GetInstance();

  AXTreeFixingServicesRouterFactory(const AXTreeFixingServicesRouterFactory&) =
      delete;
  AXTreeFixingServicesRouterFactory& operator=(
      const AXTreeFixingServicesRouterFactory&) = delete;

 private:
  friend class base::NoDestructor<AXTreeFixingServicesRouterFactory>;
  AXTreeFixingServicesRouterFactory();
  ~AXTreeFixingServicesRouterFactory() override;

  // BrowserContextKeyedServiceFactory:
  bool ServiceIsCreatedWithBrowserContext() const override;
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
  void RegisterProfilePrefs(
      user_prefs::PrefRegistrySyncable* registry) override;
};

}  // namespace tree_fixing

#endif  // CHROME_BROWSER_ACCESSIBILITY_TREE_FIXING_AX_TREE_FIXING_SERVICES_ROUTER_FACTORY_H_
