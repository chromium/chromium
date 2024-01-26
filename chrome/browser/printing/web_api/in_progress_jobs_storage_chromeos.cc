// Copyright 2023 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/printing/web_api/in_progress_jobs_storage_chromeos.h"

#include "base/check_deref.h"
#include "base/containers/fixed_flat_map.h"
#include "base/containers/fixed_flat_set.h"
#include "base/containers/map_util.h"
#include "chrome/browser/printing/local_printer_utils_chromeos.h"

namespace printing {

namespace {

using PrintJobStatus = crosapi::mojom::PrintJobStatus;
using WebPrintJobState = blink::mojom::WebPrintJobState;

// Accepted job states. The ones not listed here are silently discarded.
static constexpr auto kJobStatusToJobStateMapping =
    base::MakeFixedFlatMap<PrintJobStatus, WebPrintJobState>({
        // clang-format off
        {PrintJobStatus::kStarted,   WebPrintJobState::kProcessing},
        {PrintJobStatus::kDone,      WebPrintJobState::kCompleted},
        {PrintJobStatus::kCancelled, WebPrintJobState::kCanceled},
        {PrintJobStatus::kError,     WebPrintJobState::kAborted},
        // clang-format on
    });

// Terminal states. Once the job reaches one of these, it will no longer be
// tracked (and hence removed from the storage).
static constexpr auto kTerminalJobStates =
    base::MakeFixedFlatSet<WebPrintJobState>({
        // clang-format off
        WebPrintJobState::kCompleted,
        WebPrintJobState::kCanceled,
        WebPrintJobState::kAborted,
        // clang-format on
    });

}  // namespace

InProgressJobsStorageChromeOS::InProgressJobsStorageChromeOS() {
  GetLocalPrinterInterface()->AddPrintJobObserver(
      observer_.BindNewPipeAndPassRemote(),
      crosapi::mojom::PrintJobSource::kIsolatedWebApp, base::DoNothing());

  // Disconnects might happen if the corresponding frame is going away or the
  // renderer process crashes.
  state_observers_.set_disconnect_handler(base::BindRepeating(
      &InProgressJobsStorageChromeOS::OnStateObserverDisconnected,
      base::Unretained(this)));
}

InProgressJobsStorageChromeOS::~InProgressJobsStorageChromeOS() = default;

void InProgressJobsStorageChromeOS::OnPrintJobUpdateDeprecated(
    const std::string& printer_id,
    uint32_t job_id,
    crosapi::mojom::PrintJobStatus status) {
  NOTREACHED();
}

void InProgressJobsStorageChromeOS::OnPrintJobUpdate(
    const std::string& printer_id,
    uint32_t job_id,
    crosapi::mojom::PrintJobUpdatePtr update) {
  auto observer_id_itr = job_id_to_observer_id_.find({printer_id, job_id});
  if (observer_id_itr == job_id_to_observer_id_.end()) {
    // This job doesn't belong to us or has already been discarded.
    return;
  }

  // See invariant description in the header.
  auto& observer = CHECK_DEREF(state_observers_.Get(observer_id_itr->second));

  // Updates are forwarded to the renderer if either the `state` can be mapped
  // directly or printing is in progress (indicated by `pages_printed` > 0).
  // Cases are possible where the received `state` is unmapped; then it's
  // assumed to be `kProcessing` due to `pages_printed` being greater than zero.
  // Lastly, the notification might end up being equal to the existing job
  // configuration both in terms of `state` and `pages_printed`; in this case it
  // will be silently discarded by the renderer.
  auto* state = base::FindOrNull(kJobStatusToJobStateMapping, update->status);
  if (state || update->pages_printed > 0) {
    auto out_update = blink::mojom::WebPrintJobUpdate::New();
    out_update->state =
        state ? *state : blink::mojom::WebPrintJobState::kProcessing;
    if (update->pages_printed > 0) {
      out_update->pages_printed = update->pages_printed;
    }
    observer.OnWebPrintJobUpdate(std::move(out_update));
  }
  if (state && base::Contains(kTerminalJobStates, *state)) {
    state_observers_.Remove(observer_id_itr->second);
    job_id_to_observer_id_.erase(observer_id_itr);
  }
}

void InProgressJobsStorageChromeOS::PrintJobAcknowledgedByThePrintSystem(
    const std::string& printer_id,
    uint32_t job_id,
    mojo::PendingRemote<blink::mojom::WebPrintJobStateObserver> observer) {
  auto observer_id = state_observers_.Add(std::move(observer));
  job_id_to_observer_id_[{printer_id, job_id}] = observer_id;

  auto update = blink::mojom::WebPrintJobUpdate::New();
  update->state = blink::mojom::WebPrintJobState::kPending;
  CHECK_DEREF(state_observers_.Get(observer_id))
      .OnWebPrintJobUpdate(std::move(update));
}

void InProgressJobsStorageChromeOS::OnStateObserverDisconnected(
    mojo::RemoteSetElementId observer_id) {
  // By the time we get here `observer_id` will have already been removed from
  // `state_observers_`.
  base::EraseIf(job_id_to_observer_id_,
                [&](const auto& entry) { return entry.second == observer_id; });
}

}  // namespace printing
