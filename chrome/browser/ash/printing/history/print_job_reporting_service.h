// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_PRINTING_HISTORY_PRINT_JOB_REPORTING_SERVICE_H_
#define CHROME_BROWSER_ASH_PRINTING_HISTORY_PRINT_JOB_REPORTING_SERVICE_H_

#include <memory>

#include "base/task/sequenced_task_runner.h"
#include "chrome/browser/ash/printing/history/print_job_history_service.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/reporting/client/report_queue.h"

namespace ash {

// This service is responsible for reporting print jobs.
class PrintJobReportingService : public KeyedService,
                                 public PrintJobHistoryService::Observer {
 public:
  static std::unique_ptr<PrintJobReportingService> Create();

  // Test helper for creating a PrintJobReportingService using the specified
  // report queue
  static std::unique_ptr<PrintJobReportingService> CreateForTest(
      std::unique_ptr<::reporting::ReportQueue, base::OnTaskRunnerDeleter>
          report_queue);

  ~PrintJobReportingService() override = default;

  // PrintJobHistoryService::Observer:
  void OnPrintJobFinished(
      const printing::proto::PrintJobInfo& print_job_info) override = 0;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_PRINTING_HISTORY_PRINT_JOB_REPORTING_SERVICE_H_
