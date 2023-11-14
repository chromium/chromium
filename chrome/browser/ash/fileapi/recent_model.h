// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_FILEAPI_RECENT_MODEL_H_
#define CHROME_BROWSER_ASH_FILEAPI_RECENT_MODEL_H_

#include <memory>
#include <string>
#include <vector>

#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chrome/browser/ash/fileapi/file_accumulator.h"
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

// Provides a list of recently modified files.
//
// All member functions must be called on the UI thread.
class RecentModel : public KeyedService {
 public:
  // The name of the histogram used to record user metrics about total time
  // it took to fetch recent files.
  static constexpr char kLoadHistogramName[] = "FileBrowser.Recent.LoadTotal";

  using FileType = RecentSource::FileType;

  // Stores all parameters that identify either the current or cached search
  // performed by the recent model.
  struct SearchCriteria {
    // The query used to match against file names, e.g., "my-file".
    std::string query;
    // The maximum age of accepted files measured as a delta from now.
    base::TimeDelta now_delta;
    // The type of files accepted, e.g., images, documents, etc.
    FileType file_type;

    bool operator==(const SearchCriteria& other) const {
      return query == other.query && now_delta == other.now_delta &&
             file_type == other.file_type;
    }
  };

  using GetRecentFilesCallback =
      base::OnceCallback<void(const std::vector<RecentFile>& files)>;

  explicit RecentModel(Profile* profile);
  ~RecentModel() override;

  RecentModel(const RecentModel&) = delete;
  RecentModel& operator=(const RecentModel&) = delete;


  // Creates an instance with given sources. Only for testing.
  static std::unique_ptr<RecentModel> CreateForTest(
      std::vector<std::unique_ptr<RecentSource>> sources,
      size_t max_files);

  // Returns a list of recent files by querying sources.
  // Files are sorted by descending order of last modified time.
  // Results might be internally cached for better performance.
  void GetRecentFiles(storage::FileSystemContext* file_system_context,
                      const GURL& origin,
                      const std::string& query,
                      const base::TimeDelta& now_delta,
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
  explicit RecentModel(std::vector<std::unique_ptr<RecentSource>> sources,
                       size_t max_files);

  // The method called by each of the recent source workers, once they complete
  // their task. This method monitors the number of calls and once it is equal
  // to the number of started recent source workers, it calls
  // OnGetRecentFilesCompleted method.
  void OnGetRecentFiles(uint32_t run_on_sequence_id,
                        const base::Time& cutoff_time,
                        const SearchCriteria& search_criteria,
                        std::vector<RecentFile> files);

  // This method is called by OnGetRecentFiles once all started recent source
  // workers complete their tasks.
  void OnGetRecentFilesCompleted(const SearchCriteria& search_criteria);

  void ClearCache();

  // The callback invoked by the deadline timer.
  void OnScanTimeout(const SearchCriteria& search_criteria);

  std::vector<std::unique_ptr<RecentSource>> sources_;

  // The accumulator of files found by various recent sources.
  FileAccumulator accumulator_;

  // Cached GetRecentFiles() response.
  absl::optional<std::vector<RecentFile>> cached_files_ = absl::nullopt;

  // The parameters of the last query. These are used to check if the
  // cached content can be re-used.
  SearchCriteria cached_search_criteria_;

  // Timer to clear the cache.
  base::OneShotTimer cache_clear_timer_;

  // Time when the build started.
  base::TimeTicks build_start_time_;

  // While a recent file list is built, this vector contains callbacks to be
  // invoked with the new list.
  std::vector<GetRecentFilesCallback> pending_callbacks_;

  // Number of in-flight sources building recent file lists.
  int num_inflight_sources_ = 0;

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
