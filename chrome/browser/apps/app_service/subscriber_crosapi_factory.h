// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_APP_SERVICE_SUBSCRIBER_CROSAPI_FACTORY_H_
#define CHROME_BROWSER_APPS_APP_SERVICE_SUBSCRIBER_CROSAPI_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class Profile;

namespace apps {

class SubscriberCrosapi;

// Singleton that owns all SubscriberCrosapi and associates them with
// Profiles.
class SubscriberCrosapiFactory : public ProfileKeyedServiceFactory {
 public:
  static SubscriberCrosapi* GetForProfile(Profile* profile);

  static SubscriberCrosapiFactory* GetInstance();

  static void ShutDownForTesting(content::BrowserContext* context);

 private:
  friend base::NoDestructor<SubscriberCrosapiFactory>;

  SubscriberCrosapiFactory();
  SubscriberCrosapiFactory(const SubscriberCrosapiFactory&) = delete;
  SubscriberCrosapiFactory& operator=(const SubscriberCrosapiFactory&) = delete;
  ~SubscriberCrosapiFactory() override = default;

  // BrowserContextKeyedServiceFactory overrides.
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
};

}  // namespace apps

#endif  // CHROME_BROWSER_APPS_APP_SERVICE_SUBSCRIBER_CROSAPI_FACTORY_H_
