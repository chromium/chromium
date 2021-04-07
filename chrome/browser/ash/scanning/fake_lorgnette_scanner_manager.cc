// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/scanning/fake_lorgnette_scanner_manager.h"

#include <utility>

#include "base/bind.h"
#include "base/location.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"

namespace ash {

FakeLorgnetteScannerManager::FakeLorgnetteScannerManager() = default;

FakeLorgnetteScannerManager::~FakeLorgnetteScannerManager() = default;

void FakeLorgnetteScannerManager::GetScannerNames(
    GetScannerNamesCallback callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), scanner_names_));
}

void FakeLorgnetteScannerManager::GetScannerCapabilities(
    const std::string& scanner_name,
    GetScannerCapabilitiesCallback callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), scanner_capabilities_));
}

void FakeLorgnetteScannerManager::Scan(const std::string& scanner_name,
                                       const lorgnette::ScanSettings& settings,
                                       ProgressCallback progress_callback,
                                       PageCallback page_callback,
                                       CompletionCallback completion_callback) {
  if (scan_data_.has_value()) {
    uint32_t page_number = 1;
    for (const std::string& page_data : scan_data_.value()) {
      if (progress_callback) {
        for (const uint32_t progress : {7, 22, 40, 42, 59, 74, 95}) {
          base::ThreadTaskRunnerHandle::Get()->PostTask(
              FROM_HERE,
              base::BindOnce(progress_callback, progress, page_number));
        }
      }

      base::ThreadTaskRunnerHandle::Get()->PostTask(
          FROM_HERE, base::BindOnce(page_callback, page_data, page_number++));
    }
  }

  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(completion_callback), scan_data_.has_value(),
                     scan_data_.has_value()
                         ? lorgnette::SCAN_FAILURE_MODE_NO_FAILURE
                         : lorgnette::SCAN_FAILURE_MODE_DEVICE_BUSY));
}

void FakeLorgnetteScannerManager::CancelScan(CancelCallback cancel_callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(cancel_callback), true));
}

void FakeLorgnetteScannerManager::SetGetScannerNamesResponse(
    const std::vector<std::string>& scanner_names) {
  scanner_names_ = scanner_names;
}

void FakeLorgnetteScannerManager::SetGetScannerCapabilitiesResponse(
    const base::Optional<lorgnette::ScannerCapabilities>&
        scanner_capabilities) {
  scanner_capabilities_ = scanner_capabilities;
}

void FakeLorgnetteScannerManager::SetScanResponse(
    const base::Optional<std::vector<std::string>>& scan_data) {
  scan_data_ = scan_data;
}

}  // namespace ash
