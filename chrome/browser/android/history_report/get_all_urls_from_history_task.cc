// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/history_report/get_all_urls_from_history_task.h"

#include "base/functional/bind.h"
#include "components/history/core/browser/history_backend.h"
#include "components/history/core/browser/history_database.h"
#include "components/history/core/browser/history_types.h"
#include "components/history/core/browser/url_database.h"
#include "components/history/core/browser/url_row.h"
#include "content/public/browser/browser_thread.h"

namespace history_report {

GetAllUrlsFromHistoryTask::GetAllUrlsFromHistoryTask(
    base::WaitableEvent* wait_event,
    std::vector<std::string>* urls)
    : urls_(urls),
      wait_event_(wait_event) {
}

bool GetAllUrlsFromHistoryTask::RunOnDBThread(
    history::HistoryBackend* backend,
    history::HistoryDatabase* db) {

  history::URLDatabase::URLEnumerator it;
  db->InitURLEnumeratorForEverything(&it);

  history::URLRow row;
  while (it.GetNextURL(&row)) {
    if (row.url().is_valid()) {
      urls_->push_back(row.url().spec());
    }
  }

  wait_event_->Signal();
  return true;
}

}  // namespace history_report
