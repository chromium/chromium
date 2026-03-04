// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/data_protection/data_protection_url_lookup_service_factory.h"

#include "chrome/browser/profiles/profile.h"
#include "components/enterprise/data_protection/data_protection_url_lookup_service.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"

namespace enterprise_data_protection {

// static
DataProtectionUrlLookupServiceFactory*
DataProtectionUrlLookupServiceFactory::GetInstance() {
  static base::NoDestructor<DataProtectionUrlLookupServiceFactory> instance;
  return instance.get();
}

// static
DataProtectionUrlLookupService*
DataProtectionUrlLookupServiceFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<DataProtectionUrlLookupService*>(
      GetInstance()->GetServiceForBrowserContext(context, true));
}

DataProtectionUrlLookupServiceFactory::DataProtectionUrlLookupServiceFactory()
    : ProfileKeyedServiceFactory("DataProtectionUrlLookupService",
                                 ProfileSelections::BuildForRegularProfile()) {}

DataProtectionUrlLookupServiceFactory::
    ~DataProtectionUrlLookupServiceFactory() = default;

std::unique_ptr<KeyedService>
DataProtectionUrlLookupServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  return std::make_unique<DataProtectionUrlLookupService>();
}

}  // namespace enterprise_data_protection
