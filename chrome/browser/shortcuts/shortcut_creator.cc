// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/shortcuts/shortcut_creator.h"

#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/metrics/histogram_functions.h"

namespace shortcuts {

void EmitIconStorageCountMetric(const base::FilePath& icon_directory) {
  size_t num_files = 0;
  base::FileEnumerator file_iter(icon_directory, /*recursive=*/false,
                                 base::FileEnumerator::FILES);
  while (!file_iter.Next().empty()) {
    ++num_files;
  }
  // Note: If this metric shows that the icon directory is getting too full,
  // then a new design or cleanup may be required.
  base::UmaHistogramCounts100000("Shortcuts.Icons.StorageCount", num_files);
}

}  // namespace shortcuts
