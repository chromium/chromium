// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browsing_data/counters/downloads_counter.h"

#include "base/memory/raw_ptr.h"
#include "base/ranges/algorithm.h"
#include "chrome/browser/download/download_history.h"
#include "chrome/browser/profiles/profile.h"
#include "components/browsing_data/core/pref_names.h"
#include "content/public/browser/download_manager.h"

DownloadsCounter::DownloadsCounter(Profile* profile)
    : profile_(profile) {}

DownloadsCounter::~DownloadsCounter() {
}

const char* DownloadsCounter::GetPrefName() const {
  return browsing_data::prefs::kDeleteDownloadHistory;
}

void DownloadsCounter::Count() {
  content::DownloadManager* download_manager = profile_->GetDownloadManager();
  std::vector<raw_ptr<download::DownloadItem, VectorExperimental>> downloads;
  download_manager->GetAllDownloads(&downloads);
  base::Time begin_time = GetPeriodStart();

  ReportResult(base::ranges::count_if(
      downloads, [begin_time](const download::DownloadItem* item) {
        return item->GetStartTime() >= begin_time &&
               DownloadHistory::IsPersisted(item);
      }));
}
