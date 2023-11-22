// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_FILEAPI_RECENT_DISK_SOURCE_H_
#define CHROME_BROWSER_ASH_FILEAPI_RECENT_DISK_SOURCE_H_

#include <memory>
#include <string>
#include <vector>

#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/gtest_prod_util.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "chrome/browser/ash/fileapi/file_accumulator.h"
#include "chrome/browser/ash/fileapi/recent_file.h"
#include "chrome/browser/ash/fileapi/recent_model.h"
#include "chrome/browser/ash/fileapi/recent_source.h"
#include "storage/browser/file_system/file_system_operation.h"

namespace ash {

// RecentSource implementation for local disks.
// Used for Downloads and fuse-based Crostini.
//
// All member functions must be called on the UI thread.
class RecentDiskSource : public RecentSource {
 public:
  // Create a RecentDiskSource for the volume registered to `mount_point_name`.
  // Does nothing if no volume is registered at `mount_point_name`.
  // If `ignore_dotfiles` is true, recents will ignore directories and files
  // starting with a dot. Set `max_depth` to zero for unlimited depth.
  // The `max_files` parameter limits the maximum number of files returned on
  // the callback of `params` of GetRecentFiles method.
  RecentDiskSource(std::string mount_point_name,
                   bool ignore_dotfiles,
                   int max_depth,
                   size_t max_files,
                   std::string uma_histogram_name);

  RecentDiskSource(const RecentDiskSource&) = delete;
  RecentDiskSource& operator=(const RecentDiskSource&) = delete;

  ~RecentDiskSource() override;

  // RecentSource overrides:
  void GetRecentFiles(Params params, GetRecentFilesCallback callback) override;

  // Helper function that determines a match between file type inferred from the
  // path and the desired file_type.
  static bool MatchesFileType(const base::FilePath& path,
                              RecentSource::FileType file_type);

 private:
  FRIEND_TEST_ALL_PREFIXES(RecentDiskSourceTest, GetRecentFiles_UmaStats);

  static const char kLoadHistogramName[];

  void ScanDirectory(const Params& params,
                     const base::FilePath& path,
                     int depth);
  void OnReadDirectory(const Params& params,
                       const base::FilePath& path,
                       int depth,
                       base::File::Error result,
                       storage::FileSystemOperation::FileEntryList entries,
                       bool has_more);
  void OnGetMetadata(const base::Time& cutoff_time,
                     const storage::FileSystemURL& url,
                     base::File::Error result,
                     const base::File::Info& info);
  void OnReadOrStatFinished();

  storage::FileSystemURL BuildDiskURL(const Params& params,
                                      const base::FilePath& path) const;

  const std::string mount_point_name_;
  const bool ignore_dotfiles_;
  const int max_depth_;
  const std::string uma_histogram_name_;

  // Time when the build started.
  base::TimeTicks build_start_time_;
  // Number of ReadDirectory() calls in flight.
  int inflight_readdirs_ = 0;
  // Number of GetMetadata() calls in flight.
  int inflight_stats_ = 0;
  // Most recently modified files.
  FileAccumulator accumulator_;
  // The callback called when we are ready.
  GetRecentFilesCallback callback_;

  base::WeakPtrFactory<RecentDiskSource> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_FILEAPI_RECENT_DISK_SOURCE_H_
