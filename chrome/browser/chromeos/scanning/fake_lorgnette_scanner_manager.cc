// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/scanning/fake_lorgnette_scanner_manager.h"

#include <utility>

#include "base/bind.h"
#include "base/location.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"

namespace chromeos {

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
                                       PageCallback page_callback,
                                       ScanCallback callback) {
  if (scan_data_.has_value()) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(page_callback, scan_data_.value(), /*page_number=*/0));
  }

  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), scan_data_.has_value()));
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
    const base::Optional<std::string>& scan_data) {
  scan_data_ = scan_data;
}

}  // namespace chromeos
