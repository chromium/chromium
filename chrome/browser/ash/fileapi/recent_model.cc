// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/fileapi/recent_model.h"

#include <algorithm>
#include <string>
#include <utility>

#include "ash/constants/ash_features.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/time/time.h"
#include "chrome/browser/ash/arc/fileapi/arc_media_view_util.h"
#include "chrome/browser/ash/file_manager/path_util.h"
#include "chrome/browser/ash/file_manager/volume_manager.h"
#include "chrome/browser/ash/fileapi/recent_arc_media_source.h"
#include "chrome/browser/ash/fileapi/recent_disk_source.h"
#include "chrome/browser/ash/fileapi/recent_drive_source.h"
#include "chrome/browser/ash/fileapi/recent_file.h"
#include "chrome/browser/ash/fileapi/recent_model_factory.h"
#include "content/public/browser/browser_thread.h"
#include "storage/browser/file_system/file_system_context.h"

using content::BrowserThread;

namespace ash {

namespace {

// Helper method that transfers files that qualify, based on the cut-off time
// to the accumulator. Used either when a recent source completes the work or
// when it is stopped.
void TransferFiles(const std::vector<RecentFile>& found_files,
                   const base::Time& cutoff_time,
                   FileAccumulator* accumulator) {
  for (const auto& file : found_files) {
    if (file.last_modified() >= cutoff_time) {
      accumulator->Add(file);
    }
  }
}

// Recent file cache will be cleared this duration after it is built.
// Note: Do not make this value large. When cache is used, cut-off criteria is
// not strictly honored.
constexpr base::TimeDelta kCacheExpiration = base::Seconds(10);

// The default number of files collected from each recent source.
constexpr size_t kMaxFiles = 1000u;

std::vector<std::unique_ptr<RecentSource>> CreateDefaultSources(
    Profile* profile,
    size_t max_files) {
  std::vector<std::unique_ptr<RecentSource>> sources;

  // ARC sources.
  sources.emplace_back(std::make_unique<RecentArcMediaSource>(
      profile, arc::kImagesRootId, max_files));
  sources.emplace_back(std::make_unique<RecentArcMediaSource>(
      profile, arc::kVideosRootId, max_files));
  sources.emplace_back(std::make_unique<RecentArcMediaSource>(
      profile, arc::kDocumentsRootId, max_files));
  // Android's MediaDocumentsProvider.queryRecentDocuments() doesn't support
  // audio files, http://b/175155820. Therefore no arc::kAudioRootId source.

  // Crostini.
  sources.emplace_back(std::make_unique<RecentDiskSource>(
      file_manager::util::GetCrostiniMountPointName(profile),
      true /* ignore_dotfiles */, 4 /* max_depth */, max_files,
      "FileBrowser.Recent.LoadCrostini"));

  // Downloads / MyFiles.
  sources.emplace_back(std::make_unique<RecentDiskSource>(
      file_manager::util::GetDownloadsMountPointName(profile),
      true /* ignore_dotfiles */, 0 /* max_depth unlimited */, max_files,
      "FileBrowser.Recent.LoadDownloads"));
  sources.emplace_back(std::make_unique<RecentDriveSource>(profile, max_files));

  if (base::FeatureList::IsEnabled(ash::features::kFSPsInRecents)) {
    file_manager::VolumeManager* volume_manager =
        file_manager::VolumeManager::Get(profile);
    for (const base::WeakPtr<file_manager::Volume> volume :
         volume_manager->GetVolumeList()) {
      if (!volume || volume->type() != file_manager::VOLUME_TYPE_PROVIDED ||
          volume->file_system_type() == file_manager::util::kFuseBox) {
        // Provided volume types are served via two file system types: fusebox
        // (usable from ash or lacros, but requires ChromeOS' /usr/bin/fusebox
        // daemon process to be running) and non-fusebox (ash only, no separate
        // process required). The Files app runs in ash and could use either.
        // Using both would return duplicate results. We therefore filter out
        // the fusebox file system type.
        continue;
      }
      sources.emplace_back(std::make_unique<RecentDiskSource>(
          volume->mount_path().BaseName().AsUTF8Unsafe(),
          /*ignore_dot_files=*/true, /*max_depth=*/0, max_files,
          "FileBrowser.Recent.LoadFileSystemProvider"));
    }
  }

  return sources;
}

}  // namespace

RecentModel::CallContext::CallContext(size_t max_files,
                                      const SearchCriteria& criteria,
                                      GetRecentFilesCallback callback)
    : search_criteria(criteria),
      callback(std::move(callback)),
      build_start_time(base::TimeTicks::Now()),
      accumulator(max_files) {}

RecentModel::CallContext::CallContext(CallContext&& context)
    : search_criteria(context.search_criteria),
      callback(std::move(context.callback)),
      build_start_time(context.build_start_time),
      accumulator(std::move(context.accumulator)),
      active_sources(std::move(context.active_sources)) {}

RecentModel::CallContext::~CallContext() = default;

// static
std::unique_ptr<RecentModel> RecentModel::CreateForTest(
    std::vector<std::unique_ptr<RecentSource>> sources,
    size_t max_files) {
  return base::WrapUnique(new RecentModel(std::move(sources), max_files));
}

RecentModel::RecentModel(Profile* profile)
    : RecentModel(CreateDefaultSources(profile, kMaxFiles), kMaxFiles) {}

RecentModel::RecentModel(std::vector<std::unique_ptr<RecentSource>> sources,
                         size_t max_files)
    : sources_(std::move(sources)), max_files_(max_files) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
}

