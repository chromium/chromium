// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GCM_INSTANCE_ID_INSTANCE_ID_PROFILE_SERVICE_FACTORY_H_
#define CHROME_BROWSER_GCM_INSTANCE_ID_INSTANCE_ID_PROFILE_SERVICE_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

namespace instance_id {

class InstanceIDProfileService;

// Singleton that owns all InstanceIDProfileService and associates them with
// profiles.
class InstanceIDProfileServiceFactory : public ProfileKeyedServiceFactory {
 public:
  static InstanceIDProfileService* GetForProfile(
      content::BrowserContext* profile);
  static InstanceIDProfileServiceFactory* GetInstance();

  InstanceIDProfileServiceFactory(const InstanceIDProfileServiceFactory&) =
      delete;
  InstanceIDProfileServiceFactory& operator=(
      const InstanceIDProfileServiceFactory&) = delete;

 private:
  friend base::NoDestructor<InstanceIDProfileServiceFactory>;

  InstanceIDProfileServiceFactory();
  ~InstanceIDProfileServiceFactory() override;

  // BrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* profile) const override;
};

}  // namespace instance_id

#endif  // CHROME_BROWSER_GCM_INSTANCE_ID_INSTANCE_ID_PROFILE_SERVICE_FACTORY_H_
