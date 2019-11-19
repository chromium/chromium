// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_PRINTING_HISTORY_TEST_PRINT_JOB_DATABASE_H_
#define CHROME_BROWSER_CHROMEOS_PRINTING_HISTORY_TEST_PRINT_JOB_DATABASE_H_

#include <string>
#include <unordered_map>

#include "base/macros.h"
#include "chrome/browser/chromeos/printing/history/print_job_database.h"

namespace chromeos {

class TestPrintJobDatabase : public PrintJobDatabase {
 public:
  TestPrintJobDatabase();
  ~TestPrintJobDatabase() override;

  // PrintJobDatabase:
  void Initialize(InitializeCallback callback) override;
  bool IsInitialized() override;
  void SavePrintJob(const printing::proto::PrintJobInfo& print_job_info,
                    SavePrintJobCallback callback) override;
  void DeletePrintJobs(const std::vector<std::string>& ids,
                       DeletePrintJobsCallback callback) override;
  void GetPrintJobs(GetPrintJobsCallback callback) override;

 private:
  // In-memory database of PrintJobInfo.
  std::unordered_map<std::string, printing::proto::PrintJobInfo> database_;

  DISALLOW_COPY_AND_ASSIGN(TestPrintJobDatabase);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_PRINTING_HISTORY_TEST_PRINT_JOB_DATABASE_H_
