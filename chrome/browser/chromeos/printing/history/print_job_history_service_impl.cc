// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/printing/history/print_job_history_service_impl.h"

#include "base/bind_helpers.h"
#include "base/guid.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_macros.h"
#include "chrome/browser/chromeos/printing/cups_print_job.h"
#include "chrome/browser/chromeos/printing/history/print_job_info_proto_conversions.h"
#include "components/prefs/pref_service.h"

namespace chromeos {

PrintJobHistoryServiceImpl::PrintJobHistoryServiceImpl(
    std::unique_ptr<PrintJobDatabase> print_job_database,
    CupsPrintJobManager* print_job_manager,
    PrefService* pref_service)
    : print_job_database_(std::move(print_job_database)),
      print_job_manager_(print_job_manager),
      print_job_history_cleaner_(print_job_database_.get(), pref_service) {
  DCHECK(print_job_manager_);
  print_job_manager_->AddObserver(this);
  print_job_database_->Initialize(
      base::BindOnce(&PrintJobHistoryServiceImpl::OnPrintJobDatabaseInitialized,
                     base::Unretained(this)));
}

PrintJobHistoryServiceImpl::~PrintJobHistoryServiceImpl() {
  DCHECK(print_job_manager_);
  print_job_manager_->RemoveObserver(this);
}

void PrintJobHistoryServiceImpl::GetPrintJobs(
    PrintJobDatabase::GetPrintJobsCallback callback) {
  print_job_history_cleaner_.CleanUp(
      base::BindOnce(&PrintJobHistoryServiceImpl::OnPrintJobsCleanedUp,
                     base::Unretained(this), std::move(callback)));
}

void PrintJobHistoryServiceImpl::OnPrintJobDone(
    base::WeakPtr<CupsPrintJob> job) {
  SavePrintJob(job);
}

void PrintJobHistoryServiceImpl::OnPrintJobError(
    base::WeakPtr<CupsPrintJob> job) {
  SavePrintJob(job);
}

void PrintJobHistoryServiceImpl::OnPrintJobCancelled(
    base::WeakPtr<CupsPrintJob> job) {
  SavePrintJob(job);
}

void PrintJobHistoryServiceImpl::SavePrintJob(base::WeakPtr<CupsPrintJob> job) {
  if (!job)
    return;
  printing::proto::PrintJobInfo print_job_info =
      CupsPrintJobToProto(*job, /*id=*/base::GenerateGUID(), base::Time::Now());
  print_job_database_->SavePrintJob(
      print_job_info,
      base::BindOnce(&PrintJobHistoryServiceImpl::OnPrintJobSaved,
                     base::Unretained(this), print_job_info));
}

void PrintJobHistoryServiceImpl::OnPrintJobDatabaseInitialized(bool success) {
  if (success)
    print_job_history_cleaner_.CleanUp(base::DoNothing());
}

void PrintJobHistoryServiceImpl::OnPrintJobSaved(
    const printing::proto::PrintJobInfo& print_job_info,
    bool success) {
  UMA_HISTOGRAM_BOOLEAN("Printing.CUPS.PrintJobDatabasePrintJobSaved", success);
  for (auto& observer : observers_) {
    observer.OnPrintJobFinished(print_job_info);
  }
}

void PrintJobHistoryServiceImpl::OnPrintJobsCleanedUp(
    PrintJobDatabase::GetPrintJobsCallback callback) {
  print_job_database_->GetPrintJobs(std::move(callback));
}

}  // namespace chromeos
