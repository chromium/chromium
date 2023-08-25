// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_NEARBY_PRESENCE_NEARBY_PRESENCE_SERVICE_FACTORY_H_
#define CHROME_BROWSER_ASH_NEARBY_PRESENCE_NEARBY_PRESENCE_SERVICE_FACTORY_H_

#include "chrome/browser/profiles/profile_keyed_service_factory.h"

namespace base {
template <typename T>
class NoDestructor;
}  // namespace base

namespace content {
class BrowserContext;
}  // namespace content

namespace ash::nearby::presence {

class NearbyPresenceService;

class NearbyPresenceServiceFactory : public ProfileKeyedServiceFactory {
 public:
  NearbyPresenceServiceFactory(const NearbyPresenceServiceFactory&) = delete;
  NearbyPresenceServiceFactory& operator=(const NearbyPresenceServiceFactory&) =
      delete;

  static NearbyPresenceServiceFactory* GetInstance();

  static NearbyPresenceService* GetForBrowserContext(
      content::BrowserContext* context);

 private:
  friend base::NoDestructor<NearbyPresenceServiceFactory>;

  NearbyPresenceServiceFactory();
  ~NearbyPresenceServiceFactory() override;

  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
  void RegisterProfilePrefs(
      user_prefs::PrefRegistrySyncable* registry) override;
};

}  // namespace ash::nearby::presence

#endif  // CHROME_BROWSER_ASH_NEARBY_PRESENCE_NEARBY_PRESENCE_SERVICE_FACTORY_H_
