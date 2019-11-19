// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/printing/cups_print_job_manager.h"

#include <algorithm>

#include "base/metrics/histogram_macros.h"
#include "chrome/browser/chromeos/printing/cups_print_job.h"
#include "chrome/browser/chromeos/printing/cups_print_job_notification_manager.h"

namespace chromeos {

CupsPrintJobManager::CupsPrintJobManager(Profile* profile) : profile_(profile) {
  notification_manager_ =
      std::make_unique<CupsPrintJobNotificationManager>(profile, this);
}

CupsPrintJobManager::~CupsPrintJobManager() = default;

void CupsPrintJobManager::Shutdown() {}

void CupsPrintJobManager::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void CupsPrintJobManager::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void CupsPrintJobManager::NotifyJobCreated(base::WeakPtr<CupsPrintJob> job) {
  for (Observer& observer : observers_)
    observer.OnPrintJobCreated(job);
}

void CupsPrintJobManager::NotifyJobStarted(base::WeakPtr<CupsPrintJob> job) {
  DCHECK(job);
  print_job_start_times_[job->GetUniqueId()] = base::TimeTicks::Now();

  for (Observer& observer : observers_)
    observer.OnPrintJobStarted(job);
}

void CupsPrintJobManager::NotifyJobUpdated(base::WeakPtr<CupsPrintJob> job) {
  for (Observer& observer : observers_)
    observer.OnPrintJobUpdated(job);
}

void CupsPrintJobManager::NotifyJobResumed(base::WeakPtr<CupsPrintJob> job) {
  for (Observer& observer : observers_)
    observer.OnPrintJobResumed(job);
}

void CupsPrintJobManager::NotifyJobSuspended(base::WeakPtr<CupsPrintJob> job) {
  for (Observer& observer : observers_)
    observer.OnPrintJobSuspended(job);
}

void CupsPrintJobManager::NotifyJobCanceled(base::WeakPtr<CupsPrintJob> job) {
  RecordJobDuration(job);

  for (Observer& observer : observers_)
    observer.OnPrintJobCancelled(job);
}

void CupsPrintJobManager::NotifyJobFailed(base::WeakPtr<CupsPrintJob> job) {
  for (Observer& observer : observers_)
    observer.OnPrintJobError(job);
}

void CupsPrintJobManager::NotifyJobDone(base::WeakPtr<CupsPrintJob> job) {
  RecordJobDuration(job);

  for (Observer& observer : observers_)
    observer.OnPrintJobDone(job);
}

// TODO(jschettler): In some instances, Chrome doesn't receive an error state
// from the printer (crbug.com/883966). For that reason, the job duration is
// currently recorded for done and cancelled print jobs without accounting
// for the added time a job may spend in a suspended or error state.
void CupsPrintJobManager::RecordJobDuration(base::WeakPtr<CupsPrintJob> job) {
  DCHECK(job);

  auto it = print_job_start_times_.find(job->GetUniqueId());
  if (it == print_job_start_times_.end())
    return;

  base::TimeDelta duration = base::TimeTicks::Now() - it->second;
  switch (job->state()) {
    case CupsPrintJob::State::STATE_DOCUMENT_DONE:
      UMA_HISTOGRAM_LONG_TIMES_100("Printing.CUPS.JobDuration.JobDone",
                                   duration);
      break;
    case CupsPrintJob::State::STATE_CANCELLED:
      UMA_HISTOGRAM_LONG_TIMES_100("Printing.CUPS.JobDuration.JobCancelled",
                                   duration);
      break;
    default:
      break;
  }
  print_job_start_times_.erase(job->GetUniqueId());
}

}  // namespace chromeos
