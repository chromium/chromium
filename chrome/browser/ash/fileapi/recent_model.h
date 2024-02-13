// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_FILEAPI_RECENT_MODEL_H_
#define CHROME_BROWSER_ASH_FILEAPI_RECENT_MODEL_H_

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/containers/id_map.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chrome/browser/ash/fileapi/file_accumulator.h"
#include "chrome/browser/ash/fileapi/recent_file.h"
#include "chrome/browser/ash/fileapi/recent_source.h"
#include "components/keyed_service/core/keyed_service.h"
#include "storage/browser/file_system/file_system_url.h"

class GURL;
class Profile;

namespace storage {

class FileSystemContext;

}  // namespace storage

namespace ash {

// Implements a service that returns files matching a given query, type, with
// the given modification date. A typical use is shown below:
//
// RecentModel* model = RecentModelFactory::GetForProfile(user_profile);
// GetRecentFilesCallback callback = base:Bind(...);
// FileSystemContext* context = GetFileSystemContextForRenderFrameHost(
//     user_profile, render_frame_host());
//
// // Requests files with names containing "foo", modified within the last
// // 30 days, classified as image (jpg, png, etc.), without deleting cache
// // from the previous call.
// model->GetRecentFiles(
//     context,
//     GURL("chrome://file-manager/"),
//     "foobar",
//     base::Days(30),
//     ash::RecentModel::FileType::kImage,
//     false,
//     std::move(callback));
//
// In addition to the above flow, one can set the maximum duration for the
// GetRecentCall to take. Once that maximum duration is reached, whatever
// partial results are available, are returned.
//
// All member functions must be called on the UI thread.  However, this class
// supports multiple calls with varying parameters being issued in parallel.
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

  // Context for a single GetRecentFiles call.
  struct CallContext {
    CallContext(size_t max_files,
                const SearchCriteria& search_criteria,
                GetRecentFilesCallback callback);
    CallContext(CallContext&& context);
    ~CallContext();

    // The parameters of the last query. These are used to check if the
    // cached content can be re-used.
    SearchCriteria search_criteria;

    // The callback to call once the results are collected.
    GetRecentFilesCallback callback;

    // Time when the build started.
    base::TimeTicks build_start_time;

    // The accumulator of files found by various recent sources.
    FileAccumulator accumulator;

    // The set of recent sources processing the current request.
    std::set<raw_ptr<RecentSource>> active_sources;
  };

  // The method called by each of the recent source workers, once they complete
  // their task. This method monitors the number of calls and once it is equal
  // to the number of started recent source workers, it calls
  // OnSearchCompleted method.
  void OnGotRecentFiles(RecentSource* source,
                        const base::Time& cutoff_time,
                        const int32_t call_id,
                        std::vector<RecentFile> files);

  // This method is called by OnGetRecentFiles once all started recent source
  // workers complete their tasks.
  void OnSearchCompleted(const int32_t call_id);

  void ClearCache();

  // The callback invoked by the deadline timer.
  void OnScanTimeout(const base::Time& cutoff_time, const int32_t call_id);

  // A map that stores a context for each call to GetRecentFiles. The context
  // exists only from the start of the call until it is completed or times out.
  base::IDMap<std::unique_ptr<CallContext>> context_map_;

  // A map from call ID to a timer that terminates the call.
  base::IDMap<std::unique_ptr<base::DeadlineTimer>> deadline_map_;

  // All known recent sources.
  std::vector<std::unique_ptr<RecentSource>> sources_;

  // Cached GetRecentFiles() response.
  std::optional<std::vector<RecentFile>> cached_files_ = std::nullopt;

  // The parameters of the last query. These are used to check if the
  // cached content can be re-used.
  SearchCriteria cached_search_criteria_;

  // Timer to clear the cache.
  base::OneShotTimer cache_clear_timer_;

  // The counter used to enumerate GetRecentFiles calls. This is used to stop
  // calls that take too long.
  int32_t call_id_ = 0;

  // The maximum files to be returned by a single GetRecentFiles call.
  const size_t max_files_;

  // If set, limits the length of time the GetRecentFiles method can take before
  // returning results, if any, in the callback.
  std::optional<base::TimeDelta> scan_timeout_duration_;

  base::WeakPtrFactory<RecentModel> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_FILEAPI_RECENT_MODEL_H_
