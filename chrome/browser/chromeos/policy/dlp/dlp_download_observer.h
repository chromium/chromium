// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_POLICY_DLP_DLP_DOWNLOAD_OBSERVER_H_
#define CHROME_BROWSER_CHROMEOS_POLICY_DLP_DLP_DOWNLOAD_OBSERVER_H_

#include "chrome/browser/profiles/profile.h"
#include "components/download/public/common/download_item.h"
#include "components/download/public/common/simple_download_manager_coordinator.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/keyed_service/core/simple_factory_key.h"

namespace policy {

// Class to inform the dlp stack about downloaded files.
class DlpDownloadObserver
    : public KeyedService,
      public download::SimpleDownloadManagerCoordinator::Observer,
      public download::DownloadItem::Observer {
 public:
  explicit DlpDownloadObserver(SimpleFactoryKey* key);
  ~DlpDownloadObserver() override;

  // KeyedService:
  void Shutdown() override;

  // SimpleDownloadManagerCoordinator::Observer:
  void OnDownloadCreated(download::DownloadItem* item) override;

  // DownloadItem::Observer:
  void OnDownloadUpdated(download::DownloadItem* item) override;

 private:
  raw_ptr<SimpleFactoryKey> key_;
};
}  // namespace policy

#endif  // CHROME_BROWSER_CHROMEOS_POLICY_DLP_DLP_DOWNLOAD_OBSERVER_H_
