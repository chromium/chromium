// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_METRICS_PROFILE_METRICS_SERVICE_FACTORY_H_
#define CHROME_BROWSER_METRICS_PROFILE_METRICS_SERVICE_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

namespace metrics {
class ProfileMetricsService;
}  // namespace metrics

// Factory for ProfileMetricsService that is used for logging per-profile
// metrics.
class ProfileMetricsServiceFactory : public ProfileKeyedServiceFactory {
 public:
  static metrics::ProfileMetricsService* GetForProfile(Profile* profile);
  static ProfileMetricsServiceFactory* GetInstance();

 private:
  friend base::NoDestructor<ProfileMetricsServiceFactory>;

  ProfileMetricsServiceFactory();
  ~ProfileMetricsServiceFactory() override;

  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
};

#endif  // CHROME_BROWSER_METRICS_PROFILE_METRICS_SERVICE_FACTORY_H_
