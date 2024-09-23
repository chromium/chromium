// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_PRINTING_PRINT_MANAGEMENT_PRINTING_MANAGER_H_
#define CHROME_BROWSER_ASH_PRINTING_PRINT_MANAGEMENT_PRINTING_MANAGER_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ash/printing/cups_print_job_manager.h"
#include "chrome/browser/ash/printing/history/print_job_info.pb.h"
#include "chromeos/components/print_management/mojom/printing_manager.mojom.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/browser/history_service_observer.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/prefs/pref_member.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote_set.h"

class PrefService;

namespace history {
class DeletionInfo;
class HistoryService;
}  // namespace history

namespace ash {

class PrintJobHistoryService;

namespace printing {
namespace print_management {

class PrintingManager : public chromeos::printing::printing_manager::mojom::
                            PrintingMetadataProvider,
                        public KeyedService,
                        public history::HistoryServiceObserver,
                        public CupsPrintJobManager::Observer {
 public:
  PrintingManager(PrintJobHistoryService* print_job_history_service,
                  history::HistoryService* history_service,
                  CupsPrintJobManager* cups_print_job_manager,
                  PrefService* pref_service);

  ~PrintingManager() override;

  PrintingManager(const PrintingManager&) = delete;
  PrintingManager& operator=(const PrintingManager&) = delete;

  // printing_chromeos::printing::manager::mojom::PrintingMetadataProvider:
  void GetPrintJobs(GetPrintJobsCallback callback) override;
  void GetPrintJobHistoryExpirationPeriod(
      GetPrintJobHistoryExpirationPeriodCallback callback) override;
  void DeleteAllPrintJobs(DeleteAllPrintJobsCallback callback) override;
  void ObservePrintJobs(
      mojo::PendingRemote<
          chromeos::printing::printing_manager::mojom::PrintJobsObserver>
          observer,
      ObservePrintJobsCallback callback) override;
  void CancelPrintJob(const std::string& id,
                      CancelPrintJobCallback callback) override;
  void GetDeletePrintJobHistoryAllowedByPolicy(
      GetDeletePrintJobHistoryAllowedByPolicyCallback callback) override;

  void BindInterface(
      mojo::PendingReceiver<
          chromeos::printing::printing_manager::mojom::PrintingMetadataProvider>
          pending_receiver);

 private:
  // KeyedService:
  void Shutdown() override;

  // history::HistoryServiceObserver:
  void OnHistoryDeletions(history::HistoryService* history_service,
                          const history::DeletionInfo& deletion_info) override;

  // CupsPrintJobManager::Observer impls
  void OnPrintJobCreated(base::WeakPtr<CupsPrintJob> job) override;
  void OnPrintJobStarted(base::WeakPtr<CupsPrintJob> job) override;
  void OnPrintJobUpdated(base::WeakPtr<CupsPrintJob> job) override;
  void OnPrintJobSuspended(base::WeakPtr<CupsPrintJob> job) override;
  void OnPrintJobResumed(base::WeakPtr<CupsPrintJob> job) override;
  void OnPrintJobDone(base::WeakPtr<CupsPrintJob> job) override;
  void OnPrintJobError(base::WeakPtr<CupsPrintJob> job) override;
  void OnPrintJobCancelled(base::WeakPtr<CupsPrintJob> job) override;

  // PrintJobHistoryObserver
  void OnPrintJobsRetrieved(
      GetPrintJobsCallback callback,
      bool success,
      std::vector<proto::PrintJobInfo> print_job_info_protos);

  // Callback function that is called when the print jobs are cleared from the
  // local database.
  void OnPrintJobsDeleted(bool success);

  // Returns true if the policy pref is enabled to allow history deletions.
  bool IsHistoryDeletionAllowedByPolicy();

  // Stores |job| to local cache and notifies observers of an update to |job|.
  void UpdatePrintJob(base::WeakPtr<CupsPrintJob> job);

  // Removes |job| from the local cache and notifies observers of an update to
  // |job|.
  void RemoveAndUpdatePrintJob(base::WeakPtr<CupsPrintJob> job);

  // Notifies all observers in |print_job_observers_| of an update to a print
  // job.
  void NotifyPrintJobObservers(base::WeakPtr<CupsPrintJob> job);

  // Local cache that stores all ongoing print jobs.
  std::map<std::string, base::WeakPtr<CupsPrintJob>> active_print_jobs_;

  // Set of PrintJobsObserver mojom::remotes, each remote is bound to a
  // renderer process receiver. Automatically handles removing disconnected
  // receivers.
  mojo::RemoteSet<
      chromeos::printing::printing_manager::mojom::PrintJobsObserver>
      print_job_observers_;

  mojo::Receiver<
      chromeos::printing::printing_manager::mojom::PrintingMetadataProvider>
      receiver_{this};

  // Policy-controlled pref that determines whether print job history can be
  // deleted.
  BooleanPrefMember delete_print_job_history_allowed_;

  // Not owned, this is the intermediate layer to interact with the print
  // job local database.
  raw_ptr<PrintJobHistoryService, DanglingUntriaged> print_job_history_service_;

  // Not owned, this provides the necessary observers to observe when browser
  // history has been cleared.
  raw_ptr<history::HistoryService> history_service_;

  // Not owned, this provides the necessary observers to observe when an
  // ongoing print job has been updated.
  raw_ptr<CupsPrintJobManager> cups_print_job_manager_;

  IntegerPrefMember print_job_history_expiration_period_;

  base::ScopedObservation<history::HistoryService,
                          history::HistoryServiceObserver>
      history_service_observation_{this};

  base::WeakPtrFactory<PrintingManager> weak_ptr_factory_{this};
};

}  // namespace print_management
}  // namespace printing
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_PRINTING_PRINT_MANAGEMENT_PRINTING_MANAGER_H_
