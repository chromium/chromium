// Copyright 2023 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/printing/web_api/in_progress_jobs_storage_chromeos.h"

#include "base/check_deref.h"
#include "base/containers/fixed_flat_set.h"
#include "base/containers/map_util.h"
#include "base/functional/callback_helpers.h"
#include "chrome/browser/ash/printing/cups_print_job.h"
#include "chrome/browser/ash/printing/cups_print_job_manager_factory.h"
#include "chrome/browser/ash/printing/print_management/printing_manager.h"
#include "chrome/browser/ash/printing/print_management/printing_manager_factory.h"
#include "chrome/browser/printing/print_job.h"
#include "chrome/browser/profiles/profile.h"
#include "chromeos/ash/components/browser_context_helper/browser_context_helper.h"
#include "components/session_manager/core/session.h"
#include "components/session_manager/core/session_manager.h"

namespace printing {

namespace {

using WebPrintJobState = blink::mojom::WebPrintJobState;

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
  // Disconnects might happen if the corresponding frame is going away or the
  // renderer process crashes.
  state_observers_.set_disconnect_handler(base::BindRepeating(
      &InProgressJobsStorageChromeOS::OnStateObserverDisconnected,
      base::Unretained(this)));

  const auto* primary_session =
      session_manager::SessionManager::Get()->GetPrimarySession();
  CHECK(primary_session);
  // TODO(crbug.com/479647640): Check if we should use current user instead of
  // primary user.
  Profile* profile = Profile::FromBrowserContext(
      ash::BrowserContextHelper::Get()->GetBrowserContextByAccountId(
          primary_session->account_id()));
  CHECK(profile);

  ash::CupsPrintJobManager* print_job_manager =
      ash::CupsPrintJobManagerFactory::GetForBrowserContext(profile);
  CHECK(print_job_manager);
  cups_manager_observation_.Observe(print_job_manager);
}

InProgressJobsStorageChromeOS::~InProgressJobsStorageChromeOS() = default;

void InProgressJobsStorageChromeOS::Cancel() {
  const auto& [printer_id, job_id] = controllers_.current_context();
  const auto* primary_session =
      session_manager::SessionManager::Get()->GetPrimarySession();
  CHECK(primary_session);
  // TODO(crbug.com/479647640): Check if we should use current user instead of
  // primary user.
  Profile* profile = Profile::FromBrowserContext(
      ash::BrowserContextHelper::Get()->GetBrowserContextByAccountId(
          primary_session->account_id()));
  CHECK(profile);
  ash::printing::print_management::PrintingManagerFactory::GetForProfile(
      profile)
      ->CancelPrintJob(ash::CupsPrintJob::CreateUniqueId(printer_id, job_id),
                       base::DoNothing());
}

void InProgressJobsStorageChromeOS::OnPrintJobStarted(
    base::WeakPtr<ash::CupsPrintJob> job) {
  // Transitions the print job state from pending to processing.
  UpdateJobState(job, blink::mojom::WebPrintJobState::kProcessing);
}

void InProgressJobsStorageChromeOS::OnPrintJobUpdated(
    base::WeakPtr<ash::CupsPrintJob> job) {
  // The print job remains in the processing state, but its progress
  // (e.g. pages printed) is updated.
  UpdateJobState(job, blink::mojom::WebPrintJobState::kProcessing);
}

void InProgressJobsStorageChromeOS::OnPrintJobDone(
    base::WeakPtr<ash::CupsPrintJob> job) {
  UpdateJobState(job, blink::mojom::WebPrintJobState::kCompleted);
}

void InProgressJobsStorageChromeOS::OnPrintJobError(
    base::WeakPtr<ash::CupsPrintJob> job) {
  UpdateJobState(job, blink::mojom::WebPrintJobState::kAborted);
}

void InProgressJobsStorageChromeOS::OnPrintJobCancelled(
    base::WeakPtr<ash::CupsPrintJob> job) {
  UpdateJobState(job, blink::mojom::WebPrintJobState::kCanceled);
}

void InProgressJobsStorageChromeOS::UpdateJobState(
    base::WeakPtr<ash::CupsPrintJob> job,
    blink::mojom::WebPrintJobState state) {
  if (!job || job->source() != PrintJob::Source::kIsolatedWebApp) {
    return;
  }

  auto id_pair_itr = job_id_to_observer_controller_id_pair_.find(
      {job->printer().id(), job->job_id()});
  if (id_pair_itr == job_id_to_observer_controller_id_pair_.end()) {
    // This job doesn't belong to us or has already been discarded.
    return;
  }

  // See invariant description in the header.
  const auto& [observer_id, controller_id] = id_pair_itr->second;
  auto& observer = CHECK_DEREF(state_observers_.Get(observer_id));

  auto out_update = blink::mojom::WebPrintJobUpdate::New();
  out_update->state = state;
  if (job->printed_page_number() > 0) {
    out_update->pages_printed = job->printed_page_number();
  }
  observer.OnWebPrintJobUpdate(std::move(out_update));

  if (kTerminalJobStates.contains(state)) {
    state_observers_.Remove(observer_id);
    controllers_.Remove(controller_id);
    job_id_to_observer_controller_id_pair_.erase(id_pair_itr);
  }
}

void InProgressJobsStorageChromeOS::PrintJobAcknowledgedByThePrintSystem(
    const std::string& printer_id,
    uint32_t job_id,
    mojo::PendingRemote<blink::mojom::WebPrintJobStateObserver> observer,
    mojo::PendingReceiver<blink::mojom::WebPrintJobController> controller) {
  PrintJobUniqueId composite_id(printer_id, job_id);
  auto controller_id =
      controllers_.Add(this, std::move(controller), composite_id);
  auto observer_id = state_observers_.Add(std::move(observer));
  job_id_to_observer_controller_id_pair_[composite_id] =
      ObserverControllerIdPair(observer_id, controller_id);

  auto update = blink::mojom::WebPrintJobUpdate::New();
  update->state = blink::mojom::WebPrintJobState::kPending;
  CHECK_DEREF(state_observers_.Get(observer_id))
      .OnWebPrintJobUpdate(std::move(update));
}

void InProgressJobsStorageChromeOS::OnStateObserverDisconnected(
    mojo::RemoteSetElementId observer_id_in) {
  // By the time we get here `observer_id_in` will have already been removed
  // from `state_observers_`.
  base::EraseIf(job_id_to_observer_controller_id_pair_, [&](const auto& entry) {
    const auto& [observer_id, controller_id] = entry.second;
    return observer_id_in == observer_id;
  });
}

}  // namespace printing
