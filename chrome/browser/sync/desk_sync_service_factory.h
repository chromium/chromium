// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_DESK_SYNC_SERVICE_FACTORY_H_
#define CHROME_BROWSER_SYNC_DESK_SYNC_SERVICE_FACTORY_H_

#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class Profile;

namespace base {
template <typename T>
class NoDestructor;
}  // namespace base

namespace desks_storage {
class DeskSyncService;
}  // namespace desks_storage

// A factory to create DeskSyncService for a given browser context.
class DeskSyncServiceFactory : public ProfileKeyedServiceFactory {
 public:
  static desks_storage::DeskSyncService* GetForProfile(Profile* profile);
  static DeskSyncServiceFactory* GetInstance();

 private:
  friend base::NoDestructor<DeskSyncServiceFactory>;

  DeskSyncServiceFactory();
  DeskSyncServiceFactory(const DeskSyncServiceFactory&) = delete;
  DeskSyncServiceFactory& operator=(const DeskSyncServiceFactory&) = delete;
  ~DeskSyncServiceFactory() override = default;

  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
};

#endif  // CHROME_BROWSER_SYNC_DESK_SYNC_SERVICE_FACTORY_H_
