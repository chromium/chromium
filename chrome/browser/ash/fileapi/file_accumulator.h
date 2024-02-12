// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_FILEAPI_FILE_ACCUMULATOR_H_
#define CHROME_BROWSER_ASH_FILEAPI_FILE_ACCUMULATOR_H_

#include <vector>

#include "chrome/browser/ash/fileapi/recent_file.h"

namespace ash {

// Accumulator of files located via search operation. The accumulator has a
// limited capacity. Files are sorted based on the RecentFileComparator class.
// If one adds n > max_capacity files, (n - max_capacity) files are discarded
// based on the order given by the comparator class.
//
// Typical use consists of adding a number of files, then Get'ing the content.
// Once the content is Get'ed, the accumulator is sealed, meaning no new files
// may be added to the accumulator. To unseal the accumulator, call the Clear
// method on it, which also removes all stored files.
//
//   FilesAccumulator<RecentFilesComparator> acc(100);
//   acc.Add(recent_file_1);
//   ..
//   acc.Add(recent_file_n);
//   std::vector<RecentFile> content = acc.Get();
class FileAccumulator {
 public:
  // Creates an accumulator with the given capacity. The capacity
  // limits the maximum number of files that can be added via the Add method.
  explicit FileAccumulator(size_t max_capacity);
  FileAccumulator(FileAccumulator&& accumulator);

  ~FileAccumulator();

  // Adds a single file to the accumulator. The return value indicates if the
  // file has been added or not. A file may not be added if the accumulator is
  // sealed.
  bool Add(const RecentFile& file);

  // Returns the content of this accumulator. The first time this method is
  // called it "seals" this accumulator, re-orders the files from a heap to a
  // simple vector. This method can be called multiple times.
  const std::vector<RecentFile>& Get();

  // Clears the accumulator and unseals it.
  void Clear();

  // Returns the maximum number of recent files that are can be stored in this
  // cache.
  size_t max_capacity() const { return max_capacity_; }

 private:
  // The maximum number of recent files kept in this cache.
  const size_t max_capacity_;
  // Whether or not the accumulator is in the sealed state.
  bool sealed_;
  // The content of the cache, kept sorted by the modified time.
  std::vector<RecentFile> files_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_FILEAPI_FILE_ACCUMULATOR_H_
