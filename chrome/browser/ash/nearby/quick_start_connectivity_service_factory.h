// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_NEARBY_QUICK_START_CONNECTIVITY_SERVICE_FACTORY_H_
#define CHROME_BROWSER_ASH_NEARBY_QUICK_START_CONNECTIVITY_SERVICE_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/ash/nearby/quick_start_connectivity_service.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

namespace ash::quick_start {

class QuickStartConnectivityServiceFactory : public ProfileKeyedServiceFactory {
 public:
  static QuickStartConnectivityService* GetForProfile(Profile* profile);

  static QuickStartConnectivityServiceFactory* GetInstance();

  QuickStartConnectivityServiceFactory(
      const QuickStartConnectivityServiceFactory&) = delete;
  QuickStartConnectivityServiceFactory& operator=(
      const QuickStartConnectivityServiceFactory&) = delete;
  ~QuickStartConnectivityServiceFactory() override;

 private:
  friend base::NoDestructor<QuickStartConnectivityServiceFactory>;

  QuickStartConnectivityServiceFactory();

  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
  bool ServiceIsCreatedWithBrowserContext() const override;
};

}  // namespace ash::quick_start

#endif  // CHROME_BROWSER_ASH_NEARBY_QUICK_START_CONNECTIVITY_SERVICE_FACTORY_H_
