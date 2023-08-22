// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_AFFILIATIONS_PREFETCHER_FACTORY_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_AFFILIATIONS_PREFETCHER_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

namespace password_manager {
class AffiliationsPrefetcher;
}  // namespace password_manager

namespace content {
class BrowserContext;
}  // namespace content

class Profile;

// Creates instances of AffiliationsPrefetcher per Profile.
class AffiliationsPrefetcherFactory : public ProfileKeyedServiceFactory {
 public:
  static AffiliationsPrefetcherFactory* GetInstance();
  static password_manager::AffiliationsPrefetcher* GetForProfile(
      Profile* profile);

 private:
  friend class base::NoDestructor<AffiliationsPrefetcherFactory>;

  AffiliationsPrefetcherFactory();
  ~AffiliationsPrefetcherFactory() override;

  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
};

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_AFFILIATIONS_PREFETCHER_FACTORY_H_
