// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_DATA_PROTECTION_DATA_PROTECTION_URL_LOOKUP_SERVICE_H_
#define CHROME_BROWSER_ENTERPRISE_DATA_PROTECTION_DATA_PROTECTION_URL_LOOKUP_SERVICE_H_
#include <memory>
#include <string>

#include "base/containers/lru_cache.h"
#include "base/time/time.h"
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

  enum class URLVerdictCacheEvent {
    // Verdict obtained from cache.
    kCacheHit = 0,

    // Chrome made a URL scan request.
    kUrlScanRequest = 1,

    kMaxValue = kUrlScanRequest
  };

 private:
  struct Verdict {
    Verdict();
    Verdict(Verdict&&);
    ~Verdict();

    std::unique_ptr<safe_browsing::RTLookupResponse> response;
    base::Time expiry_time;
  };

  void OnRealTimeLookupComplete(
      LookupCallback callback,
      const GURL& url,
      const std::string& identifier,
      bool is_success,
      bool is_cached,
      std::unique_ptr<safe_browsing::RTLookupResponse> rt_lookup_response);

  static size_t GetVerdictCacheMaxSize();

  static bool IsVerdictExpired(const Verdict& verdict);

  // cache which maps the full URL specification string to the safe-browsing
  // verdict, and its expiry time.
  base::LRUCache<std::string, Verdict> verdict_cache_;

  base::WeakPtrFactory<DataProtectionUrlLookupService> weak_factory_{this};
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
