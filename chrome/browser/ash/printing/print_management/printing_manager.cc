// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/printing/print_management/printing_manager.h"

#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "chrome/browser/ash/printing/cups_print_job.h"
#include "chrome/browser/ash/printing/history/print_job_history_service.h"
#include "chrome/browser/ash/printing/history/print_job_history_service_factory.h"
#include "chrome/browser/ash/printing/print_management/print_job_info_mojom_conversions.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/browser/history_types.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_context.h"

namespace ash {
namespace printing {
namespace print_management {

using ::history::DeletionInfo;
using ::history::HistoryService;
using proto::PrintJobInfo;
namespace mojom = ::chromeos::printing::printing_manager::mojom;

PrintingManager::PrintingManager(
    PrintJobHistoryService* print_job_history_service,
    HistoryService* history_service,
    CupsPrintJobManager* cups_print_job_manager,
    PrefService* pref_service)
    : print_job_history_service_(print_job_history_service),
      history_service_(history_service),
      cups_print_job_manager_(cups_print_job_manager) {
  DCHECK(history_service_);
  DCHECK(cups_print_job_manager_);
  history_service_observation_.Observe(history_service_.get());
  cups_print_job_manager_->AddObserver(this);

  delete_print_job_history_allowed_.Init(prefs::kDeletePrintJobHistoryAllowed,
                                         pref_service);
  print_job_history_expiration_period_.Init(
      prefs::kPrintJobHistoryExpirationPeriod, pref_service);
}

PrintingManager::~PrintingManager() {
  DCHECK(history_service_);
  DCHECK(cups_print_job_manager_);
  history_service_observation_.Reset();
  cups_print_job_manager_->RemoveObserver(this);
}

void PrintingManager::GetPrintJobs(GetPrintJobsCallback callback) {
  print_job_history_service_->GetPrintJobs(
      base::BindOnce(&PrintingManager::OnPrintJobsRetrieved,
                     base::Unretained(this), std::move(callback)));
}
void PrintingManager::GetPrintJobHistoryExpirationPeriod(
    GetPrintJobHistoryExpirationPeriodCallback callback) {
  std::move(callback).Run(print_job_history_expiration_period_.GetValue(),
                          print_job_history_expiration_period_.IsManaged());
}

void PrintingManager::DeleteAllPrintJobs(DeleteAllPrintJobsCallback callback) {
  if (!IsHistoryDeletionAllowedByPolicy()) {
    std::move(callback).Run(/*success=*/false);
    return;
  }

  print_job_history_service_->DeleteAllPrintJobs(std::move(callback));
}

void PrintingManager::CancelPrintJob(const std::string& id,
                                     CancelPrintJobCallback callback) {
  // Checks if the print job is still stored in the local cache and the validity
  // of the WeakPtr and do not attempt to cancel an invalid print job.
  if (!base::Contains(active_print_jobs_, id) || !active_print_jobs_[id]) {
    std::move(callback).Run(/*attempted_cancel=*/false);
    return;
  }

  CupsPrintJob* print_job = active_print_jobs_[id].get();
  cups_print_job_manager_->CancelPrintJob(print_job);
  std::move(callback).Run(/*attempted_cancel=*/true);
}

void PrintingManager::ObservePrintJobs(
    mojo::PendingRemote<mojom::PrintJobsObserver> observer,
    ObservePrintJobsCallback callback) {
  print_job_observers_.Add(std::move(observer));
  std::move(callback).Run();
}

void PrintingManager::GetDeletePrintJobHistoryAllowedByPolicy(
    GetDeletePrintJobHistoryAllowedByPolicyCallback callback) {
  return std::move(callback).Run(IsHistoryDeletionAllowedByPolicy());
}

void PrintingManager::OnHistoryDeletions(HistoryService* history_service,
                                         const DeletionInfo& deletion_info) {
  // We only handle deletion of all history because it is an explicit action by
  // user to explicitly remove all their history-related content.
  if (!IsHistoryDeletionAllowedByPolicy() || !deletion_info.IsAllHistory()) {
    return;
  }

  DeleteAllPrintJobs(base::BindOnce(&PrintingManager::OnPrintJobsDeleted,
                                    weak_ptr_factory_.GetWeakPtr()));
}

void PrintingManager::OnPrintJobCreated(base::WeakPtr<CupsPrintJob> job) {
  UpdatePrintJob(job);
}

void PrintingManager::OnPrintJobStarted(base::WeakPtr<CupsPrintJob> job) {
  UpdatePrintJob(job);
}

void PrintingManager::OnPrintJobUpdated(base::WeakPtr<CupsPrintJob> job) {
  UpdatePrintJob(job);
}

void PrintingManager::OnPrintJobSuspended(base::WeakPtr<CupsPrintJob> job) {
  UpdatePrintJob(job);
}

void PrintingManager::OnPrintJobResumed(base::WeakPtr<CupsPrintJob> job) {
  UpdatePrintJob(job);
}

void PrintingManager::OnPrintJobDone(base::WeakPtr<CupsPrintJob> job) {
  RemoveAndUpdatePrintJob(job);
}

void PrintingManager::OnPrintJobError(base::WeakPtr<CupsPrintJob> job) {
  RemoveAndUpdatePrintJob(job);
}

void PrintingManager::OnPrintJobCancelled(base::WeakPtr<CupsPrintJob> job) {
  RemoveAndUpdatePrintJob(job);
}

void PrintingManager::OnPrintJobsDeleted(bool success) {
  DCHECK(success) << "Clearing print jobs failed unexpectedly.";
  for (auto& observer : print_job_observers_) {
    observer->OnAllPrintJobsDeleted();
  }
}

void PrintingManager::OnPrintJobsRetrieved(
    GetPrintJobsCallback callback,
    bool success,
    std::vector<PrintJobInfo> print_job_info_protos) {
  std::vector<mojom::PrintJobInfoPtr> print_job_infos;
  print_job_infos.reserve(print_job_info_protos.size() +
                          active_print_jobs_.size());

  if (success) {
    for (const auto& print_job_info : print_job_info_protos) {
      print_job_infos.push_back(PrintJobProtoToMojom(print_job_info));
    }
    for (const auto& kv : active_print_jobs_) {
      if (kv.second) {
        const CupsPrintJob& print_job(*kv.second);
        print_job_infos.push_back(CupsPrintJobToMojom(print_job));
      }
    }
  }

  std::move(callback).Run(std::move(print_job_infos));
}

void PrintingManager::UpdatePrintJob(base::WeakPtr<CupsPrintJob> job) {
  if (!job) {
    LOG(WARNING) << "Failed to update an invalid print job.";
    return;
  }

  active_print_jobs_[job->GetUniqueId()] = job;
  NotifyPrintJobObservers(job);
}

void PrintingManager::RemoveAndUpdatePrintJob(base::WeakPtr<CupsPrintJob> job) {
  if (!job) {
    LOG(WARNING) << "Failed to update and remove an invalid print job.";
    return;
  }

  active_print_jobs_.erase(job->GetUniqueId());
  NotifyPrintJobObservers(job);
}

void PrintingManager::NotifyPrintJobObservers(base::WeakPtr<CupsPrintJob> job) {
  DCHECK(job);
  for (auto& observer : print_job_observers_) {
    observer->OnPrintJobUpdate(CupsPrintJobToMojom(*job));
  }
}

void PrintingManager::BindInterface(
    mojo::PendingReceiver<mojom::PrintingMetadataProvider> pending_receiver) {
  receiver_.reset();
  receiver_.Bind(std::move(pending_receiver));
}

bool PrintingManager::IsHistoryDeletionAllowedByPolicy() {
  return delete_print_job_history_allowed_.GetValue();
}

void PrintingManager::Shutdown() {
  receiver_.reset();
  weak_ptr_factory_.InvalidateWeakPtrs();
}

}  // namespace print_management
}  // namespace printing
}  // namespace ash
