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
#include "chromeos/crosapi/mojom/local_printer.mojom.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote_set.h"
#include "third_party/blink/public/mojom/printing/web_printing.mojom.h"

namespace printing {

class InProgressJobsStorageChromeOS
    : public blink::mojom::WebPrintJobController,
      public crosapi::mojom::PrintJobObserver {
 public:
  InProgressJobsStorageChromeOS();
  ~InProgressJobsStorageChromeOS() override;

  // blink::mojom::WebPrintJobController:
  void Cancel() override;

  // crosapi::mojom::PrintJobObserver:
  void OnPrintJobUpdateDeprecated(
      const std::string& printer_id,
      uint32_t job_id,
      crosapi::mojom::PrintJobStatus status) override;
  void OnPrintJobUpdate(const std::string& printer_id,
                        uint32_t job_id,
                        crosapi::mojom::PrintJobUpdatePtr update) override;

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

  // Invariant:
  // * `state_observers_` has `observer_id` <=> `job_id_to_observer_id_` has
  //   a `job_id` that maps to `observer_id`;
  mojo::RemoteSet<blink::mojom::WebPrintJobStateObserver> state_observers_;
  base::flat_map<PrintJobUniqueId, ObserverControllerIdPair>
      job_id_to_observer_controller_id_pair_;
  mojo::ReceiverSet<blink::mojom::WebPrintJobController, PrintJobUniqueId>
      controllers_;

  mojo::Receiver<crosapi::mojom::PrintJobObserver> observer_{this};
};

}  // namespace printing

#endif  // CHROME_BROWSER_PRINTING_WEB_API_IN_PROGRESS_JOBS_STORAGE_CHROMEOS_H_