RecentModel::~RecentModel() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(sources_.empty());
}

void RecentModel::GetRecentFiles(
    storage::FileSystemContext* file_system_context,
    const GURL& origin,
    const std::string& query,
    const base::TimeDelta& now_delta,
    FileType file_type,
    bool invalidate_cache,
    GetRecentFilesCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  const int32_t this_call_id = ++call_id_;
  SearchCriteria search_criteria = {
      .query = query,
      .now_delta = now_delta,
      .file_type = file_type,
  };

  /**
   * Use cache only if:
   *  * cache has value.
   *  * invalidate_cache = false.
   *  * cached file type matches the query
   * file type. Otherwise clear cache if
   * it has values.
   */
  if (cached_files_.has_value()) {
    if (!invalidate_cache && cached_search_criteria_ == search_criteria) {
      std::move(callback).Run(cached_files_.value());
      return;
    }
    cached_files_.reset();
  }

  auto context = std::make_unique<CallContext>(max_files_, search_criteria,
                                               std::move(callback));
  for (const auto& source : sources_) {
    context->active_sources.insert(source.get());
  }
  context_map_.AddWithID(std::move(context), this_call_id);

  if (sources_.empty()) {
    OnSearchCompleted(this_call_id);
    return;
  }

  // cutoff_time is the oldest modified time for a file to be considered
  // recent.
  base::Time cutoff_time = base::Time::Now() - now_delta;

  if (scan_timeout_duration_) {
    auto timer = std::make_unique<base::DeadlineTimer>();
    base::DeadlineTimer* timer_ptr = timer.get();
    deadline_map_.AddWithID(std::move(timer), this_call_id);
    timer_ptr->Start(FROM_HERE,
                     base::TimeTicks::Now() + *scan_timeout_duration_,
                     base::BindOnce(&RecentModel::OnScanTimeout,
                                    weak_ptr_factory_.GetWeakPtr(), cutoff_time,
                                    this_call_id));
  }

  // If there is no scan timeout we set the end_time, i.e., the time by which
  // the scan is supposed to be done, to maximum possible time. In the current
  // code base that is about year 292,471.
  base::TimeTicks end_time =
      scan_timeout_duration_ ? base::TimeTicks::Now() + *scan_timeout_duration_
                             : base::TimeTicks::Max();

  const RecentSource::Params params(file_system_context, this_call_id, origin,
                                    query, cutoff_time, end_time, file_type);
  for (const auto& source : sources_) {
    source->GetRecentFiles(
        params, base::BindOnce(&RecentModel::OnGotRecentFiles,
                               weak_ptr_factory_.GetWeakPtr(), source.get(),
                               cutoff_time, this_call_id));
  }
}

void RecentModel::SetScanTimeout(const base::TimeDelta& delta) {
  scan_timeout_duration_ = delta;
}

void RecentModel::ClearScanTimeout() {
  scan_timeout_duration_.reset();
}

void RecentModel::OnScanTimeout(const base::Time& cutoff_time,
                                const int32_t call_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  CallContext* context = context_map_.Lookup(call_id);
  if (context == nullptr) {
    return;
  }

  for (RecentSource* source : context->active_sources) {
    TransferFiles(source->Stop(call_id), cutoff_time, &context->accumulator);
  }
  context->active_sources.clear();
  OnSearchCompleted(call_id);
}

void RecentModel::Shutdown() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  context_map_.Clear();
  deadline_map_.Clear();
  // Some RecentSource implementations have references to other
  // KeyedServices, so we destruct them here.
  sources_.clear();
}

void RecentModel::OnGotRecentFiles(RecentSource* source,
                                   const base::Time& cutoff_time,
                                   const int32_t call_id,
                                   std::vector<RecentFile> files) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  CallContext* context = context_map_.Lookup(call_id);
  if (context == nullptr) {
    return;
  }
  TransferFiles(files, cutoff_time, &context->accumulator);
  context->active_sources.erase(source);

  if (context->active_sources.empty()) {
    OnSearchCompleted(call_id);
  }
}

void RecentModel::OnSearchCompleted(const int32_t call_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  CallContext* context = context_map_.Lookup(call_id);
  if (context == nullptr) {
    return;
  }
  deadline_map_.Remove(call_id);

  DCHECK(context->active_sources.empty());
  DCHECK(!context->callback.is_null());
  DCHECK(!context->build_start_time.is_null());

  cached_files_ = context->accumulator.Get();
  cached_search_criteria_ = context->search_criteria;

  DCHECK(cached_files_.has_value());

  UMA_HISTOGRAM_TIMES(kLoadHistogramName,
                      base::TimeTicks::Now() - context->build_start_time);

  // Starts a timer to clear cache.
  cache_clear_timer_.Start(
      FROM_HERE, kCacheExpiration,
      base::BindOnce(&RecentModel::ClearCache, weak_ptr_factory_.GetWeakPtr()));

  std::move(context->callback).Run(context->accumulator.Get());
  context_map_.Remove(call_id);
}

void RecentModel::ClearCache() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  cached_files_.reset();
}

}  // namespace ash
