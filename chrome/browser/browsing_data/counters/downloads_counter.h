// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BROWSING_DATA_COUNTERS_DOWNLOADS_COUNTER_H_
#define CHROME_BROWSER_BROWSING_DATA_COUNTERS_DOWNLOADS_COUNTER_H_

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "components/browsing_data/core/counters/browsing_data_counter.h"
#include "content/public/browser/download_manager.h"

class Profile;

// A BrowsingDataCounter that counts the number of downloads as seen on the
// chrome://downloads page.
class DownloadsCounter : public browsing_data::BrowsingDataCounter,
                         public content::DownloadManager::Observer {
 public:
  explicit DownloadsCounter(Profile* profile);

  DownloadsCounter(const DownloadsCounter&) = delete;
  DownloadsCounter& operator=(const DownloadsCounter&) = delete;

  ~DownloadsCounter() override;

  const char* GetPrefName() const override;

 private:
  // BrowsingDataRemover implementation.
  void Count() override;

  // content::DownloadManager::Observer implementation.
  void OnManagerInitialized() override;
  void ManagerGoingDown(content::DownloadManager* manager) override;

  // Helper to perform the count of download items and report results.
  void CountDownloads();

  raw_ptr<Profile> profile_;

  base::ScopedObservation<content::DownloadManager,
                          content::DownloadManager::Observer>
      download_manager_observation_{this};
};

#endif  // CHROME_BROWSER_BROWSING_DATA_COUNTERS_DOWNLOADS_COUNTER_H_
