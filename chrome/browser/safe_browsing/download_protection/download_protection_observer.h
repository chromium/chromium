// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_DOWNLOAD_PROTECTION_DOWNLOAD_PROTECTION_OBSERVER_H_
#define CHROME_BROWSER_SAFE_BROWSING_DOWNLOAD_PROTECTION_DOWNLOAD_PROTECTION_OBSERVER_H_

#include "base/scoped_multi_source_observation.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager_observer.h"
#include "chrome/browser/profiles/profile_observer.h"
#include "components/download/public/common/download_item.h"
#include "components/download/public/common/simple_download_manager_coordinator.h"

namespace safe_browsing {

// This class is responsible for observing download events and reporting them as
// appropriate. For save package scans, this also runs the correct callback when
// the user bypasses a scan.
class DownloadProtectionObserver
    : public download::DownloadItem::Observer,
      public download::SimpleDownloadManagerCoordinator::Observer,
      public ProfileManagerObserver,
      public ProfileObserver {
 public:
  DownloadProtectionObserver();

  DownloadProtectionObserver(const DownloadProtectionObserver&) = delete;
  DownloadProtectionObserver& operator=(const DownloadProtectionObserver&) =
      delete;

  ~DownloadProtectionObserver() override;

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
  void OnDownloadRemoved(download::DownloadItem* download) override;

  // Reports the bypass event for this download because it was opened before the
  // verdict was available. |danger_type| is the danger type returned by the
  // async scan.
  void ReportDelayedBypassEvent(download::DownloadItem* download,
                                download::DownloadDangerType danger_type);

 private:
  base::flat_map<download::DownloadItem*, download::DownloadDangerType>
      danger_types_;
  base::ScopedMultiSourceObservation<Profile, ProfileObserver>
      observed_profiles_{this};
  base::ScopedMultiSourceObservation<
      download::SimpleDownloadManagerCoordinator,
      download::SimpleDownloadManagerCoordinator::Observer>
      observed_coordinators_{this};
  base::ScopedMultiSourceObservation<download::DownloadItem,
                                     download::DownloadItem::Observer>
      observed_downloads_{this};

  void AddBypassEventToPref(download::DownloadItem* download);
  void ReportAndRecordDangerousDownloadWarningBypassed(
      download::DownloadItem* download,
      download::DownloadDangerType danger_type);
};

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_DOWNLOAD_PROTECTION_DOWNLOAD_PROTECTION_OBSERVER_H_
