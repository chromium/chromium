// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SIGNIN_IDENTITY_MANAGER_FACTORY_H_
#define CHROME_BROWSER_SIGNIN_IDENTITY_MANAGER_FACTORY_H_

#include "base/memory/singleton.h"
#include "base/observer_list.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

namespace signin {
class IdentityManager;
}

class Profile;

// Singleton that owns all IdentityManager instances and associates them with
// Profiles.
class IdentityManagerFactory : public ProfileKeyedServiceFactory {
 public:
  class Observer : public base::CheckedObserver {
   public:
    // Called when a IdentityManager instance is created.
    virtual void IdentityManagerCreated(
        signin::IdentityManager* identity_manager) {}

   protected:
    ~Observer() override {}
  };

  static signin::IdentityManager* GetForProfile(Profile* profile);
  static signin::IdentityManager* GetForProfileIfExists(const Profile* profile);

  // Returns an instance of the IdentityManagerFactory singleton.
  static IdentityManagerFactory* GetInstance();

  IdentityManagerFactory(const IdentityManagerFactory&) = delete;
  IdentityManagerFactory& operator=(const IdentityManagerFactory&) = delete;

  // Ensures that IdentityManagerFactory and the factories on which it depends
  // are built.
  static void EnsureFactoryAndDependeeFactoriesBuilt();

  // Methods to register or remove observers of IdentityManager
  // creation/shutdown.
  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

 private:
  friend struct base::DefaultSingletonTraits<IdentityManagerFactory>;

  IdentityManagerFactory();
  ~IdentityManagerFactory() override;

  // BrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* profile) const override;
  void RegisterProfilePrefs(
      user_prefs::PrefRegistrySyncable* registry) override;

  // List of observers. Checks that list is empty on destruction.
  base::ObserverList<Observer, /*check_empty=*/true, /*allow_reentrancy=*/false>
      observer_list_;
};

#endif  // CHROME_BROWSER_SIGNIN_IDENTITY_MANAGER_FACTORY_H_
