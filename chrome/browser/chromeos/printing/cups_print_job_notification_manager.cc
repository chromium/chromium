// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/printing/cups_print_job_notification_manager.h"

#include "chrome/browser/chromeos/printing/cups_print_job.h"
#include "chrome/browser/chromeos/printing/cups_print_job_manager.h"
#include "chrome/browser/chromeos/printing/cups_print_job_notification.h"
#include "chrome/browser/profiles/profile.h"

namespace chromeos {

CupsPrintJobNotificationManager::CupsPrintJobNotificationManager(
    Profile* profile,
    CupsPrintJobManager* print_job_manager)
    : print_job_manager_(print_job_manager), profile_(profile) {
  DCHECK(print_job_manager_);
  print_job_manager_->AddObserver(this);
}

CupsPrintJobNotificationManager::~CupsPrintJobNotificationManager() {
  DCHECK(print_job_manager_);
  print_job_manager_->RemoveObserver(this);
}

void CupsPrintJobNotificationManager::OnPrintJobCreated(
    base::WeakPtr<CupsPrintJob> job) {
  if (!job)
    return;
  if (base::Contains(notification_map_, job.get()))
    return;
  notification_map_[job.get()] =
      std::make_unique<CupsPrintJobNotification>(this, job, profile_);
}

void CupsPrintJobNotificationManager::OnPrintJobStarted(
    base::WeakPtr<CupsPrintJob> job) {
  UpdateNotification(job);
}

void CupsPrintJobNotificationManager::OnPrintJobUpdated(
    base::WeakPtr<CupsPrintJob> job) {
  UpdateNotification(job);
}

void CupsPrintJobNotificationManager::OnPrintJobSuspended(
    base::WeakPtr<CupsPrintJob> job) {
  UpdateNotification(job);
}

void CupsPrintJobNotificationManager::OnPrintJobResumed(
    base::WeakPtr<CupsPrintJob> job) {
  UpdateNotification(job);
}

void CupsPrintJobNotificationManager::OnPrintJobDone(
    base::WeakPtr<CupsPrintJob> job) {
  UpdateNotification(job);
}

void CupsPrintJobNotificationManager::OnPrintJobError(
    base::WeakPtr<CupsPrintJob> job) {
  UpdateNotification(job);
}

void CupsPrintJobNotificationManager::OnPrintJobCancelled(
    base::WeakPtr<CupsPrintJob> job) {
  UpdateNotification(job);
}

void CupsPrintJobNotificationManager::OnPrintJobNotificationRemoved(
    CupsPrintJobNotification* notification) {
  // |notification|.print_job_ might be a nullptr at this moment, so we iterate
  // through |notification_map_| to find |notification|.
  auto it = notification_map_.begin();
  for (; it != notification_map_.end(); it++) {
    if (it->second.get() == notification)
      break;
  }

  if (it != notification_map_.end())
    notification_map_.erase(it);
}

void CupsPrintJobNotificationManager::UpdateNotification(
    base::WeakPtr<CupsPrintJob> job) {
  if (!job)
    return;
  DCHECK(base::Contains(notification_map_, job.get()));
  notification_map_[job.get()]->OnPrintJobStatusUpdated();
}

}  // namespace chromeos
