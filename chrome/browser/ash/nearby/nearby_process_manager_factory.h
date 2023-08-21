// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_NEARBY_NEARBY_PROCESS_MANAGER_FACTORY_H_
#define CHROME_BROWSER_ASH_NEARBY_NEARBY_PROCESS_MANAGER_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class Profile;

namespace ash::nearby {

class NearbyProcessManager;

// Creates a NearbyProcessManager for the primary user as well as for the OTR
// signin profile.
//
// Part of the role of the OTR signin profile is to render the OOBE UI as well
// as the sign-in screen. We need to create a NearbyProcessManager for this
// profile for use with Quick Start. It should be noted that the OTR signin
// profile continues to exist even after the user session begins, which means
// the OTR signin profile and the user profile both exist at the same time. As a
// result there will be multiple instances of the NearbyProcessManager in
// existence at the same time.
//
// TODO(b/280308935): At the end of the Quick Start flow we will release all
// process references which will trigger cleanup in NearbyProcessManager and
// NearbyDependenciesProvider. Although there will be two instances of these
// services in existence at the same time, one will be inactive.
class NearbyProcessManagerFactory : public ProfileKeyedServiceFactory {
 public:
  static NearbyProcessManager* GetForProfile(Profile* profile);

  static NearbyProcessManagerFactory* GetInstance();

  // When true is passed, this factory will create a NearbyProcessManager even
  // when it is not the primary profile.
  static void SetBypassPrimaryUserCheckForTesting(
      bool bypass_primary_user_check_for_testing);

  NearbyProcessManagerFactory(const NearbyProcessManagerFactory&) = delete;
  NearbyProcessManagerFactory& operator=(const NearbyProcessManagerFactory&) =
      delete;

 private:
  friend base::NoDestructor<NearbyProcessManagerFactory>;

  // Returns true if the nearby process can be launched for |profile|
  static bool CanBeLaunchedForProfile(Profile* profile);

  NearbyProcessManagerFactory();
  ~NearbyProcessManagerFactory() override;

  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
  bool ServiceIsCreatedWithBrowserContext() const override;
};

}  // namespace ash::nearby

#endif  // CHROME_BROWSER_ASH_NEARBY_NEARBY_PROCESS_MANAGER_FACTORY_H_
