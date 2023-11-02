// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/printing/history/test_print_job_history_service_observer.h"

namespace ash {

TestPrintJobHistoryServiceObserver::TestPrintJobHistoryServiceObserver(
    PrintJobHistoryService* print_job_history_service,
    base::RepeatingClosure run_loop_closure)
    : print_job_history_service_(print_job_history_service),
      run_loop_closure_(run_loop_closure) {
  print_job_history_service_->AddObserver(this);
}

TestPrintJobHistoryServiceObserver::~TestPrintJobHistoryServiceObserver() {
  print_job_history_service_->RemoveObserver(this);
}

void TestPrintJobHistoryServiceObserver::OnPrintJobFinished(
    const printing::proto::PrintJobInfo& print_job_info) {
  num_print_jobs_++;
  run_loop_closure_.Run();
}

}  // namespace ash
