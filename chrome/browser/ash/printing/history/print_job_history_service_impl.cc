// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/printing/history/print_job_history_service_impl.h"

#include "base/functional/callback_helpers.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_macros.h"
#include "base/uuid.h"
#include "chrome/browser/ash/printing/cups_print_job.h"
#include "chrome/browser/ash/printing/history/print_job_info_proto_conversions.h"
#include "chrome/browser/printing/print_job.h"
#include "components/prefs/pref_service.h"

namespace ash {

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
                     weak_ptr_factory_.GetWeakPtr()));
}

PrintJobHistoryServiceImpl::~PrintJobHistoryServiceImpl() {
  DCHECK(print_job_manager_);
  print_job_manager_->RemoveObserver(this);
}

void PrintJobHistoryServiceImpl::GetPrintJobs(
    PrintJobDatabase::GetPrintJobsCallback callback) {
  print_job_history_cleaner_.CleanUp(
      base::BindOnce(&PrintJobHistoryServiceImpl::OnPrintJobsCleanedUp,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void PrintJobHistoryServiceImpl::DeleteAllPrintJobs(
    PrintJobDatabase::DeletePrintJobsCallback callback) {
  print_job_database_->Clear(
      base::BindOnce(&PrintJobHistoryServiceImpl::OnClearDone,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void PrintJobHistoryServiceImpl::OnClearDone(
    PrintJobDatabase::DeletePrintJobsCallback callback,
    bool success) {
  std::move(callback).Run(success);
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

  // Prevent saving print jobs if it's from incognito browser sessions.
  // TODO(crbug/1053704): Add policy pref to enable storing incognito print
  // jobs.
  if (job->source() == ::printing::PrintJob::Source::kPrintPreviewIncognito) {
    return;
  }

  printing::proto::PrintJobInfo print_job_info = CupsPrintJobToProto(
      *job, /*id=*/base::Uuid::GenerateRandomV4().AsLowercaseString(),
      base::Time::Now());
  print_job_database_->SavePrintJob(
      print_job_info,
      base::BindOnce(&PrintJobHistoryServiceImpl::OnPrintJobSaved,
                     weak_ptr_factory_.GetWeakPtr(), print_job_info));
}

void PrintJobHistoryServiceImpl::OnPrintJobDatabaseInitialized(bool success) {
  if (success)
    print_job_history_cleaner_.CleanUp(base::DoNothing());
}

void PrintJobHistoryServiceImpl::OnPrintJobSaved(
    const printing::proto::PrintJobInfo& print_job_info,
    bool success) {
  for (auto& observer : observers_) {
    observer.OnPrintJobFinished(print_job_info);
  }
}

void PrintJobHistoryServiceImpl::OnPrintJobsCleanedUp(
    PrintJobDatabase::GetPrintJobsCallback callback) {
  print_job_database_->GetPrintJobs(
      base::BindOnce(&PrintJobHistoryServiceImpl::OnGetPrintJobsDone,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void PrintJobHistoryServiceImpl::OnGetPrintJobsDone(
    PrintJobDatabase::GetPrintJobsCallback callback,
    bool success,
    std::vector<printing::proto::PrintJobInfo> entries) {
  std::move(callback).Run(success, entries);
}

}  // namespace ash
