// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browsing_data/counters/downloads_counter.h"

#include <algorithm>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/download/download_history.h"
#include "chrome/browser/profiles/profile.h"
#include "components/browsing_data/core/pref_names.h"
#include "content/public/browser/download_manager.h"

DownloadsCounter::DownloadsCounter(Profile* profile)
    : profile_(profile) {}

DownloadsCounter::~DownloadsCounter() = default;

const char* DownloadsCounter::GetPrefName() const {
  return browsing_data::prefs::kDeleteDownloadHistory;
}

void DownloadsCounter::Count() {
  download_manager_observation_.Reset();

  content::DownloadManager* download_manager = profile_->GetDownloadManager();
  if (download_manager) {
    if (download_manager->IsManagerInitialized()) {
      CountDownloads();
    } else {
      download_manager_observation_.Observe(download_manager);
    }
  } else {
    ReportResult(0);
  }
}

void DownloadsCounter::OnManagerInitialized() {
  download_manager_observation_.Reset();
  CountDownloads();
}

void DownloadsCounter::ManagerGoingDown(content::DownloadManager* manager) {
  download_manager_observation_.Reset();
}

void DownloadsCounter::CountDownloads() {
  content::DownloadManager* download_manager = profile_->GetDownloadManager();
  if (!download_manager) {
    ReportResult(0);
    return;
  }
  std::vector<raw_ptr<download::DownloadItem, VectorExperimental>> downloads;
  download_manager->GetAllDownloads(&downloads);
  base::Time begin_time = GetPeriodStart();

  ReportResult(std::ranges::count_if(
      downloads, [begin_time](const download::DownloadItem* item) {
        return item->GetStartTime() >= begin_time &&
               DownloadHistory::IsPersisted(item);
      }));
}
