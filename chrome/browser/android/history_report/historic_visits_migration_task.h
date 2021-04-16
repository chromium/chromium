// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_HISTORY_REPORT_HISTORIC_VISITS_MIGRATION_TASK_H_
#define CHROME_BROWSER_ANDROID_HISTORY_REPORT_HISTORIC_VISITS_MIGRATION_TASK_H_

#include "base/macros.h"
#include "base/synchronization/waitable_event.h"
#include "components/history/core/browser/history_db_task.h"

namespace history_report {
class UsageReportsBufferService;

class HistoricVisitsMigrationTask : public history::HistoryDBTask {
 public:
  HistoricVisitsMigrationTask(base::WaitableEvent* event,
                              UsageReportsBufferService* report_buffer_service);

  bool RunOnDBThread(history::HistoryBackend* backend,
                             history::HistoryDatabase* db) override;
  void DoneRunOnMainThread() override {}

 private:
  ~HistoricVisitsMigrationTask() override {}

  base::WaitableEvent* wait_event_;
  UsageReportsBufferService* usage_reports_buffer_service_;

  DISALLOW_COPY_AND_ASSIGN(HistoricVisitsMigrationTask);
};

}  // namespace history_report

#endif  // CHROME_BROWSER_ANDROID_HISTORY_REPORT_HISTORIC_VISITS_MIGRATION_TASK_H_
