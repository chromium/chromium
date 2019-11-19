// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/chrome_cleaner/chrome_cleaner_scanner_results_win.h"

namespace safe_browsing {

ChromeCleanerScannerResults::ChromeCleanerScannerResults() = default;

ChromeCleanerScannerResults::ChromeCleanerScannerResults(
    const FileCollection& files_to_delete,
    const RegistryKeyCollection& registry_keys,
    const ExtensionCollection& extension_ids)
    : files_to_delete_(files_to_delete),
      registry_keys_(registry_keys),
      extension_ids_(extension_ids) {}

ChromeCleanerScannerResults::ChromeCleanerScannerResults(
    const ChromeCleanerScannerResults& other)
    : files_to_delete_(other.files_to_delete_),
      registry_keys_(other.registry_keys_),
      extension_ids_(other.extension_ids_) {}

ChromeCleanerScannerResults::~ChromeCleanerScannerResults() = default;

ChromeCleanerScannerResults& ChromeCleanerScannerResults::operator=(
    const ChromeCleanerScannerResults& other) {
  files_to_delete_ = other.files_to_delete_;
  registry_keys_ = other.registry_keys_;
  extension_ids_ = other.extension_ids_;
  return *this;
}

}  // namespace safe_browsing
