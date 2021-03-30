// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_DOWNLOAD_PROTECTION_DOWNLOAD_REPORTER_H_
#define CHROME_BROWSER_SAFE_BROWSING_DOWNLOAD_PROTECTION_DOWNLOAD_REPORTER_H_

#include "base/macros.h"
#include "base/scoped_observer.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager_observer.h"
#include "chrome/browser/profiles/profile_observer.h"
#include "components/download/public/common/download_item.h"
#include "components/download/public/common/simple_download_manager_coordinator.h"

namespace safe_browsing {

// This class is responsible for observing download events and reporting them as
// appropriate.
class DownloadReporter
    : public download::DownloadItem::Observer,
      public download::SimpleDownloadManagerCoordinator::Observer,
      public ProfileManagerObserver,
      public ProfileObserver {
 public:
  DownloadReporter();
  ~DownloadReporter() override;

  // ProfileManagerObserver:
  void OnProfileAdded(Profile* profile) override;

  // ProfileObserver:
  void OnOffTheRecordProfileCreated(Profile* off_the_record) override;
  void OnProfileWillBeDestroyed(Profile* profile) override;

  // SimpleDownloadManagerCoordinator::Observer:
  void OnManagerGoingDown(
      download::SimpleDownloadManagerCoordinator* coordinator) override;
  void OnDownloadCreated(download::DownloadItem* download) override;

  // DownloadItem::Observer:
  void OnDownloadDestroyed(download::DownloadItem* download) override;
  void OnDownloadUpdated(download::DownloadItem* download) override;

 private:
  base::flat_map<download::DownloadItem*, download::DownloadDangerType>
      danger_types_;
  ScopedObserver<Profile, ProfileObserver> observed_profiles_{this};
  ScopedObserver<download::SimpleDownloadManagerCoordinator,
                 download::SimpleDownloadManagerCoordinator::Observer>
      observed_coordinators_{this};
  ScopedObserver<download::DownloadItem, download::DownloadItem::Observer>
      observed_downloads_{this};

  void AddBypassEventToPref(download::DownloadItem* download);

  DISALLOW_COPY_AND_ASSIGN(DownloadReporter);
};

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_DOWNLOAD_PROTECTION_DOWNLOAD_REPORTER_H_
