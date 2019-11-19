// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_PRINTING_CUPS_PRINT_JOB_MANAGER_H_
#define CHROME_BROWSER_CHROMEOS_PRINTING_CUPS_PRINT_JOB_MANAGER_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/observer_list.h"
#include "base/time/time.h"
#include "components/keyed_service/core/keyed_service.h"

class Profile;

namespace chromeos {

class CupsPrintJob;
class CupsPrintJobNotificationManager;

class CupsPrintJobManager : public KeyedService {
 public:
  class Observer : public base::CheckedObserver {
   public:
    virtual void OnPrintJobCreated(base::WeakPtr<CupsPrintJob> job) {}
    virtual void OnPrintJobStarted(base::WeakPtr<CupsPrintJob> job) {}
    virtual void OnPrintJobUpdated(base::WeakPtr<CupsPrintJob> job) {}
    virtual void OnPrintJobSuspended(base::WeakPtr<CupsPrintJob> job) {}
    virtual void OnPrintJobResumed(base::WeakPtr<CupsPrintJob> job) {}

    // Handle print job completion.  Note, |job| will be deleted after
    // notification is complete.
    virtual void OnPrintJobDone(base::WeakPtr<CupsPrintJob> job) {}

    // Handle print job error.  Note, |job| will be deleted after
    // notification is complete.
    virtual void OnPrintJobError(base::WeakPtr<CupsPrintJob> job) {}

    // Handle print job cancellation.  Note, |job| will be deleted after
    // notification is complete.
    virtual void OnPrintJobCancelled(base::WeakPtr<CupsPrintJob> job) {}

   protected:
    ~Observer() override {}
  };

  static CupsPrintJobManager* CreateInstance(Profile* profile);

  explicit CupsPrintJobManager(Profile* profile);
  ~CupsPrintJobManager() override;

  // KeyedService override:
  void Shutdown() override;

  // Cancel a print job |job|. Note the |job| will be deleted after cancelled.
  // There will be no notifications after cancellation.
  virtual void CancelPrintJob(CupsPrintJob* job) = 0;
  virtual bool SuspendPrintJob(CupsPrintJob* job) = 0;
  virtual bool ResumePrintJob(CupsPrintJob* job) = 0;

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

 protected:
  void NotifyJobCreated(base::WeakPtr<CupsPrintJob> job);
  void NotifyJobStarted(base::WeakPtr<CupsPrintJob> job);
  void NotifyJobUpdated(base::WeakPtr<CupsPrintJob> job);
  void NotifyJobResumed(base::WeakPtr<CupsPrintJob> job);
  void NotifyJobSuspended(base::WeakPtr<CupsPrintJob> job);
  void NotifyJobCanceled(base::WeakPtr<CupsPrintJob> job);
  void NotifyJobFailed(base::WeakPtr<CupsPrintJob> job);
  void NotifyJobDone(base::WeakPtr<CupsPrintJob> job);

  Profile* profile_;

 private:
  void RecordJobDuration(base::WeakPtr<CupsPrintJob> job);

  std::unique_ptr<CupsPrintJobNotificationManager> notification_manager_;
  base::ObserverList<Observer> observers_;

  // Keyed by CupsPrintJob's unique ID
  std::map<std::string, base::TimeTicks> print_job_start_times_;

  DISALLOW_COPY_AND_ASSIGN(CupsPrintJobManager);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_PRINTING_CUPS_PRINT_JOB_MANAGER_H_
