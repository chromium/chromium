// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_BULK_LEAK_CHECK_SERVICE_FACTORY_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_BULK_LEAK_CHECK_SERVICE_FACTORY_H_

#include "chrome/browser/profiles/profile_keyed_service_factory.h"

namespace password_manager {
class BulkLeakCheckServiceInterface;
}

class Profile;

// Creates instances of BulkLeakCheckService per Profile.
class BulkLeakCheckServiceFactory : public ProfileKeyedServiceFactory {
 public:
  BulkLeakCheckServiceFactory();
  ~BulkLeakCheckServiceFactory() override;

  static BulkLeakCheckServiceFactory* GetInstance();
  static password_manager::BulkLeakCheckServiceInterface* GetForProfile(
      Profile* profile);

 private:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
};

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_BULK_LEAK_CHECK_SERVICE_FACTORY_H_
