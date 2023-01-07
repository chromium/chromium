// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_PRINTING_HISTORY_PRINT_JOB_HISTORY_SERVICE_H_
#define CHROME_BROWSER_ASH_PRINTING_HISTORY_PRINT_JOB_HISTORY_SERVICE_H_

#include "base/observer_list.h"
#include "chrome/browser/ash/printing/history/print_job_database.h"
#include "components/keyed_service/core/keyed_service.h"

class PrefRegistrySimple;

namespace ash {

// This service is responsible for maintaining print job history.
class PrintJobHistoryService : public KeyedService {
 public:
  class Observer {
   public:
    virtual void OnPrintJobFinished(
        const printing::proto::PrintJobInfo& print_job_info) = 0;

    virtual void OnShutdown() {}

   protected:
    virtual ~Observer() = default;
  };

  PrintJobHistoryService();

  PrintJobHistoryService(const PrintJobHistoryService&) = delete;
  PrintJobHistoryService& operator=(const PrintJobHistoryService&) = delete;

  ~PrintJobHistoryService() override;

  // Register the print job history preferences with the |registry|.
  static void RegisterProfilePrefs(PrefRegistrySimple* registry);

  // Retrieves all print jobs from the database.
  virtual void GetPrintJobs(
      PrintJobDatabase::GetPrintJobsCallback callback) = 0;

  // Deletes all print jobs from the database.
  virtual void DeleteAllPrintJobs(
      PrintJobDatabase::DeletePrintJobsCallback callback) = 0;

  void AddObserver(PrintJobHistoryService::Observer* observer);
  void RemoveObserver(PrintJobHistoryService::Observer* observer);

  // KeyedService:
  void Shutdown() override;

 protected:
  base::ObserverList<PrintJobHistoryService::Observer>::Unchecked observers_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_PRINTING_HISTORY_PRINT_JOB_HISTORY_SERVICE_H_
