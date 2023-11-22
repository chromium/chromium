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
  sources.emplace_back(
      std::make_unique<RecentArcMediaSource>(profile, max_files));
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
    : sources_(std::move(sources)),
      accumulator_(max_files),
      current_sequence_id_(0) {
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

  SearchCriteria search_criteria = {
      .query = query,
      .now_delta = now_delta,
      .file_type = file_type,
  };
  /**
   * Use cache only if:
   *  * cache has value.
   *  * invalidate_cache = false.
   *  * cached file type matches the query file type.
   * Otherwise clear cache if it has values.
   */
  if (cached_files_.has_value()) {
    if (!invalidate_cache && cached_search_criteria_ == search_criteria) {
      std::move(callback).Run(cached_files_.value());
      return;
    }
    cached_files_.reset();
  }

  bool builder_already_running = !pending_callbacks_.empty();
  pending_callbacks_.emplace_back(std::move(callback));

  // If a builder is already running, just enqueue the callback and return.
  if (builder_already_running) {
    return;
  }

  // Start building a recent file list.
  DCHECK_EQ(0, num_inflight_sources_);
  DCHECK(build_start_time_.is_null());

  build_start_time_ = base::TimeTicks::Now();

  num_inflight_sources_ = sources_.size();
  if (sources_.empty()) {
    OnGetRecentFilesCompleted(search_criteria);
    return;
  }

  // cutoff_time is the oldest modified time for a file to be considered recent.
  base::Time cutoff_time = base::Time::Now() - now_delta;

  accumulator_.Clear();
  uint32_t run_on_sequence_id = current_sequence_id_;
  // If there is no scan timeout we set the end_time, i.e., the time by which
  // the scan is supposed to be done, to maximum possible time. In the current
  // code base that is about year 292,471.
  base::TimeTicks end_time =
      scan_timeout_duration_ ? base::TimeTicks::Now() + *scan_timeout_duration_
                             : base::TimeTicks::Max();

  const RecentSource::Params params(file_system_context, origin, query,
                                    cutoff_time, end_time, file_type);
  for (const auto& source : sources_) {
    source->GetRecentFiles(
        params,
        base::BindOnce(&RecentModel::OnGetRecentFiles,
                       weak_ptr_factory_.GetWeakPtr(), run_on_sequence_id,
                       cutoff_time, search_criteria));
  }
  if (scan_timeout_duration_) {
    deadline_timer_.Start(
        FROM_HERE, base::TimeTicks::Now() + *scan_timeout_duration_,
        base::BindOnce(&RecentModel::OnScanTimeout,
                       weak_ptr_factory_.GetWeakPtr(), search_criteria));
  }
}

void RecentModel::SetScanTimeout(const base::TimeDelta& delta) {
  scan_timeout_duration_ = delta;
}

void RecentModel::ClearScanTimeout() {
  scan_timeout_duration_.reset();
}

void RecentModel::OnScanTimeout(const SearchCriteria& search_criteria) {
  if (num_inflight_sources_ > 0) {
    num_inflight_sources_ = 0;
    OnGetRecentFilesCompleted(search_criteria);
  }
}

void RecentModel::Shutdown() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // Some RecentSource implementations have references to other KeyedServices,
  // so we destruct them here.
  sources_.clear();
}

void RecentModel::OnGetRecentFiles(uint32_t run_on_sequence_id,
                                   const base::Time& cutoff_time,
                                   const SearchCriteria& search_criteria,
                                   std::vector<RecentFile> files) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (run_on_sequence_id != current_sequence_id_) {
    // This source replied too late. We are no longer accepting any recent
    // files for this call. The supplied files are ignored.
    DCHECK(!deadline_timer_.IsRunning());
    return;
  }

  for (const auto& file : files) {
    if (file.last_modified() >= cutoff_time) {
      accumulator_.Add(file);
    }
  }

  --num_inflight_sources_;
  if (num_inflight_sources_ == 0) {
    OnGetRecentFilesCompleted(search_criteria);
  }
}

void RecentModel::OnGetRecentFilesCompleted(
    const SearchCriteria& search_criteria) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  DCHECK_EQ(0, num_inflight_sources_);
  DCHECK(!cached_files_.has_value());
  DCHECK(!build_start_time_.is_null());

  ++current_sequence_id_;
  deadline_timer_.Stop();

  cached_files_ = accumulator_.Get();
  cached_search_criteria_ = search_criteria;

  DCHECK(cached_files_.has_value());

  UMA_HISTOGRAM_TIMES(kLoadHistogramName,
                      base::TimeTicks::Now() - build_start_time_);
  build_start_time_ = base::TimeTicks();

  // Starts a timer to clear cache.
  cache_clear_timer_.Start(
      FROM_HERE, kCacheExpiration,
      base::BindOnce(&RecentModel::ClearCache, weak_ptr_factory_.GetWeakPtr()));

  // Invoke all pending callbacks.
  std::vector<GetRecentFilesCallback> callbacks_to_call;
  callbacks_to_call.swap(pending_callbacks_);
  DCHECK(pending_callbacks_.empty());
  DCHECK(!callbacks_to_call.empty());
  for (auto& callback : callbacks_to_call) {
    std::move(callback).Run(accumulator_.Get());
  }
  accumulator_.Clear();
}

void RecentModel::ClearCache() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  cached_files_.reset();
}

}  // namespace ash
