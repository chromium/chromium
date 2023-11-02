// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_CHROME_CLEANER_CHROME_CLEANER_SCANNER_RESULTS_WIN_H_
#define CHROME_BROWSER_SAFE_BROWSING_CHROME_CLEANER_CHROME_CLEANER_SCANNER_RESULTS_WIN_H_

#include <set>
#include <string>

#include "base/files/file_path.h"

namespace safe_browsing {

// Represents scanner results sent by the Chrome Cleanup tool via the Chrome
// Prompt IPC, that needs to be propaged to visualization components.
class ChromeCleanerScannerResults {
 public:
  using FileCollection = std::set<base::FilePath>;
  using RegistryKeyCollection = std::set<std::wstring>;

  ChromeCleanerScannerResults();
  ChromeCleanerScannerResults(const FileCollection& files_to_delete,
                              const RegistryKeyCollection& registry_keys);
  ChromeCleanerScannerResults(const ChromeCleanerScannerResults& other);
  ~ChromeCleanerScannerResults();

  ChromeCleanerScannerResults& operator=(
      const ChromeCleanerScannerResults& other);

  const FileCollection& files_to_delete() const { return files_to_delete_; }
  const RegistryKeyCollection& registry_keys() const { return registry_keys_; }

 private:
  FileCollection files_to_delete_;
  RegistryKeyCollection registry_keys_;
};

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_CHROME_CLEANER_CHROME_CLEANER_SCANNER_RESULTS_WIN_H_
