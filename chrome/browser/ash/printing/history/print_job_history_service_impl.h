// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_PRINTING_HISTORY_PRINT_JOB_HISTORY_SERVICE_IMPL_H_
#define CHROME_BROWSER_ASH_PRINTING_HISTORY_PRINT_JOB_HISTORY_SERVICE_IMPL_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ash/printing/cups_print_job_manager.h"
#include "chrome/browser/ash/printing/history/print_job_database.h"
#include "chrome/browser/ash/printing/history/print_job_history_cleaner.h"
#include "chrome/browser/ash/printing/history/print_job_history_service.h"

class PrefService;

namespace ash {

class CupsPrintJobManager;

// This service is responsible for maintaining print job history.
// It observes CupsPrintJobManager events and uses PrintJobDatabase as
// persistent storage for print job history.
class PrintJobHistoryServiceImpl : public PrintJobHistoryService,
                                   public CupsPrintJobManager::Observer {
 public:
  PrintJobHistoryServiceImpl(
      std::unique_ptr<PrintJobDatabase> print_job_database,
      CupsPrintJobManager* print_job_manager,
      PrefService* pref_service);

  PrintJobHistoryServiceImpl(const PrintJobHistoryServiceImpl&) = delete;
  PrintJobHistoryServiceImpl& operator=(const PrintJobHistoryServiceImpl&) =
      delete;

  ~PrintJobHistoryServiceImpl() override;

  // PrintJobHistoryService:
  void GetPrintJobs(PrintJobDatabase::GetPrintJobsCallback callback) override;
  void DeleteAllPrintJobs(
      PrintJobDatabase::DeletePrintJobsCallback callback) override;

 private:
  // CupsPrintJobManager::Observer:
  void OnPrintJobDone(base::WeakPtr<CupsPrintJob> job) override;
  void OnPrintJobError(base::WeakPtr<CupsPrintJob> job) override;
  void OnPrintJobCancelled(base::WeakPtr<CupsPrintJob> job) override;

  void SavePrintJob(base::WeakPtr<CupsPrintJob> job);

  void OnPrintJobDatabaseInitialized(bool success);

  void OnPrintJobSaved(const printing::proto::PrintJobInfo& print_job_info,
                       bool success);

  // Helper function to make sure that callback is only called as long as
  // this class instance still lives.
  void OnClearDone(PrintJobDatabase::DeletePrintJobsCallback callback,
                   bool success);

  void OnPrintJobsCleanedUp(PrintJobDatabase::GetPrintJobsCallback callback);
  // Helper function to make sure that callback is only called as long as
  // this class instance still lives.
  void OnGetPrintJobsDone(PrintJobDatabase::GetPrintJobsCallback callback,
                          bool success,
                          std::vector<printing::proto::PrintJobInfo> entries);

  std::unique_ptr<PrintJobDatabase> print_job_database_;
  raw_ptr<CupsPrintJobManager> print_job_manager_;
  PrintJobHistoryCleaner print_job_history_cleaner_;
  // Used for avoiding that callbacks are called after the class was
  // destroyed already.
  base::WeakPtrFactory<PrintJobHistoryServiceImpl> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_PRINTING_HISTORY_PRINT_JOB_HISTORY_SERVICE_IMPL_H_
