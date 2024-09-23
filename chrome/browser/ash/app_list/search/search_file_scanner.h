// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_APP_LIST_SEARCH_SEARCH_FILE_SCANNER_H_
#define CHROME_BROWSER_ASH_APP_LIST_SEARCH_SEARCH_FILE_SCANNER_H_

#include "base/files/file_path.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"

class Profile;

namespace app_list {

// This class scans the user's local file. This is used for collecting data
// about user's local file distribution and estimating the power and performance
// of the new launcher search features.
// The scan only collect metrics, and does not have any data stored for any
// other purpose.
// TODO(b/337130427) considering moving the metrics recording to
// `ImageAnnotationWorker` when image search is launched, or deprecate this
// class if it is no longer needed.
class SearchFileScanner {
 public:
  SearchFileScanner(
      Profile* profile,
      const base::FilePath& root_path,
      const std::vector<base::FilePath>& excluded_paths,
      std::optional<base::TimeDelta> start_delay_override = std::nullopt);
  ~SearchFileScanner();

  SearchFileScanner(const SearchFileScanner&) = delete;
  SearchFileScanner& operator=(const SearchFileScanner&) = delete;

 private:
  // Starts file scan on non-UI thread.
  void StartFileScan();

  // Updates the last scan log time when the file scan is complete.
  void OnScanComplete();

  const raw_ptr<Profile> profile_;
  const base::FilePath root_path_;
  const std::vector<base::FilePath> excluded_paths_;

  base::WeakPtrFactory<SearchFileScanner> weak_factory_{this};
};

}  // namespace app_list

#endif  // CHROME_BROWSER_ASH_APP_LIST_SEARCH_SEARCH_FILE_SCANNER_H_
