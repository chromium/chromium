// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LACROS_DRIVEFS_CACHE_H_
#define CHROME_BROWSER_LACROS_DRIVEFS_CACHE_H_

#include "chromeos/crosapi/mojom/drive_integration_service.mojom.h"
#include "mojo/public/cpp/bindings/receiver.h"

// This instance connects to ash-chrome, listens to drive mount point path
// changes (including changes in drive availability), and caches the info for
// later synchronous reads using `GetDefaultPaths()`.
class DriveFsCache : public crosapi::mojom::DriveIntegrationServiceObserver {
 public:
  DriveFsCache();
  DriveFsCache(const DriveFsCache&) = delete;
  DriveFsCache& operator=(const DriveFsCache&) = delete;
  ~DriveFsCache() override;

  // Start observing drive availability changes in ash-chrome.
  // This is a post-construction step to decouple from LacrosService.
  void Start();

 private:
  // crosapi::mojom::DriveIntegrationServiceObserver:
  void OnMountPointPathChanged(const base::FilePath& drivefs) override;

  // Receives mojo messages from ash-chromem (under Streaming mode).
  mojo::Receiver<crosapi::mojom::DriveIntegrationServiceObserver> receiver_{
      this};
};

#endif  // CHROME_BROWSER_LACROS_DRIVEFS_CACHE_H_
