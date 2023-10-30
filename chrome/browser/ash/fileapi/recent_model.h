// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_FILEAPI_RECENT_MODEL_H_
#define CHROME_BROWSER_ASH_FILEAPI_RECENT_MODEL_H_

#include <memory>
#include <queue>
#include <vector>

#include "base/functional/callback_forward.h"
#include "base/gtest_prod_util.h"
#include "base/memory/weak_ptr.h"
#include "base/synchronization/lock.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chrome/browser/ash/fileapi/recent_file.h"
#include "chrome/browser/ash/fileapi/recent_source.h"
#include "components/keyed_service/core/keyed_service.h"
#include "storage/browser/file_system/file_system_url.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

class GURL;
class Profile;

namespace storage {

class FileSystemContext;

}  // namespace storage

namespace ash {

class RecentModelFactory;

// Provides a list of recently modified files.
//
// All member functions must be called on the UI thread.
class RecentModel : public KeyedService {
 public:
  using GetRecentFilesCallback =
      base::OnceCallback<void(const std::vector<RecentFile>& files)>;
  using FileType = RecentSource::FileType;

  RecentModel(const RecentModel&) = delete;
  RecentModel& operator=(const RecentModel&) = delete;

  ~RecentModel() override;

  // Returns an instance for the given profile.
  static RecentModel* GetForProfile(Profile* profile);

  // Creates an instance with given sources. Only for testing.
  static std::unique_ptr<RecentModel> CreateForTest(
      std::vector<std::unique_ptr<RecentSource>> sources);

  // Returns a list of recent files by querying sources.
  // Files are sorted by descending order of last modified time.
  // Results might be internally cached for better performance.
  void GetRecentFiles(storage::FileSystemContext* file_system_context,
                      const GURL& origin,
                      const std::string& query,
                      FileType file_type,
                      bool invalidate_cache,
                      GetRecentFilesCallback callback);

  // KeyedService overrides:
  void Shutdown() override;

  // Sets the timeout for recent model to return recent files. By default,
  // there is no timeout. However, if one is set, any recent source that does
  // not deliver results before the timeout elapses is ignored.
  void SetScanTimeout(const base::TimeDelta& delta);

  // Clears the timeout by which recent sources must deliver results to have
  // them retunred to the caller of GetRecentFiles.
  void ClearScanTimeout();

 private:
  friend class RecentModelFactory;
  friend class RecentModelTest;
  friend class RecentModelCacheTest;
  FRIEND_TEST_ALL_PREFIXES(RecentModelTest, GetRecentFiles_UmaStats);
  FRIEND_TEST_ALL_PREFIXES(RecentModelCacheTest,
                           GetRecentFiles_InvalidateCache);

  static const char kLoadHistogramName[];

  explicit RecentModel(Profile* profile);
  explicit RecentModel(std::vector<std::unique_ptr<RecentSource>> sources);

  void OnGetRecentFiles(uint32_t run_on_sequence_id,
                        size_t max_files,
                        const base::Time& cutoff_time,
                        const std::string& query,
                        FileType file_type,
                        std::vector<RecentFile> files);
  void OnGetRecentFilesCompleted(const std::string& query, FileType file_type);
  void ClearCache();

  // The callback invoked by the deadline timer.
  void OnScanTimeout(const std::string& query, FileType file_type);

  void SetMaxFilesForTest(size_t max_files);
  void SetForcedCutoffTimeForTest(const base::Time& forced_cutoff_time);

  std::vector<std::unique_ptr<RecentSource>> sources_;

  // The maximum number of files in Recent. This value won't be changed from
  // default except for unit tests.
  size_t max_files_ = 1000;

  // If this is set to non-null, it is used as a cut-off time. Should be used
  // only in unit tests.
  absl::optional<base::Time> forced_cutoff_time_;

  // Cached GetRecentFiles() response.
  absl::optional<std::vector<RecentFile>> cached_files_ = absl::nullopt;

  // The query used in the most recent call.
  std::string cached_query_;

  // File type of the cached GetRecentFiles() response.
  FileType cached_files_type_ = FileType::kAll;

  // Timer to clear the cache.
  base::OneShotTimer cache_clear_timer_;

  // Time when the build started.
  base::TimeTicks build_start_time_;

  // While a recent file list is built, this vector contains callbacks to be
  // invoked with the new list.
  std::vector<GetRecentFilesCallback> pending_callbacks_;

  // Number of in-flight sources building recent file lists.
  int num_inflight_sources_ = 0;

  // Intermediate container of recent files while building a list.
  std::priority_queue<RecentFile, std::vector<RecentFile>, RecentFileComparator>
      intermediate_files_;

  // The deadline timer started when recent files are requested, if
  // scan_timeout_duration_ is set. This timer enforces the maximum time limit
  // the fetching of recent files can take. Once the timer goes off no more
  // results are accepted from any source. Whatever recent files were collected
  // so far are returned to the caller of the GetRecentFiles method.
  base::DeadlineTimer deadline_timer_;

  // If set, limits the length of time the GetRecentFiles method can take before
  // returning results, if any, in the callback.
  absl::optional<base::TimeDelta> scan_timeout_duration_;

  // The monotically increasing sequence number. Used to distinguish between
  // current and timed out calls.
  uint32_t current_sequence_id_ = 0;

  base::WeakPtrFactory<RecentModel> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_FILEAPI_RECENT_MODEL_H_
