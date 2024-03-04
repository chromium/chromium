// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LACROS_CLOUD_FILE_SYSTEM_PATH_CACHE_H_
#define CHROME_BROWSER_LACROS_CLOUD_FILE_SYSTEM_PATH_CACHE_H_

#include "chromeos/crosapi/mojom/drive_integration_service.mojom.h"
#include "chromeos/crosapi/mojom/one_drive_integration_service.mojom.h"
#include "mojo/public/cpp/bindings/receiver.h"

// This instance connects to ash-chrome, listens to Google Drive and Microsoft
// OneDrive mount points paths changes (including changes in drive
// availability), and caches the info for later synchronous reads using
// `GetDefaultPaths()`.
class CloudFileSystemPathCache
    : public crosapi::mojom::DriveIntegrationServiceObserver,
      public crosapi::mojom::OneDriveMountObserver {
 public:
  CloudFileSystemPathCache();
  CloudFileSystemPathCache(const CloudFileSystemPathCache&) = delete;
  CloudFileSystemPathCache& operator=(const CloudFileSystemPathCache&) = delete;
  ~CloudFileSystemPathCache() override;

  // Start observing drive availability changes in ash-chrome.
  // This is a post-construction step to decouple from LacrosService.
  void Start();

 private:
  // crosapi::mojom::DriveIntegrationServiceObserver:
  void OnMountPointPathChanged(const base::FilePath& drivefs) override;

  // crosapi::mojom::OneDriveMountObserver:
  void OnOneDriveMountPointPathChanged(const base::FilePath& drivefs) override;

  // Receives mojo messages from ash-chrome (under Streaming mode) for Google
  // Drive mount changes.
  mojo::Receiver<crosapi::mojom::DriveIntegrationServiceObserver>
      drivefs_receiver_{this};

  // Receives mojo messages from ash-chrome (under Streaming mode) for OneDrive
  // mount changes.
  mojo::Receiver<crosapi::mojom::OneDriveMountObserver> onedrive_receiver_{
      this};
};

#endif  // CHROME_BROWSER_LACROS_CLOUD_FILE_SYSTEM_PATH_CACHE_H_
