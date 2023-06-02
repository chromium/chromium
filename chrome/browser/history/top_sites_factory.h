// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_HISTORY_TOP_SITES_FACTORY_H_
#define CHROME_BROWSER_HISTORY_TOP_SITES_FACTORY_H_

#include <vector>

#include "base/memory/ref_counted.h"
#include "chrome/browser/profiles/refcounted_profile_keyed_service_factory.h"

class Profile;

namespace base {
template <typename T>
class NoDestructor;
}

namespace history {
struct PrepopulatedPage;
class TopSites;
}

class Profile;

// Used for creating and fetching a per-profile instance of the TopSites.
class TopSitesFactory : public RefcountedProfileKeyedServiceFactory {
 public:
  // Get the TopSites service for |profile|, creating one if needed.
  static scoped_refptr<history::TopSites> GetForProfile(Profile* profile);

  // Get the singleton instance of the factory.
  static TopSitesFactory* GetInstance();

  // Creates a TopSites service for |context| with |prepopulated_page_list|.
  // Public for testing.
  static scoped_refptr<history::TopSites> BuildTopSites(
      content::BrowserContext* context,
      const std::vector<history::PrepopulatedPage>& prepopulated_page_list);

  TopSitesFactory(const TopSitesFactory&) = delete;
  TopSitesFactory& operator=(const TopSitesFactory&) = delete;

 private:
  friend base::NoDestructor<TopSitesFactory>;

  TopSitesFactory();
  ~TopSitesFactory() override;

  // Overridden from BrowserContextKeyedServiceFactory.
  scoped_refptr<RefcountedKeyedService> BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
  void RegisterProfilePrefs(
      user_prefs::PrefRegistrySyncable* registry) override;
  bool ServiceIsNULLWhileTesting() const override;
};

#endif  // CHROME_BROWSER_HISTORY_TOP_SITES_FACTORY_H_
