// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_DATA_PROTECTION_DATA_PROTECTION_URL_LOOKUP_SERVICE_FACTORY_H_
#define CHROME_BROWSER_ENTERPRISE_DATA_PROTECTION_DATA_PROTECTION_URL_LOOKUP_SERVICE_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

namespace content {
class BrowserContext;
}

namespace enterprise_data_protection {

class DataProtectionUrlLookupService;

class DataProtectionUrlLookupServiceFactory
    : public ProfileKeyedServiceFactory {
 public:
  static DataProtectionUrlLookupServiceFactory* GetInstance();
  static DataProtectionUrlLookupService* GetForBrowserContext(
      content::BrowserContext* context);

 private:
  DataProtectionUrlLookupServiceFactory();
  ~DataProtectionUrlLookupServiceFactory() override;
  friend base::NoDestructor<DataProtectionUrlLookupServiceFactory>;

  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
};

}  // namespace enterprise_data_protection

#endif  // CHROME_BROWSER_ENTERPRISE_DATA_PROTECTION_DATA_PROTECTION_URL_LOOKUP_SERVICE_FACTORY_H_
