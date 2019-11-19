// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_PRINTING_HISTORY_PRINT_JOB_HISTORY_SERVICE_IMPL_H_
#define CHROME_BROWSER_CHROMEOS_PRINTING_HISTORY_PRINT_JOB_HISTORY_SERVICE_IMPL_H_

#include "chrome/browser/chromeos/printing/cups_print_job_manager.h"
#include "chrome/browser/chromeos/printing/history/print_job_database.h"
#include "chrome/browser/chromeos/printing/history/print_job_history_cleaner.h"
#include "chrome/browser/chromeos/printing/history/print_job_history_service.h"

class PrefService;

namespace chromeos {

class CupsPrintJobManager;

// This service is responsible for maintaining print job history.
// It observes CupsPrintJobManager events and uses PrintJobDatabase as
// persistent storage for print job history.
class PrintJobHistoryServiceImpl
    : public PrintJobHistoryService,
      public chromeos::CupsPrintJobManager::Observer {
 public:
  PrintJobHistoryServiceImpl(
      std::unique_ptr<PrintJobDatabase> print_job_database,
      CupsPrintJobManager* print_job_manager,
      PrefService* pref_service);
  ~PrintJobHistoryServiceImpl() override;

  // PrintJobHistoryService:
  void GetPrintJobs(PrintJobDatabase::GetPrintJobsCallback callback) override;

 private:
  // CupsPrintJobManager::Observer:
  void OnPrintJobDone(base::WeakPtr<CupsPrintJob> job) override;
  void OnPrintJobError(base::WeakPtr<CupsPrintJob> job) override;
  void OnPrintJobCancelled(base::WeakPtr<CupsPrintJob> job) override;

  void SavePrintJob(base::WeakPtr<CupsPrintJob> job);

  void OnPrintJobDatabaseInitialized(bool success);

  void OnPrintJobSaved(const printing::proto::PrintJobInfo& print_job_info,
                       bool success);

  void OnPrintJobsCleanedUp(PrintJobDatabase::GetPrintJobsCallback callback);

  std::unique_ptr<PrintJobDatabase> print_job_database_;
  CupsPrintJobManager* print_job_manager_;
  PrintJobHistoryCleaner print_job_history_cleaner_;

  DISALLOW_COPY_AND_ASSIGN(PrintJobHistoryServiceImpl);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_PRINTING_HISTORY_PRINT_JOB_HISTORY_SERVICE_IMPL_H_
