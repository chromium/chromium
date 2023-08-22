// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_BROWSER_MANAGEMENT_MANAGEMENT_SERVICE_FACTORY_H_
#define CHROME_BROWSER_ENTERPRISE_BROWSER_MANAGEMENT_MANAGEMENT_SERVICE_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"
#include "components/policy/core/common/management/platform_management_service.h"

class Profile;

namespace policy {

class ManagementService;

class ManagementServiceFactory : public ProfileKeyedServiceFactory {
 public:
  ManagementServiceFactory(const ManagementServiceFactory&) = delete;
  ManagementServiceFactory& operator=(const ManagementServiceFactory&) = delete;

  // Returns the singleton instance of ManagementServiceFactory.
  static ManagementServiceFactory* GetInstance();

  // Returns the ManagementService associated with |profile|.
  static ManagementService* GetForProfile(Profile* profile);

  static ManagementService* GetForPlatform();

 private:
  friend class base::NoDestructor<ManagementServiceFactory>;

  ManagementServiceFactory();
  ~ManagementServiceFactory() override;

  // BrowserContextKeyedServiceFactory overrides:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
};

}  // namespace policy

#endif  // CHROME_BROWSER_ENTERPRISE_BROWSER_MANAGEMENT_MANAGEMENT_SERVICE_FACTORY_H_
