// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_HISTORY_REPORT_GET_ALL_URLS_FROM_HISTORY_TASK_H_
#define CHROME_BROWSER_ANDROID_HISTORY_REPORT_GET_ALL_URLS_FROM_HISTORY_TASK_H_

#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/synchronization/waitable_event.h"
#include "components/history/core/browser/history_db_task.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/browser/history_types.h"

namespace history_report {

class GetAllUrlsFromHistoryTask : public history::HistoryDBTask {
 public:
  GetAllUrlsFromHistoryTask(base::WaitableEvent* wait_event,
                            std::vector<std::string>* urls);

  GetAllUrlsFromHistoryTask(const GetAllUrlsFromHistoryTask&) = delete;
  GetAllUrlsFromHistoryTask& operator=(const GetAllUrlsFromHistoryTask&) =
      delete;

  bool RunOnDBThread(history::HistoryBackend* backend,
                     history::HistoryDatabase* db) override;
  void DoneRunOnMainThread() override {}

 protected:
  ~GetAllUrlsFromHistoryTask() override {}

 private:
  raw_ptr<std::vector<std::string>> urls_;
  raw_ptr<base::WaitableEvent> wait_event_;
};

}  // namespace history_report

#endif  // CHROME_BROWSER_ANDROID_HISTORY_REPORT_GET_ALL_URLS_FROM_HISTORY_TASK_H_
