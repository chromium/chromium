// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_FILEAPI_RECENT_DISK_SOURCE_H_
#define CHROME_BROWSER_CHROMEOS_FILEAPI_RECENT_DISK_SOURCE_H_

#include <memory>
#include <queue>
#include <string>
#include <vector>

#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/time/time.h"
#include "chrome/browser/chromeos/fileapi/recent_file.h"
#include "chrome/browser/chromeos/fileapi/recent_model.h"
#include "chrome/browser/chromeos/fileapi/recent_source.h"
#include "storage/browser/file_system/file_system_operation.h"

namespace chromeos {

// RecentSource implementation for local disks.
// Used for Downloads and fuse-based Crostini.
//
// All member functions must be called on the UI thread.
class RecentDiskSource : public RecentSource {
 public:
  // Create a RecentDiskSource for the volume registered to |mount_point_name|.
  // Does nothing if no volume is registered at |mount_point_name|.
  // If |ignore_dotfiles| is true, recents will ignore directories and files
  // starting with a dot.  Set |max_depth| to zero for unlimited depth.
  RecentDiskSource(std::string mount_point_name,
                   bool ignore_dotfiles,
                   int max_depth,
                   std::string uma_histogram_name);
  ~RecentDiskSource() override;

  // RecentSource overrides:
  void GetRecentFiles(Params params) override;

 private:
  FRIEND_TEST_ALL_PREFIXES(RecentDiskSourceTest, GetRecentFiles_UmaStats);

  static const char kLoadHistogramName[];

  void ScanDirectory(const base::FilePath& path, int depth);
  void OnReadDirectory(const base::FilePath& path,
                       int depth,
                       base::File::Error result,
                       storage::FileSystemOperation::FileEntryList entries,
                       bool has_more);
  void OnGetMetadata(const storage::FileSystemURL& url,
                     base::File::Error result,
                     const base::File::Info& info);
  void OnReadOrStatFinished();

  storage::FileSystemURL BuildDiskURL(const base::FilePath& path) const;

  const std::string mount_point_name_;
  const bool ignore_dotfiles_;
  const int max_depth_;
  const std::string uma_histogram_name_;

  // Parameters given to GetRecentFiles().
  base::Optional<Params> params_;

  // Time when the build started.
  base::TimeTicks build_start_time_;
  // Number of ReadDirectory() calls in flight.
  int inflight_readdirs_ = 0;
  // Number of GetMetadata() calls in flight.
  int inflight_stats_ = 0;
  // Most recently modified files.
  std::priority_queue<RecentFile, std::vector<RecentFile>, RecentFileComparator>
      recent_files_;

  base::WeakPtrFactory<RecentDiskSource> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(RecentDiskSource);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_FILEAPI_RECENT_DISK_SOURCE_H_
