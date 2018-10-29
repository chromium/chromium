// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync_file_system/sync_file_system_service_factory.h"

#include <set>
#include <utility>

#include "base/command_line.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync_file_system/local/local_file_sync_service.h"
#include "chrome/browser/sync_file_system/sync_file_system_service.h"
#include "chrome/browser/sync_file_system/syncable_file_system_util.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
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
  return base::Singleton<SyncFileSystemServiceFactory>::get();
}

void SyncFileSystemServiceFactory::set_mock_local_file_service(
    std::unique_ptr<LocalFileSyncService> mock_local_service) {
  mock_local_file_service_ = std::move(mock_local_service);
}

void SyncFileSystemServiceFactory::set_mock_remote_file_service(
    std::unique_ptr<RemoteFileSyncService> mock_remote_service) {
  mock_remote_file_service_ = std::move(mock_remote_service);
}

SyncFileSystemServiceFactory::SyncFileSystemServiceFactory()
    : BrowserContextKeyedServiceFactory(
        "SyncFileSystemService",
        BrowserContextDependencyManager::GetInstance()) {
  typedef std::set<BrowserContextKeyedServiceFactory*> FactorySet;
  FactorySet factories;
  factories.insert(extensions::ExtensionRegistryFactory::GetInstance());
  RemoteFileSyncService::AppendDependsOnFactories(&factories);
  for (auto iter = factories.begin(); iter != factories.end(); ++iter) {
    DependsOn(*iter);
  }
}

SyncFileSystemServiceFactory::~SyncFileSystemServiceFactory() {}

KeyedService* SyncFileSystemServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);

  SyncFileSystemService* service = new SyncFileSystemService(profile);

  std::unique_ptr<LocalFileSyncService> local_file_service;
  if (mock_local_file_service_)
    local_file_service = std::move(mock_local_file_service_);
  else
    local_file_service = LocalFileSyncService::Create(profile);

  std::unique_ptr<RemoteFileSyncService> remote_file_service;
  if (mock_remote_file_service_) {
    remote_file_service = std::move(mock_remote_file_service_);
  } else {
    remote_file_service = RemoteFileSyncService::CreateForBrowserContext(
        context, service->task_logger());
  }

  service->Initialize(std::move(local_file_service),
                      std::move(remote_file_service));
  return service;
}

}  // namespace sync_file_system
