// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRINTING_WEB_API_IN_PROGRESS_JOBS_STORAGE_CHROMEOS_H_
#define CHROME_BROWSER_PRINTING_WEB_API_IN_PROGRESS_JOBS_STORAGE_CHROMEOS_H_

#include <cstdint>
#include <optional>
#include <string>
#include <utility>

#include "base/containers/flat_map.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ash/printing/cups_print_job_manager.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote_set.h"
#include "third_party/blink/public/mojom/printing/web_printing.mojom.h"

namespace ash {
class CupsPrintJob;
}

namespace printing {

class InProgressJobsStorageChromeOS
    : public blink::mojom::WebPrintJobController,
      public ash::CupsPrintJobManager::Observer {
 public:
  InProgressJobsStorageChromeOS();
  ~InProgressJobsStorageChromeOS() override;

  // blink::mojom::WebPrintJobController:
  void Cancel() override;

  // ash::CupsPrintJobManager::Observer:
  void OnPrintJobStarted(base::WeakPtr<ash::CupsPrintJob> job) override;
  void OnPrintJobUpdated(base::WeakPtr<ash::CupsPrintJob> job) override;
  void OnPrintJobDone(base::WeakPtr<ash::CupsPrintJob> job) override;
  void OnPrintJobError(base::WeakPtr<ash::CupsPrintJob> job) override;
  void OnPrintJobCancelled(base::WeakPtr<ash::CupsPrintJob> job) override;

  // Adds a job with `job_id` from `printer_id` to the storage and starts
  // dispatching notifications to it via the supplied `observer`.
  void PrintJobAcknowledgedByThePrintSystem(
      const std::string& printer_id,
      uint32_t job_id,
      mojo::PendingRemote<blink::mojom::WebPrintJobStateObserver> observer,
      mojo::PendingReceiver<blink::mojom::WebPrintJobController> controller);

 private:
  using PrintJobUniqueId =
      std::pair</*printer_id=*/std::string, /*job_id=*/uint32_t>;
  using ObserverControllerIdPair =
      std::pair</*observer_id=*/mojo::RemoteSetElementId,
                /*controller_id=*/mojo::ReceiverId>;

  // When observer with `observer_id` disconnects, this function cleans up
  // everything related to that job.
  void OnStateObserverDisconnected(mojo::RemoteSetElementId observer_id_in);

  void UpdateJobState(base::WeakPtr<ash::CupsPrintJob> job,
                      blink::mojom::WebPrintJobState state);

  // Invariant:
  // * `state_observers_` has `observer_id` <=> `job_id_to_observer_id_` has
  //   a `job_id` that maps to `observer_id`;
  mojo::RemoteSet<blink::mojom::WebPrintJobStateObserver> state_observers_;
  base::flat_map<PrintJobUniqueId, ObserverControllerIdPair>
      job_id_to_observer_controller_id_pair_;
  mojo::ReceiverSet<blink::mojom::WebPrintJobController, PrintJobUniqueId>
      controllers_;

  base::ScopedObservation<ash::CupsPrintJobManager,
                          ash::CupsPrintJobManager::Observer>
      cups_manager_observation_{this};
};

}  // namespace printing

#endif  // CHROME_BROWSER_PRINTING_WEB_API_IN_PROGRESS_JOBS_STORAGE_CHROMEOS_H_
