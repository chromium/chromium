// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DINO_DINO_BOOKMARK_METRICS_SERVICE_FACTORY_H_
#define CHROME_BROWSER_DINO_DINO_BOOKMARK_METRICS_SERVICE_FACTORY_H_

#include <memory>

#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class Profile;

namespace base {
template <typename T>
class NoDestructor;
}  // namespace base

namespace dino {

class DinoBookmarkMetricsService;

// Singleton factory that owns and associates `DinoBookmarkMetricsService`
// instances with regular `Profile`s.
class DinoBookmarkMetricsServiceFactory : public ProfileKeyedServiceFactory {
 public:
  static DinoBookmarkMetricsService* GetForProfile(Profile* profile);
  static DinoBookmarkMetricsServiceFactory* GetInstance();
  static TestingFactory GetDefaultFactory();

  DinoBookmarkMetricsServiceFactory(const DinoBookmarkMetricsServiceFactory&) =
      delete;
  DinoBookmarkMetricsServiceFactory& operator=(
      const DinoBookmarkMetricsServiceFactory&) = delete;

 private:
  friend base::NoDestructor<DinoBookmarkMetricsServiceFactory>;

  DinoBookmarkMetricsServiceFactory();
  ~DinoBookmarkMetricsServiceFactory() override;

  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
  bool ServiceIsCreatedWithBrowserContext() const override;
  bool ServiceIsNULLWhileTesting() const override;
};

}  // namespace dino

#endif  // CHROME_BROWSER_DINO_DINO_BOOKMARK_METRICS_SERVICE_FACTORY_H_
