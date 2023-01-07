// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/printing/history/test_print_job_database.h"

#include "base/functional/callback.h"
#include "chrome/browser/ash/printing/history/print_job_info.pb.h"

namespace ash {

using printing::proto::PrintJobInfo;

TestPrintJobDatabase::TestPrintJobDatabase() = default;

TestPrintJobDatabase::~TestPrintJobDatabase() = default;

void TestPrintJobDatabase::Initialize(InitializeCallback callback) {
  std::move(callback).Run(true);
}

bool TestPrintJobDatabase::IsInitialized() {
  return true;
}

void TestPrintJobDatabase::SavePrintJob(const PrintJobInfo& print_job_info,
                                        SavePrintJobCallback callback) {
  database_[print_job_info.id()] = print_job_info;
  std::move(callback).Run(true);
}

void TestPrintJobDatabase::DeletePrintJobs(const std::vector<std::string>& ids,
                                           DeletePrintJobsCallback callback) {
  for (const std::string& id : ids)
    database_.erase(id);
  std::move(callback).Run(true);
}

void TestPrintJobDatabase::Clear(DeletePrintJobsCallback callback) {
  database_.clear();
  std::move(callback).Run(/*success=*/true);
}

void TestPrintJobDatabase::GetPrintJobs(GetPrintJobsCallback callback) {
  std::vector<PrintJobInfo> entries;
  entries.reserve(database_.size());
  for (const auto& pair : database_)
    entries.push_back(pair.second);
  std::move(callback).Run(true, std::move(entries));
}

}  // namespace ash
