// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BROWSING_DATA_CHROME_BROWSING_DATA_LIFETIME_MANAGER_FACTORY_H_
#define CHROME_BROWSER_BROWSING_DATA_CHROME_BROWSING_DATA_LIFETIME_MANAGER_FACTORY_H_

#include "chrome/browser/profiles/profile_keyed_service_factory.h"

namespace base {
template <typename T>
class NoDestructor;
}

class ChromeBrowsingDataLifetimeManager;
class Profile;

class ChromeBrowsingDataLifetimeManagerFactory
    : public ProfileKeyedServiceFactory {
 public:
  ChromeBrowsingDataLifetimeManagerFactory(
      const ChromeBrowsingDataLifetimeManagerFactory&) = delete;
  ChromeBrowsingDataLifetimeManagerFactory& operator=(
      const ChromeBrowsingDataLifetimeManagerFactory&) = delete;

  // Returns the singleton instance of ChromeBrowsingDataLifetimeManagerFactory.
  static ChromeBrowsingDataLifetimeManagerFactory* GetInstance();

  // Returns the ChromeBrowsingDataLifetimeManager associated with |profile|.
  static ChromeBrowsingDataLifetimeManager* GetForProfile(Profile* profile);

 private:
  friend base::NoDestructor<ChromeBrowsingDataLifetimeManagerFactory>;

  ChromeBrowsingDataLifetimeManagerFactory();
  ~ChromeBrowsingDataLifetimeManagerFactory() override;

  // BrowserContextKeyedServiceFactory overrides:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
  bool ServiceIsCreatedWithBrowserContext() const override;
};

#endif  // CHROME_BROWSER_BROWSING_DATA_CHROME_BROWSING_DATA_LIFETIME_MANAGER_FACTORY_H_
