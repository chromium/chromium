// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ORIGIN_GATING_ORIGIN_GATING_SERVICE_FACTORY_H_
#define CHROME_BROWSER_ORIGIN_GATING_ORIGIN_GATING_SERVICE_FACTORY_H_

#include <memory>

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class KeyedService;

namespace content {
class BrowserContext;
}  // namespace content

namespace origin_gating {

class OriginGatingService;

class OriginGatingServiceFactory : public ProfileKeyedServiceFactory {
 public:
  static OriginGatingServiceFactory* GetInstance();
  static OriginGatingService* GetForBrowserContext(
      content::BrowserContext* context);

  OriginGatingServiceFactory(const OriginGatingServiceFactory&) = delete;
  OriginGatingServiceFactory& operator=(const OriginGatingServiceFactory&) =
      delete;

 private:
  friend base::NoDestructor<OriginGatingServiceFactory>;

  OriginGatingServiceFactory();
  ~OriginGatingServiceFactory() override;

  // ProfileKeyedServiceFactory:
  bool ServiceIsCreatedWithBrowserContext() const override;
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
};

}  // namespace origin_gating

#endif  // CHROME_BROWSER_ORIGIN_GATING_ORIGIN_GATING_SERVICE_FACTORY_H_
