// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_DATA_PROTECTION_DATA_PROTECTION_URL_LOOKUP_SERVICE_H_
#define CHROME_BROWSER_ENTERPRISE_DATA_PROTECTION_DATA_PROTECTION_URL_LOOKUP_SERVICE_H_
#include <memory>
#include <string>

#include "chrome/browser/profiles/profile_keyed_service_factory.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/safe_browsing/core/browser/realtime/url_lookup_service_base.h"

namespace content {
class BrowserContext;
class WebContents;
}  // namespace content

namespace enterprise_data_protection {

using LookupCallback =
    base::OnceCallback<void(std::unique_ptr<safe_browsing::RTLookupResponse>)>;

// Service that contains a URL lookup verdict cache and scopes it to a profile
// TODO(447624248): consider combining with
// `ChromeEnterpriseRealTimeUrlLookupService`
class DataProtectionUrlLookupService : public KeyedService {
 public:
  DataProtectionUrlLookupService();
  ~DataProtectionUrlLookupService() override;

  void DoLookup(safe_browsing::RealTimeUrlLookupServiceBase* lookup_service,
                const GURL& url,
                const std::string& identifier,
                LookupCallback callback,
                content::WebContents* web_contents);
};

// =====================================
// DataProtectionUrlLookupServiceFactory
// =====================================

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

#endif  // CHROME_BROWSER_ENTERPRISE_DATA_PROTECTION_DATA_PROTECTION_URL_LOOKUP_SERVICE_H_
