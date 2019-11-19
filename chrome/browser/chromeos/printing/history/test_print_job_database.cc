// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/printing/history/test_print_job_database.h"

#include "base/callback.h"
#include "chrome/browser/chromeos/printing/history/print_job_info.pb.h"

namespace chromeos {

TestPrintJobDatabase::TestPrintJobDatabase() {}

TestPrintJobDatabase::~TestPrintJobDatabase() {}

void TestPrintJobDatabase::Initialize(InitializeCallback callback) {
  std::move(callback).Run(true);
}

bool TestPrintJobDatabase::IsInitialized() {
  return true;
}

void TestPrintJobDatabase::SavePrintJob(
    const printing::proto::PrintJobInfo& print_job_info,
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

void TestPrintJobDatabase::GetPrintJobs(GetPrintJobsCallback callback) {
  std::unique_ptr<std::vector<printing::proto::PrintJobInfo>> entries(
      new std::vector<printing::proto::PrintJobInfo>());
  for (const auto& pair : database_)
    entries->emplace_back(pair.second);
  std::move(callback).Run(true, std::move(entries));
}

}  // namespace chromeos
