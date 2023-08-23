// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_AFFILIATION_SERVICE_FACTORY_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_AFFILIATION_SERVICE_FACTORY_H_

#include "chrome/browser/profiles/profile_keyed_service_factory.h"

namespace password_manager {
class AffiliationService;
}

namespace content {
class BrowserContext;
}

class Profile;

// Creates instances of AffiliationService per Profile.
class AffiliationServiceFactory : public ProfileKeyedServiceFactory {
 public:
  AffiliationServiceFactory();
  ~AffiliationServiceFactory() override;

  static AffiliationServiceFactory* GetInstance();
  static password_manager::AffiliationService* GetForProfile(Profile* profile);

 private:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
};

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_AFFILIATION_SERVICE_FACTORY_H_
