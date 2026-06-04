// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRIVATE_INSIGHTS_PRIVATE_INSIGHTS_SERVICE_FACTORY_H_
#define CHROME_BROWSER_PRIVATE_INSIGHTS_PRIVATE_INSIGHTS_SERVICE_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class Profile;

namespace private_insights {

class PrivateInsightsService;

class PrivateInsightsServiceFactory : public ProfileKeyedServiceFactory {
 public:
  static PrivateInsightsService* GetForProfile(Profile* profile);
  static PrivateInsightsServiceFactory* GetInstance();

 private:
  friend base::NoDestructor<PrivateInsightsServiceFactory>;

  PrivateInsightsServiceFactory();
  ~PrivateInsightsServiceFactory() override;

  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
  bool ServiceIsCreatedWithBrowserContext() const override;
};

}  // namespace private_insights

#endif  // CHROME_BROWSER_PRIVATE_INSIGHTS_PRIVATE_INSIGHTS_SERVICE_FACTORY_H_
