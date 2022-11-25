// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_PRINTING_HISTORY_TEST_PRINT_JOB_DATABASE_H_
#define CHROME_BROWSER_ASH_PRINTING_HISTORY_TEST_PRINT_JOB_DATABASE_H_

#include <string>
#include <unordered_map>

#include "chrome/browser/ash/printing/history/print_job_database.h"

namespace ash {

class TestPrintJobDatabase : public PrintJobDatabase {
 public:
  TestPrintJobDatabase();

  TestPrintJobDatabase(const TestPrintJobDatabase&) = delete;
  TestPrintJobDatabase& operator=(const TestPrintJobDatabase&) = delete;

  ~TestPrintJobDatabase() override;

  // PrintJobDatabase:
  void Initialize(InitializeCallback callback) override;
  bool IsInitialized() override;
  void SavePrintJob(const printing::proto::PrintJobInfo& print_job_info,
                    SavePrintJobCallback callback) override;
  void DeletePrintJobs(const std::vector<std::string>& ids,
                       DeletePrintJobsCallback callback) override;
  void Clear(DeletePrintJobsCallback callback) override;
  void GetPrintJobs(GetPrintJobsCallback callback) override;

 private:
  // In-memory database of PrintJobInfo.
  std::unordered_map<std::string, printing::proto::PrintJobInfo> database_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_PRINTING_HISTORY_TEST_PRINT_JOB_DATABASE_H_
