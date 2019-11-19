// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/printing/fake_cups_print_job_manager.h"

#include <utility>

#include "base/bind.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "chrome/browser/chromeos/printing/cups_print_job.h"
#include "chrome/browser/chromeos/printing/cups_print_job_manager.h"
#include "content/public/browser/browser_context.h"
#include "ui/message_center/public/cpp/notification.h"

namespace chromeos {

// static
int FakeCupsPrintJobManager::next_job_id_ = 0;

FakeCupsPrintJobManager::FakeCupsPrintJobManager(Profile* profile)
    : CupsPrintJobManager(profile) {
  VLOG(1) << "Using Fake Print Job Manager";
}

FakeCupsPrintJobManager::~FakeCupsPrintJobManager() = default;

bool FakeCupsPrintJobManager::CreatePrintJob(const std::string& printer_name,
                                             const std::string& title,
                                             int total_page_number) {
  Printer printer(printer_name);
  printer.set_display_name(printer_name);
  // Create a new print job.
  std::unique_ptr<CupsPrintJob> new_job = std::make_unique<CupsPrintJob>(
      printer, next_job_id_++, title, total_page_number,
      ::printing::PrintJob::Source::PRINT_PREVIEW, /*source_id=*/"",
      printing::proto::PrintSettings());
  print_jobs_.push_back(std::move(new_job));

  // Show the waiting-for-printing notification immediately.
  base::SequencedTaskRunnerHandle::Get()->PostNonNestableDelayedTask(
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
  for (auto iter = print_jobs_.begin(); iter != print_jobs_.end(); ++iter) {
    if (iter->get() == job) {
      print_jobs_.erase(iter);
      break;
    }
  }
}

bool FakeCupsPrintJobManager::SuspendPrintJob(CupsPrintJob* job) {
  job->set_state(CupsPrintJob::State::STATE_SUSPENDED);
  NotifyJobSuspended(job->GetWeakPtr());
  return true;
}

bool FakeCupsPrintJobManager::ResumePrintJob(CupsPrintJob* job) {
  job->set_state(CupsPrintJob::State::STATE_RESUMED);
  NotifyJobResumed(job->GetWeakPtr());

  base::SequencedTaskRunnerHandle::Get()->PostNonNestableDelayedTask(
      FROM_HERE,
      base::BindOnce(&FakeCupsPrintJobManager::ChangePrintJobState,
                     weak_ptr_factory_.GetWeakPtr(), job),
      base::TimeDelta::FromMilliseconds(3000));

  return true;
}

void FakeCupsPrintJobManager::ChangePrintJobState(CupsPrintJob* job) {
  // |job| might have been deleted.
  bool found = false;
  for (auto iter = print_jobs_.begin(); iter != print_jobs_.end(); ++iter) {
    if (iter->get() == job) {
      found = true;
      break;
    }
  }

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
      for (auto iter = print_jobs_.begin(); iter != print_jobs_.end(); ++iter) {
        if (iter->get() == job) {
          print_jobs_.erase(iter);
          break;
        }
      }
      break;
    default:
      break;
  }

  base::SequencedTaskRunnerHandle::Get()->PostNonNestableDelayedTask(
      FROM_HERE,
      base::BindOnce(&FakeCupsPrintJobManager::ChangePrintJobState,
                     weak_ptr_factory_.GetWeakPtr(), job),
      base::TimeDelta::FromMilliseconds(3000));
}

// static
CupsPrintJobManager* CupsPrintJobManager::CreateInstance(Profile* profile) {
  return new FakeCupsPrintJobManager(profile);
}

}  // namespace chromeos
