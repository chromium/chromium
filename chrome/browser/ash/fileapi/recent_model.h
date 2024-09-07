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
#include "chrome/common/extensions/api/file_manager_private.h"
#include "components/keyed_service/core/keyed_service.h"
#include "storage/browser/file_system/file_system_url.h"

class GURL;
class Profile;

namespace storage {

class FileSystemContext;

}  // namespace storage

namespace ash {

// The specifications of conditions on recent sources. Only volumes of the given
// type are searched for recent files.
struct RecentSourceSpec {
  // The type of volume that is to be scanned.
  extensions::api::file_manager_private::VolumeType volume_type;
};

// The options that impact how the search for recent files is carried out. The
// default values for the options is to look for files that were modified in the
// last 30 days, with no limit to how long the scan can take, returning any type
// of files, but no more than 1000. If possible files will be returned from the
// recent cache.
//
// It is critical that you specify the source_list to be all sources that you
// wish to search. By default the source list is empty, meaning that nothing
// is searched.
struct RecentModelOptions {
  RecentModelOptions();
  ~RecentModelOptions();

  // How far back to accept files.
  base::TimeDelta now_delta = base::Days(30);

  // The maximum time the scan for recent files can take. Sources that do
  // not complete before the timeout do not contribute to returned results.
  base::TimeDelta scan_timeout = base::TimeDelta::Max();

  // The maximum number of files to be returned.
  size_t max_files = 1000u;

  // Whether or not to invalidate the cache; if this flag is true, even if
  // there are cached results, they are not returned. Instead a full scan
  // of sources is performed.
  bool invalidate_cache = false;

  // The type of files to be returned.
  RecentSource::FileType file_type = RecentSource::FileType::kAll;

  // A vector of recent sources specifications. Only sources matching the
  // specification are going to be used when retrieving recent files. This field
  // must be non-empty.
  std::vector<RecentSourceSpec> source_specs;
};

// Implements a service that returns files matching a given query, type, with
// the given modification date. A typical use is shown below:
//
// RecentModel* model = RecentModelFactory::GetForProfile(user_profile);
// GetRecentFilesCallback callback = base:Bind(...);
// FileSystemContext* context = GetFileSystemContextForRenderFrameHost(
//     user_profile, render_frame_host());
//
// // Requests files from local disk, with names containing "foo", modified
// // within the last 30 days (default), classified as image (jpg, png, etc.),
// // without deleting cache from the previous call (default).
// ash::RecentModelOptions options;
// options.file_type = ash::RecentSource::FileType::kImage
// options.source_spec = {
//   { .volume_type = VolumeType::kDownloads },
// };
// model->GetRecentFiles(
//     context,
//     GURL("chrome://file-manager/"),
//     "foo",
//     options,
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
    // The maximum number of files to  be returned.
    size_t max_files;
    // The type of files accepted, e.g., images, documents, etc.
    FileType file_type;

    bool operator==(const SearchCriteria& other) const = default;
  };

  using GetRecentFilesCallback =
      base::OnceCallback<void(const std::vector<RecentFile>& files)>;

  explicit RecentModel(Profile* profile);
  ~RecentModel() override;

  RecentModel(const RecentModel&) = delete;
  RecentModel& operator=(const RecentModel&) = delete;

  // Creates an instance with given sources. Only for testing.
  static std::unique_ptr<RecentModel> CreateForTest(
      std::vector<std::unique_ptr<RecentSource>> sources);

  // Returns a list of recent files by querying sources.
  // Files are sorted by descending order of last modified time.
  // Results might be internally cached for better performance.
  void GetRecentFiles(storage::FileSystemContext* file_system_context,
                      const GURL& origin,
                      const std::string& query,
                      const RecentModelOptions& options,
                      GetRecentFilesCallback callback);

  // KeyedService overrides:
  void Shutdown() override;

 private:
  explicit RecentModel(std::vector<std::unique_ptr<RecentSource>> sources);

  // Context for a single GetRecentFiles call.
  struct CallContext {
    CallContext(const SearchCriteria& search_criteria,
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

  // If set, limits the length of time the GetRecentFiles method can take before
  // returning results, if any, in the callback.
  std::optional<base::TimeDelta> scan_timeout_duration_;

  base::WeakPtrFactory<RecentModel> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_FILEAPI_RECENT_MODEL_H_
