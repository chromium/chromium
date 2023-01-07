// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/scanning/fake_lorgnette_scanner_manager.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/location.h"
#include "base/task/single_thread_task_runner.h"
#include "third_party/re2/src/re2/re2.h"

namespace ash {

namespace {

// A list of Epson models that do not rotate alternating ADF scanned pages
// to be excluded in IsRotateAlternate().
constexpr char kEpsonNoFlipModels[] =
    "\\b("
    "DS-790WN"
    "|LP-M8180A"
    "|LP-M8180F"
    "|LX-10020M"
    "|LX-10050KF"
    "|LX-10050MF"
    "|LX-6050MF"
    "|LX-7550MF"
    "|PX-M7070FX"
    "|PX-M7080FX"
    "|PX-M7090FX"
    "|PX-M7110F"
    "|PX-M7110FP"
    "|PX-M860F"
    "|PX-M880FX"
    "|WF-6530"
    "|WF-6590"
    "|WF-6593"
    "|WF-C20600"
    "|WF-C20600a"
    "|WF-C20600c"
    "|WF-C20750"
    "|WF-C20750a"
    "|WF-C20750c"
    "|WF-C21000"
    "|WF-C21000a"
    "|WF-C21000c"
    "|WF-C579R"
    "|WF-C579Ra"
    "|WF-C8610"
    "|WF-C8690"
    "|WF-C8690a"
    "|WF-C869R"
    "|WF-C869Ra"
    "|WF-C878R"
    "|WF-C878Ra"
    "|WF-C879R"
    "|WF-C879Ra"
    "|WF-M21000"
    "|WF-M21000a"
    "|WF-M21000c"
    ")\\b";

}  // namespace

FakeLorgnetteScannerManager::FakeLorgnetteScannerManager() = default;

FakeLorgnetteScannerManager::~FakeLorgnetteScannerManager() = default;

void FakeLorgnetteScannerManager::GetScannerNames(
    GetScannerNamesCallback callback) {
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), scanner_names_));
}

void FakeLorgnetteScannerManager::GetScannerCapabilities(
    const std::string& scanner_name,
    GetScannerCapabilitiesCallback callback) {
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), scanner_capabilities_));
}

bool FakeLorgnetteScannerManager::IsRotateAlternate(
    const std::string& scanner_name,
    const std::string& source_name) {
  if (!RE2::PartialMatch(source_name, RE2("(?i)adf duplex"))) {
    return false;
  }

  // No implementation of GetUsableDeviceNameAndProtocol() available
  // so assume scanner name is formatted as device_name.
  std::string exclude_regex = std::string("^(airscan|ippusb).*(EPSON\\s+)?") +
                              std::string(kEpsonNoFlipModels);
  if (RE2::PartialMatch(scanner_name, RE2("^(epsonds|epson2)")) ||
      RE2::PartialMatch(scanner_name, RE2(exclude_regex))) {
    return false;
  }

  return RE2::PartialMatch(scanner_name, RE2("(?i)epson"));
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
          base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
              FROM_HERE,
              base::BindOnce(progress_callback, progress, page_number));
        }
      }

      base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, base::BindOnce(page_callback, page_data, page_number++));
    }
  }

  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(completion_callback),
                     scan_data_.has_value()
                         ? lorgnette::SCAN_FAILURE_MODE_NO_FAILURE
                         : lorgnette::SCAN_FAILURE_MODE_DEVICE_BUSY));
}

void FakeLorgnetteScannerManager::CancelScan(CancelCallback cancel_callback) {
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(cancel_callback), true));
}

void FakeLorgnetteScannerManager::SetGetScannerNamesResponse(
    const std::vector<std::string>& scanner_names) {
  scanner_names_ = scanner_names;
}

void FakeLorgnetteScannerManager::SetGetScannerCapabilitiesResponse(
    const absl::optional<lorgnette::ScannerCapabilities>&
        scanner_capabilities) {
  scanner_capabilities_ = scanner_capabilities;
}

void FakeLorgnetteScannerManager::SetScanResponse(
    const absl::optional<std::vector<std::string>>& scan_data) {
  scan_data_ = scan_data;
}

}  // namespace ash
