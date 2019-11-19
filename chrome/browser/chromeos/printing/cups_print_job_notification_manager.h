// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_PRINTING_CUPS_PRINT_JOB_NOTIFICATION_MANAGER_H_
#define CHROME_BROWSER_CHROMEOS_PRINTING_CUPS_PRINT_JOB_NOTIFICATION_MANAGER_H_

#include <memory>
#include <unordered_map>

#include "base/macros.h"
#include "chrome/browser/chromeos/printing/cups_print_job_manager.h"

class Profile;

namespace chromeos {

class CupsPrintJob;
class CupsPrintJobNotification;

class CupsPrintJobNotificationManager : public CupsPrintJobManager::Observer {
 public:
  using PrintJobNotificationMap =
      std::unordered_map<CupsPrintJob*,
                         std::unique_ptr<CupsPrintJobNotification>>;

  CupsPrintJobNotificationManager(Profile* profile,
                                  CupsPrintJobManager* print_job_manager);
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

 private:
  void UpdateNotification(base::WeakPtr<CupsPrintJob> job);

  PrintJobNotificationMap notification_map_;
  CupsPrintJobManager* print_job_manager_;
  Profile* profile_;

  DISALLOW_COPY_AND_ASSIGN(CupsPrintJobNotificationManager);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_PRINTING_CUPS_PRINT_JOB_NOTIFICATION_MANAGER_H_
