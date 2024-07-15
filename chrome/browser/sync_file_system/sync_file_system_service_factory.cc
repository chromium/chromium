// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync_file_system/sync_file_system_service_factory.h"

#include <set>
#include <utility>

#include "base/check_is_test.h"
#include "base/command_line.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/browser/sync_file_system/local/local_file_sync_service.h"
#include "chrome/browser/sync_file_system/sync_file_system_service.h"
#include "chrome/browser/sync_file_system/syncable_file_system_util.h"
#include "extensions/browser/extension_registry_factory.h"

namespace sync_file_system {

// static
SyncFileSystemService* SyncFileSystemServiceFactory::GetForProfile(
    Profile* profile) {
  return static_cast<SyncFileSystemService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
SyncFileSystemServiceFactory* SyncFileSystemServiceFactory::GetInstance() {
  static base::NoDestructor<SyncFileSystemServiceFactory> instance;
  return instance.get();
}

// static
std::unique_ptr<SyncFileSystemService>
SyncFileSystemServiceFactory::BuildWithRemoteFileSyncServiceForTest(
    content::BrowserContext* context,
    std::unique_ptr<RemoteFileSyncService> mock_remote_service) {
  CHECK_IS_TEST();
  Profile* profile = Profile::FromBrowserContext(context);
  auto service =
      base::WrapUnique(new sync_file_system::SyncFileSystemService(profile));
  service->Initialize(LocalFileSyncService::Create(profile),
                      std::move(mock_remote_service));
  return service;
}

SyncFileSystemServiceFactory::SyncFileSystemServiceFactory()
    : ProfileKeyedServiceFactory(
          "SyncFileSystemService",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOriginalOnly)
              // TODO(crbug.com/40257657): Check if this service is needed in
              // Guest mode.
              .WithGuest(ProfileSelection::kOriginalOnly)
              // TODO(crbug.com/41488885): Check if this service is needed for
              // Ash Internals.
              .WithAshInternals(ProfileSelection::kOriginalOnly)
              .Build()) {
  typedef std::set<BrowserContextKeyedServiceFactory*> FactorySet;
  FactorySet factories;
  factories.insert(extensions::ExtensionRegistryFactory::GetInstance());
  factories.insert(SyncServiceFactory::GetInstance());
  RemoteFileSyncService::AppendDependsOnFactories(&factories);
  for (auto iter = factories.begin(); iter != factories.end(); ++iter) {
    DependsOn(*iter);
  }
}

SyncFileSystemServiceFactory::~SyncFileSystemServiceFactory() = default;

std::unique_ptr<KeyedService>
SyncFileSystemServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);

  std::unique_ptr<SyncFileSystemService> service =
      std::make_unique<SyncFileSystemService>(profile);
  service->Initialize(LocalFileSyncService::Create(profile),
                      RemoteFileSyncService::CreateForBrowserContext(
                          context, service->task_logger()));
  return service;
}

}  // namespace sync_file_system
