// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_PRINTING_CUPS_PRINT_JOB_NOTIFICATION_MANAGER_H_
#define CHROME_BROWSER_ASH_PRINTING_CUPS_PRINT_JOB_NOTIFICATION_MANAGER_H_

#include <memory>
#include <unordered_map>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ash/printing/cups_print_job_manager.h"

class Profile;

namespace ash {

class CupsPrintJob;
class CupsPrintJobNotification;

class CupsPrintJobNotificationManager : public CupsPrintJobManager::Observer {
 public:
  using PrintJobNotificationMap =
      std::unordered_map<CupsPrintJob*,
                         std::unique_ptr<CupsPrintJobNotification>>;

  CupsPrintJobNotificationManager(Profile* profile,
                                  CupsPrintJobManager* print_job_manager);
  CupsPrintJobNotificationManager(const CupsPrintJobNotificationManager&) =
      delete;
  CupsPrintJobNotificationManager& operator=(
      const CupsPrintJobNotificationManager&) = delete;
  ~CupsPrintJobNotificationManager() override;

  // CupsPrintJobManager::Observer overrides:
  void OnPrintJobCreated(base::WeakPtr<CupsPrintJob> job) override;
  void OnPrintJobStarted(base::WeakPtr<CupsPrintJob> job) override;
  void OnPrintJobUpdated(base::WeakPtr<CupsPrintJob> job) override;
  void OnPrintJobSuspended(base::WeakPtr<CupsPrintJob> job) override;
  void OnPrintJobResumed(base::WeakPtr<CupsPrintJob> job) override;
  void OnPrintJobDone(base::WeakPtr<CupsPrintJob> job) override;
  void OnPrintJobError(base::WeakPtr<CupsPrintJob> job) override;
  void OnPrintJobCancelled(base::WeakPtr<CupsPrintJob> job) override;

  void OnPrintJobNotificationRemoved(CupsPrintJobNotification* notification);

  // Return the notification for the given print job, or nullptr if not found.
  CupsPrintJobNotification* GetNotificationForTesting(CupsPrintJob* job);

 private:
  void UpdateNotification(base::WeakPtr<CupsPrintJob> job);

  PrintJobNotificationMap notification_map_;
  raw_ptr<CupsPrintJobManager> print_job_manager_;
  raw_ptr<Profile, DanglingUntriaged> profile_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_PRINTING_CUPS_PRINT_JOB_NOTIFICATION_MANAGER_H_
