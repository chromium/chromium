// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_PRINTING_HISTORY_PRINT_JOB_HISTORY_CLEANER_H_
#define CHROME_BROWSER_ASH_PRINTING_HISTORY_PRINT_JOB_HISTORY_CLEANER_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "chrome/browser/ash/printing/history/print_job_database.h"

class PrefService;

namespace base {
class Clock;
}  // namespace base

namespace ash {

class PrintJobHistoryCleaner {
 public:
  // The default amount of time the metadata of completed print job is stored on
  // the device.
  static constexpr int kDefaultPrintJobHistoryExpirationPeriodDays = 90;

  PrintJobHistoryCleaner(PrintJobDatabase* print_job_database,
                         PrefService* pref_service);

  PrintJobHistoryCleaner(const PrintJobHistoryCleaner&) = delete;
  PrintJobHistoryCleaner& operator=(const PrintJobHistoryCleaner&) = delete;

  ~PrintJobHistoryCleaner();

  // Removes expired print jobs from the database.
  // The expiration period is controlled by
  // |prefs::kPrintJobHistoryExpirationPeriod| pref.
  // |callback| is called after all expired print jobs are removed from the
  // database.
  void CleanUp(base::OnceClosure callback);

  void SetClockForTesting(const base::Clock* clock);

 private:
  void OnPrefServiceInitialized(base::OnceClosure callback, bool success);
  void OnPrintJobsRetrieved(
      base::OnceClosure callback,
      bool success,
      std::vector<printing::proto::PrintJobInfo> print_job_infos);
  void OnPrintJobsDeleted(base::OnceClosure callback, bool success);

  // This object is owned by PrintJobHistoryService and outlives
  // PrintJobHistoryCleaner.
  raw_ptr<PrintJobDatabase> print_job_database_;

  raw_ptr<PrefService> pref_service_;

  // Points to the base::DefaultClock by default.
  raw_ptr<const base::Clock> clock_;

  // Stores the completion time of the oldest print job in the database or the
  // time of last cleanup.
  // This is used to determine whether we need to run real cleanup or not.
  base::Time oldest_print_job_completion_time_;

  base::WeakPtrFactory<PrintJobHistoryCleaner> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_PRINTING_HISTORY_PRINT_JOB_HISTORY_CLEANER_H_
