// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/printing/fake_cups_print_job_manager.h"

#include <utility>
#include <vector>

#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/task/sequenced_task_runner.h"
#include "chrome/browser/ash/printing/cups_print_job.h"
#include "chrome/browser/ash/printing/cups_print_job_manager.h"
#include "content/public/browser/browser_context.h"
#include "ui/message_center/public/cpp/notification.h"

namespace ash {

FakeCupsPrintJobManager::FakeCupsPrintJobManager(Profile* profile)
    : CupsPrintJobManager(profile) {
  VLOG(1) << "Using Fake Print Job Manager";
}

FakeCupsPrintJobManager::~FakeCupsPrintJobManager() = default;

bool FakeCupsPrintJobManager::CreatePrintJob(
    const std::string& printer_id,
    const std::string& title,
    uint32_t job_id,
    int total_page_number,
    ::printing::PrintJob::Source source,
    const std::string& source_id,
    const printing::proto::PrintSettings& settings) {
  chromeos::Printer printer(printer_id);
  printer.set_display_name(printer_id);

  // Create a new print job.
  print_jobs_.push_back(std::make_unique<CupsPrintJob>(
      printer, job_id, title, total_page_number, source, source_id, settings));

  // Show the waiting-for-printing notification immediately.
  base::SequencedTaskRunner::GetCurrentDefault()->PostNonNestableDelayedTask(
      FROM_HERE,
      base::BindOnce(&FakeCupsPrintJobManager::ChangePrintJobState,
                     weak_ptr_factory_.GetWeakPtr(), print_jobs_.back().get()),
      base::TimeDelta());

  return true;
}

void FakeCupsPrintJobManager::CancelPrintJob(CupsPrintJob* job) {
  job->set_state(CupsPrintJob::State::STATE_CANCELLED);
  NotifyJobCanceled(job->GetWeakPtr());

  // Note: |job| is deleted here.
  std::erase_if(print_jobs_,
                [&](const auto& print_job) { return print_job.get() == job; });
}

bool FakeCupsPrintJobManager::SuspendPrintJob(CupsPrintJob* job) {
  job->set_state(CupsPrintJob::State::STATE_SUSPENDED);
  NotifyJobSuspended(job->GetWeakPtr());
  return true;
}

bool FakeCupsPrintJobManager::ResumePrintJob(CupsPrintJob* job) {
  job->set_state(CupsPrintJob::State::STATE_RESUMED);
  NotifyJobResumed(job->GetWeakPtr());

  base::SequencedTaskRunner::GetCurrentDefault()->PostNonNestableDelayedTask(
      FROM_HERE,
      base::BindOnce(&FakeCupsPrintJobManager::ChangePrintJobState,
                     weak_ptr_factory_.GetWeakPtr(), job),
      base::Milliseconds(3000));

  return true;
}

void FakeCupsPrintJobManager::ChangePrintJobState(CupsPrintJob* job) {
  // |job| might have been deleted.
  const bool found =
      base::Contains(print_jobs_, job, &std::unique_ptr<CupsPrintJob>::get);
  if (!found || job->state() == CupsPrintJob::State::STATE_SUSPENDED ||
      job->state() == CupsPrintJob::State::STATE_FAILED) {
    return;
  }

  switch (job->state()) {
    case CupsPrintJob::State::STATE_NONE:
      job->set_state(CupsPrintJob::State::STATE_WAITING);
      NotifyJobCreated(job->GetWeakPtr());
      break;
    case CupsPrintJob::State::STATE_WAITING:
      job->set_state(CupsPrintJob::State::STATE_STARTED);
      NotifyJobStarted(job->GetWeakPtr());
      break;
    case CupsPrintJob::State::STATE_STARTED:
      job->set_printed_page_number(job->printed_page_number() + 1);
      job->set_state(CupsPrintJob::State::STATE_PAGE_DONE);
      NotifyJobStarted(job->GetWeakPtr());
      break;
    case CupsPrintJob::State::STATE_PAGE_DONE:
    case CupsPrintJob::State::STATE_RESUMED:
      if (job->printed_page_number() == job->total_page_number()) {
        job->set_state(CupsPrintJob::State::STATE_DOCUMENT_DONE);
        NotifyJobDone(job->GetWeakPtr());
      } else {
        job->set_printed_page_number(job->printed_page_number() + 1);
        job->set_state(CupsPrintJob::State::STATE_PAGE_DONE);
        NotifyJobUpdated(job->GetWeakPtr());
      }
      break;
    case CupsPrintJob::State::STATE_DOCUMENT_DONE:
      // Delete |job| since it's completed.
      std::erase_if(print_jobs_, [&](const auto& print_job) {
        return print_job.get() == job;
      });
      break;
    default:
      break;
  }

  base::SequencedTaskRunner::GetCurrentDefault()->PostNonNestableDelayedTask(
      FROM_HERE,
      base::BindOnce(&FakeCupsPrintJobManager::ChangePrintJobState,
                     weak_ptr_factory_.GetWeakPtr(), job),
      base::Milliseconds(3000));
}

}  // namespace ash
