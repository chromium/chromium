// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_PRINTING_HISTORY_PRINT_JOB_DATABASE_H_
#define CHROME_BROWSER_ASH_PRINTING_HISTORY_PRINT_JOB_DATABASE_H_

#include <memory>
#include <string>
#include <vector>

#include "base/functional/callback_forward.h"

namespace ash {
namespace printing::proto {
class PrintJobInfo;
}  // namespace printing::proto

class PrintJobDatabase {
 public:
  using InitializeCallback = base::OnceCallback<void(bool success)>;

  using SavePrintJobCallback = base::OnceCallback<void(bool success)>;

  using DeletePrintJobsCallback = base::OnceCallback<void(bool success)>;

  using GetPrintJobsCallback = base::OnceCallback<
      void(bool success, std::vector<printing::proto::PrintJobInfo> entries)>;

  using GetPrintJobsFromProtoDatabaseCallback = base::OnceCallback<void(
      bool success,
      std::unique_ptr<std::vector<printing::proto::PrintJobInfo>> entries)>;

  virtual ~PrintJobDatabase() = default;

  // Initializes this database asynchronously.
  // Note that the callback will be called only after the call returned. It may
  // also still be called after PrintJobDatabase was destructed.
  virtual void Initialize(InitializeCallback callback) = 0;

  // Returns whether the database is initialized.
  virtual bool IsInitialized() = 0;

  // Saves given print job in the storage.
  // Note that the callback will be called only after the call returned. It may
  // also still be called after PrintJobDatabase was destructed.
  virtual void SavePrintJob(const printing::proto::PrintJobInfo& print_job_info,
                            SavePrintJobCallback callback) = 0;

  // Removes the print jobs associated with given |ids| from the storage.
  // Note that the callback will be called only after the call returned. It may
  // also still be called after PrintJobDatabase was destructed.
  virtual void DeletePrintJobs(const std::vector<std::string>& ids,
                               DeletePrintJobsCallback callback) = 0;

  // Removes all the print jobs from the storage.
  // Note that the callback will be called only after the call returned. It may
  // also still be called after PrintJobDatabase was destructed.
  virtual void Clear(DeletePrintJobsCallback callback) = 0;

  // Retrieves all print jobs from the storage.
  // Note that the callback will be called only after the call returned. It may
  // also still be called after PrintJobDatabase was destructed.
  virtual void GetPrintJobs(GetPrintJobsCallback callback) = 0;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_PRINTING_HISTORY_PRINT_JOB_DATABASE_H_
