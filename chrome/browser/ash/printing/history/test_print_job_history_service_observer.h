// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_PRINTING_HISTORY_TEST_PRINT_JOB_HISTORY_SERVICE_OBSERVER_H_
#define CHROME_BROWSER_ASH_PRINTING_HISTORY_TEST_PRINT_JOB_HISTORY_SERVICE_OBSERVER_H_

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/ash/printing/history/print_job_history_service.h"

namespace ash {

// Observer that counts the number of times it has been called.
class TestPrintJobHistoryServiceObserver
    : public PrintJobHistoryService::Observer {
 public:
  TestPrintJobHistoryServiceObserver(
      PrintJobHistoryService* print_job_history_service,
      base::RepeatingClosure run_loop_closure);
  ~TestPrintJobHistoryServiceObserver() override;

  int num_print_jobs() { return num_print_jobs_; }

 private:
  // PrintJobHistoryService::Observer:
  void OnPrintJobFinished(
      const printing::proto::PrintJobInfo& print_job_info) override;

  raw_ptr<PrintJobHistoryService> print_job_history_service_;
  base::RepeatingClosure run_loop_closure_;

  // The number of times the observer is called.
  int num_print_jobs_ = 0;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_PRINTING_HISTORY_TEST_PRINT_JOB_HISTORY_SERVICE_OBSERVER_H_
