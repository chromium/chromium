// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_SYNC_SYNC_APPSYNC_SERVICE_FACTORY_H_
#define CHROME_BROWSER_ASH_SYNC_SYNC_APPSYNC_SERVICE_FACTORY_H_

#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class Profile;

namespace base {
template <typename T>
struct DefaultSingletonTraits;
}  // namespace base

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
  friend struct base::DefaultSingletonTraits<SyncAppsyncServiceFactory>;

  SyncAppsyncServiceFactory();
  ~SyncAppsyncServiceFactory() override;

  // BrowserContextKeyedServiceFactory implementation.
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_SYNC_SYNC_APPSYNC_SERVICE_FACTORY_H_
