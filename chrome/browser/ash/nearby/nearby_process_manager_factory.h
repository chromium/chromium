// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_NEARBY_NEARBY_PROCESS_MANAGER_FACTORY_H_
#define CHROME_BROWSER_ASH_NEARBY_NEARBY_PROCESS_MANAGER_FACTORY_H_

#include "base/memory/singleton.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class Profile;

namespace ash {
namespace nearby {

class NearbyProcessManager;

// Creates a NearbyProcessManager for the primary user. No instance is created
// any other profile.
class NearbyProcessManagerFactory : public ProfileKeyedServiceFactory {
 public:
  static NearbyProcessManager* GetForProfile(Profile* profile);

  // Returns true if the nearby process can be launched for |profile|
  static bool CanBeLaunchedForProfile(Profile* profile);

  static NearbyProcessManagerFactory* GetInstance();

  // When true is passed, this factory will create a NearbyProcessManager even
  // when it is not the primary profile.
  static void SetBypassPrimaryUserCheckForTesting(
      bool bypass_primary_user_check_for_testing);

 private:
  friend struct base::DefaultSingletonTraits<NearbyProcessManagerFactory>;

  NearbyProcessManagerFactory();
  NearbyProcessManagerFactory(const NearbyProcessManagerFactory&) = delete;
  NearbyProcessManagerFactory& operator=(const NearbyProcessManagerFactory&) =
      delete;
  ~NearbyProcessManagerFactory() override;

  // BrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
  bool ServiceIsCreatedWithBrowserContext() const override;
};

}  // namespace nearby
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_NEARBY_NEARBY_PROCESS_MANAGER_FACTORY_H_
