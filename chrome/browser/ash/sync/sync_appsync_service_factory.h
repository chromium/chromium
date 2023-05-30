// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_SYNC_SYNC_APPSYNC_SERVICE_FACTORY_H_
#define CHROME_BROWSER_ASH_SYNC_SYNC_APPSYNC_SERVICE_FACTORY_H_

#include <memory>

#include "chrome/browser/profiles/profile_keyed_service_factory.h"
#include "components/keyed_service/core/keyed_service.h"

class Profile;

namespace base {
template <typename T>
class NoDestructor;
}  // namespace base

namespace content {
class BrowserContext;
}  // namespace content

namespace ash {

class SyncAppsyncService;

class SyncAppsyncServiceFactory : public ProfileKeyedServiceFactory {
 public:
  static SyncAppsyncService* GetForProfile(Profile* profile);
  static SyncAppsyncServiceFactory* GetInstance();

  SyncAppsyncServiceFactory(const SyncAppsyncServiceFactory& other) = delete;
  SyncAppsyncServiceFactory& operator=(const SyncAppsyncServiceFactory& other) =
      delete;

 private:
  friend base::NoDestructor<SyncAppsyncServiceFactory>;

  SyncAppsyncServiceFactory();
  ~SyncAppsyncServiceFactory() override;

  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
  bool ServiceIsCreatedWithBrowserContext() const override;
  bool ServiceIsNULLWhileTesting() const override;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_SYNC_SYNC_APPSYNC_SERVICE_FACTORY_H_
