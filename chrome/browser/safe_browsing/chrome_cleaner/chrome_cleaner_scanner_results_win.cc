// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/chrome_cleaner/chrome_cleaner_scanner_results_win.h"

namespace safe_browsing {

ChromeCleanerScannerResults::ChromeCleanerScannerResults() = default;

ChromeCleanerScannerResults::ChromeCleanerScannerResults(
    const FileCollection& files_to_delete,
    const RegistryKeyCollection& registry_keys)
    : files_to_delete_(files_to_delete), registry_keys_(registry_keys) {}

ChromeCleanerScannerResults::ChromeCleanerScannerResults(
    const ChromeCleanerScannerResults& other)
    : files_to_delete_(other.files_to_delete_),
      registry_keys_(other.registry_keys_) {}

ChromeCleanerScannerResults::~ChromeCleanerScannerResults() = default;

ChromeCleanerScannerResults& ChromeCleanerScannerResults::operator=(
    const ChromeCleanerScannerResults& other) {
  files_to_delete_ = other.files_to_delete_;
  registry_keys_ = other.registry_keys_;
  return *this;
}

}  // namespace safe_browsing
