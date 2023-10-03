// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_SAFE_BROWSING_METRICS_COLLECTOR_FACTORY_H_
#define CHROME_BROWSER_SAFE_BROWSING_SAFE_BROWSING_METRICS_COLLECTOR_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class KeyedService;
class Profile;

namespace content {
class BrowserContext;
}

namespace safe_browsing {

class SafeBrowsingMetricsCollector;

// Singleton that owns SafeBrowsingMetricsCollector objects, one for each active
// Profile. It listens to profile destroy events and destroy its associated
// object. It returns a nullptr in the Incognito mode.
class SafeBrowsingMetricsCollectorFactory : public ProfileKeyedServiceFactory {
 public:
  // Creates the object if it doesn't exist already for the given |profile|.
  // If the object already exists, return its pointer.
  static SafeBrowsingMetricsCollector* GetForProfile(Profile* profile);

  // Get the singleton instance.
  static SafeBrowsingMetricsCollectorFactory* GetInstance();

  SafeBrowsingMetricsCollectorFactory(
      const SafeBrowsingMetricsCollectorFactory&) = delete;
  SafeBrowsingMetricsCollectorFactory& operator=(
      const SafeBrowsingMetricsCollectorFactory&) = delete;

 private:
  friend base::NoDestructor<SafeBrowsingMetricsCollectorFactory>;

  SafeBrowsingMetricsCollectorFactory();
  ~SafeBrowsingMetricsCollectorFactory() override = default;

  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
};

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_SAFE_BROWSING_METRICS_COLLECTOR_FACTORY_H_
