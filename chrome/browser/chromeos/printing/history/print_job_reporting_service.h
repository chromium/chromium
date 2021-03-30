// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_PRINTING_HISTORY_PRINT_JOB_REPORTING_SERVICE_H_
#define CHROME_BROWSER_CHROMEOS_PRINTING_HISTORY_PRINT_JOB_REPORTING_SERVICE_H_

#include <memory>

#include "base/callback.h"
#include "chrome/browser/chromeos/printing/history/print_job_history_service.h"
#include "chrome/browser/chromeos/printing/history/print_job_info.pb.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/reporting/client/report_queue.h"

namespace chromeos {

// This service is responsible for reporting print jobs.
class PrintJobReportingService : public KeyedService,
                                 public PrintJobHistoryService::Observer {
 public:
  static std::unique_ptr<PrintJobReportingService> Create();

  ~PrintJobReportingService() override = default;

  virtual base::OnceCallback<void(std::unique_ptr<reporting::ReportQueue>)>
  GetReportQueueSetter() = 0;

  // PrintJobHistoryService::Observer:
  void OnPrintJobFinished(
      const printing::proto::PrintJobInfo& print_job_info) override = 0;
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_PRINTING_HISTORY_PRINT_JOB_REPORTING_SERVICE_H_
