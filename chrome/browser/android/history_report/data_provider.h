// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_HISTORY_REPORT_DATA_PROVIDER_H_
#define CHROME_BROWSER_ANDROID_HISTORY_REPORT_DATA_PROVIDER_H_

#include <stdint.h>

#include <memory>
#include <vector>

#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/cancelable_task_tracker.h"

class Profile;

namespace history {
class HistoryService;
}

namespace bookmarks {
class BookmarkModel;
class ModelLoader;
}

namespace history_report {

class DeltaFileEntryWithData;
class DeltaFileService;
class UsageReportsBufferService;

// Provides data from History and Bookmark backends.
class DataProvider {
 public:
  DataProvider(Profile* profile,
               DeltaFileService* delta_file_service,
               bookmarks::BookmarkModel* bookmark_model);
  ~DataProvider();

  // Provides up to limit delta file entries with sequence number > last_seq_no.
  std::unique_ptr<std::vector<DeltaFileEntryWithData>> Query(
      int64_t last_seq_no,
      int32_t limit);
  void StartVisitMigrationToUsageBuffer(
      UsageReportsBufferService* buffer_service);

 private:
  void RecreateLog();

  history::HistoryService* history_service_;
  scoped_refptr<bookmarks::ModelLoader> bookmark_model_loader_;
  DeltaFileService* delta_file_service_;
  base::CancelableTaskTracker history_task_tracker_;

  DISALLOW_COPY_AND_ASSIGN(DataProvider);
};

}  // namespace history_report

#endif  // CHROME_BROWSER_ANDROID_HISTORY_REPORT_DATA_PROVIDER_H_
