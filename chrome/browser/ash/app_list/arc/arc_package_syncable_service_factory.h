// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_APP_LIST_ARC_ARC_PACKAGE_SYNCABLE_SERVICE_FACTORY_H_
#define CHROME_BROWSER_ASH_APP_LIST_ARC_ARC_PACKAGE_SYNCABLE_SERVICE_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

namespace arc {

class ArcPackageSyncableService;

class ArcPackageSyncableServiceFactory : public ProfileKeyedServiceFactory {
 public:
  ArcPackageSyncableServiceFactory(const ArcPackageSyncableServiceFactory&) =
      delete;
  ArcPackageSyncableServiceFactory& operator=(
      const ArcPackageSyncableServiceFactory&) = delete;

  static ArcPackageSyncableService* GetForBrowserContext(
      content::BrowserContext* context);

  static ArcPackageSyncableServiceFactory* GetInstance();

 private:
  friend base::NoDestructor<ArcPackageSyncableServiceFactory>;

  ArcPackageSyncableServiceFactory();
  ~ArcPackageSyncableServiceFactory() override;

  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
};

}  // namespace arc

#endif  // CHROME_BROWSER_ASH_APP_LIST_ARC_ARC_PACKAGE_SYNCABLE_SERVICE_FACTORY_H_
