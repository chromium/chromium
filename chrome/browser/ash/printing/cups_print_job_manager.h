// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_PRINTING_CUPS_PRINT_JOB_MANAGER_H_
#define CHROME_BROWSER_ASH_PRINTING_CUPS_PRINT_JOB_MANAGER_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "base/time/time.h"
#include "chrome/browser/ash/printing/history/print_job_info.pb.h"
#include "chrome/browser/printing/print_job.h"
#include "components/keyed_service/core/keyed_service.h"

class Profile;

namespace crosapi {
class TestControllerAsh;
}  // namespace crosapi

namespace ash {

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
  CupsPrintJobManager(const CupsPrintJobManager&) = delete;
  CupsPrintJobManager& operator=(const CupsPrintJobManager&) = delete;
  ~CupsPrintJobManager() override;

  // KeyedService override:
  void Shutdown() override;

  // Add a CUPS print job to the print job management application and monitor
  // it for completion. The print job must already have been sent to CUPS
  // before calling this function.
  virtual bool CreatePrintJob(
      const std::string& printer_id,
      const std::string& title,
      uint32_t job_id,
      int total_page_number,
      ::printing::PrintJob::Source source,
      const std::string& source_id,
      const printing::proto::PrintSettings& settings) = 0;

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

  raw_ptr<Profile, DanglingUntriaged> profile_;

 private:
  friend class crosapi::TestControllerAsh;
  void RecordJobDuration(base::WeakPtr<CupsPrintJob> job);

  base::ObserverList<Observer> observers_;
  std::unique_ptr<CupsPrintJobNotificationManager> notification_manager_;

  // Keyed by CupsPrintJob's unique ID
  std::map<std::string, base::TimeTicks> print_job_start_times_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_PRINTING_CUPS_PRINT_JOB_MANAGER_H_
