// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_HTTPS_ENGAGEMENT_METRICS_HTTPS_ENGAGEMENT_SERVICE_FACTORY_H_
#define CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_HTTPS_ENGAGEMENT_METRICS_HTTPS_ENGAGEMENT_SERVICE_FACTORY_H_

#include "chrome/browser/profiles/profile_keyed_service_factory.h"

namespace content {
class BrowserContext;
}  // namespace content

namespace base {
template <typename T>
class NoDestructor;
}

class HttpsEngagementService;

// Singleton that owns all HttpsEngagementKeyServices and associates them with
// BrowserContexts.
class HttpsEngagementServiceFactory : public ProfileKeyedServiceFactory {
 public:
  static HttpsEngagementService* GetForBrowserContext(
      content::BrowserContext* context);
  static HttpsEngagementServiceFactory* GetInstance();

  HttpsEngagementServiceFactory(const HttpsEngagementServiceFactory&) = delete;
  HttpsEngagementServiceFactory& operator=(
      const HttpsEngagementServiceFactory&) = delete;

 private:
  friend base::NoDestructor<HttpsEngagementServiceFactory>;

  HttpsEngagementServiceFactory();
  ~HttpsEngagementServiceFactory() override;

  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
  bool ServiceIsCreatedWithBrowserContext() const override;
};

#endif  // CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_HTTPS_ENGAGEMENT_METRICS_HTTPS_ENGAGEMENT_SERVICE_FACTORY_H_
