// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/printing/test_cups_wrapper.h"

#include <utility>

#include "base/functional/callback.h"

namespace chromeos {

TestCupsWrapper::TestCupsWrapper() = default;

TestCupsWrapper::~TestCupsWrapper() = default;

void TestCupsWrapper::QueryCupsPrintJobs(
    const std::vector<std::string>& printer_ids,
    base::OnceCallback<void(std::unique_ptr<QueryResult>)> callback) {
  auto result = std::make_unique<CupsWrapper::QueryResult>();
  result->success = false;
  std::move(callback).Run(std::move(result));
}

void TestCupsWrapper::CancelJob(const std::string& printer_id, int job_id) {}

void TestCupsWrapper::QueryCupsPrinterStatus(
    const std::string& printer_id,
    base::OnceCallback<void(std::unique_ptr<::printing::PrinterStatus>)>
        callback) {
  auto result = std::make_unique<::printing::PrinterStatus>();
  auto it = printer_reasons_.find(printer_id);
  if (it != printer_reasons_.end())
    result->reasons.push_back(it->second);
  std::move(callback).Run(std::move(result));
}

void TestCupsWrapper::SetPrinterStatus(
    const std::string& printer_id,
    const ::printing::PrinterStatus::PrinterReason& printer_reason) {
  printer_reasons_[printer_id] = printer_reason;
}

}  // namespace chromeos
