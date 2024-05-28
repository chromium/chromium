// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_FILEAPI_RECENT_DISK_SOURCE_H_
#define CHROME_BROWSER_ASH_FILEAPI_RECENT_DISK_SOURCE_H_

#include <memory>
#include <string>
#include <vector>

#include "base/containers/id_map.h"
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
  RecentDiskSource(
      extensions::api::file_manager_private::VolumeType volume_type,
      std::string mount_point_name,
      bool ignore_dotfiles,
      int max_depth,
      std::string uma_histogram_name);

  RecentDiskSource(const RecentDiskSource&) = delete;
  RecentDiskSource& operator=(const RecentDiskSource&) = delete;

  ~RecentDiskSource() override;

  // RecentSource overrides:
  void GetRecentFiles(const Params& params,
                      GetRecentFilesCallback callback) override;

  // Stops the recent files search. Returns any partial results already
  // collected.
  std::vector<RecentFile> Stop(const int32_t call_id) override;

  // Helper function that determines a match between file type inferred from the
  // path and the desired file_type.
  static bool MatchesFileType(const base::FilePath& path,
                              RecentSource::FileType file_type);

 private:
  FRIEND_TEST_ALL_PREFIXES(RecentDiskSourceTest, GetRecentFiles_UmaStats);

  static const char kLoadHistogramName[];

  void ScanDirectory(const int32_t call_id,
                     const base::FilePath& path,
                     int depth);
  void OnReadDirectory(const int32_t call_id,
                       const base::FilePath& path,
                       int depth,
                       base::File::Error result,
                       storage::FileSystemOperation::FileEntryList entries,
                       bool has_more);
  void OnGotMetadata(const int32_t call_id,
                     const storage::FileSystemURL& url,
                     base::File::Error result,
                     const base::File::Info& info);
  void OnReadOrStatFinished(int32_t call_id);

  storage::FileSystemURL BuildDiskURL(const Params& params,
                                      const base::FilePath& path) const;

  const std::string mount_point_name_;
  const bool ignore_dotfiles_;
  const int max_depth_;
  const std::string uma_histogram_name_;

  // CallContext gather information for a single GetRecentFiles call. As
  // GetRecentFiles call can take time, and some data is collected on IO thread,
  // we cannot guarantee that two calls will not overlap. To solve this each
  // call receives a unique call_id and its context is stored in the map. As the
  // map is only accessed on the UI thread we do not need to use additional
  // locks to guarantee its consistency.
  struct CallContext {
    CallContext(const Params& params, GetRecentFilesCallback callback);
    // Move constructor; necessary as callback is a move-only type.
    CallContext(CallContext&& context);

    ~CallContext();

    // The parameters of the GetRecentFiles call.
    const Params params;

    // The callback called when the files and their metadata is ready.
    GetRecentFilesCallback callback;
    // Time when the build started.
    base::TimeTicks build_start_time;
    // Number of ReadDirectory() calls in flight.
    int inflight_readdirs = 0;
    // Number of GetMetadata() calls in flight.
    int inflight_stats = 0;
    // Most recently modified files.
    FileAccumulator accumulator;
  };

  // A map from call_id to the context of the call.
  base::IDMap<std::unique_ptr<CallContext>> context_map_;

  base::WeakPtrFactory<RecentDiskSource> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_FILEAPI_RECENT_DISK_SOURCE_H_
