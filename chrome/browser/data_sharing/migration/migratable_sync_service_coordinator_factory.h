// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DATA_SHARING_MIGRATION_MIGRATABLE_SYNC_SERVICE_COORDINATOR_FACTORY_H_
#define CHROME_BROWSER_DATA_SHARING_MIGRATION_MIGRATABLE_SYNC_SERVICE_COORDINATOR_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

namespace content {
class BrowserContext;
}  // namespace content

namespace data_sharing {
class MigratableSyncServiceCoordinator;

// A factory to create a MigratableSyncServiceCoordinator.
class MigratableSyncServiceCoordinatorFactory
    : public ProfileKeyedServiceFactory {
 public:
  // Gets the MigratableSyncServiceCoordinator for the profile. Returns null
  // for incognito.
  static MigratableSyncServiceCoordinator* GetForProfile(Profile* profile);

  // Gets the lazy singleton instance of the
  // MigratableSyncServiceCoordinatorFactory.
  static MigratableSyncServiceCoordinatorFactory* GetInstance();

  // Disallow copy/assign.
  MigratableSyncServiceCoordinatorFactory(
      const MigratableSyncServiceCoordinatorFactory&) = delete;
  MigratableSyncServiceCoordinatorFactory& operator=(
      const MigratableSyncServiceCoordinatorFactory&) = delete;

 private:
  friend base::NoDestructor<MigratableSyncServiceCoordinatorFactory>;

  MigratableSyncServiceCoordinatorFactory();
  ~MigratableSyncServiceCoordinatorFactory() override;

  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
};

}  // namespace data_sharing

#endif  // CHROME_BROWSER_DATA_SHARING_MIGRATION_MIGRATABLE_SYNC_SERVICE_COORDINATOR_FACTORY_H_
