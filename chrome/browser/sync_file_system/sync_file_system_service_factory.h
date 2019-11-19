// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_FILE_SYSTEM_SYNC_FILE_SYSTEM_SERVICE_FACTORY_H_
#define CHROME_BROWSER_SYNC_FILE_SYSTEM_SYNC_FILE_SYSTEM_SERVICE_FACTORY_H_

#include <memory>

#include "base/memory/singleton.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

class Profile;

namespace sync_file_system {

class LocalFileSyncService;
class RemoteFileSyncService;
class SyncFileSystemService;

class SyncFileSystemServiceFactory : public BrowserContextKeyedServiceFactory {
 public:
  static SyncFileSystemService* GetForProfile(Profile* profile);
  static SyncFileSystemServiceFactory* GetInstance();

  // This overrides the local/remote service for testing.
  // For testing this must be called before GetForProfile is called.
  // Otherwise a new DriveFileSyncService is created for the new service.
  // Since we use std::unique_ptr it's one-off and the instance is passed
  // to the newly created SyncFileSystemService.
  void set_mock_local_file_service(
      std::unique_ptr<LocalFileSyncService> mock_local_service);
  void set_mock_remote_file_service(
      std::unique_ptr<RemoteFileSyncService> mock_remote_service);

 private:
  friend struct base::DefaultSingletonTraits<SyncFileSystemServiceFactory>;
  SyncFileSystemServiceFactory();
  ~SyncFileSystemServiceFactory() override;

  // BrowserContextKeyedServiceFactory overrides.
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;

  mutable std::unique_ptr<LocalFileSyncService> mock_local_file_service_;
  mutable std::unique_ptr<RemoteFileSyncService> mock_remote_file_service_;
};

}  // namespace sync_file_system

#endif  // CHROME_BROWSER_SYNC_FILE_SYSTEM_SYNC_FILE_SYSTEM_SERVICE_FACTORY_H_
