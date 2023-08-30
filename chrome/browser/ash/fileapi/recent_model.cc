// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/fileapi/recent_model.h"

#include <algorithm>
#include <iterator>
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

// Cut-off time. Files older than this are filtered out.
constexpr base::TimeDelta kCutoffTimeDelta = base::Days(30);

// Recent file cache will be cleared this duration after it is built.
// Note: Do not make this value large. When cache is used, cut-off criteria is
// not strictly honored.
constexpr base::TimeDelta kCacheExpiration = base::Seconds(10);

std::vector<std::unique_ptr<RecentSource>> CreateDefaultSources(
    Profile* profile) {
  std::vector<std::unique_ptr<RecentSource>> sources;
  sources.emplace_back(std::make_unique<RecentArcMediaSource>(profile));
  // Crostini.
  sources.emplace_back(std::make_unique<RecentDiskSource>(
      file_manager::util::GetCrostiniMountPointName(profile),
      true /* ignore_dotfiles */, 4 /* max_depth */,
      "FileBrowser.Recent.LoadCrostini"));
  // Downloads / MyFiles.
  sources.emplace_back(std::make_unique<RecentDiskSource>(
      file_manager::util::GetDownloadsMountPointName(profile),
      true /* ignore_dotfiles */, 0 /* max_depth unlimited */,
      "FileBrowser.Recent.LoadDownloads"));
  sources.emplace_back(std::make_unique<RecentDriveSource>(profile));

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
          /*ignore_dot_files=*/true, /*max_depth=*/0,
          "FileBrowser.Recent.LoadFileSystemProvider"));
    }
  }

  return sources;
}

}  // namespace

const char RecentModel::kLoadHistogramName[] = "FileBrowser.Recent.LoadTotal";

// static
RecentModel* RecentModel::GetForProfile(Profile* profile) {
  return RecentModelFactory::GetForProfile(profile);
}

// static
std::unique_ptr<RecentModel> RecentModel::CreateForTest(
    std::vector<std::unique_ptr<RecentSource>> sources) {
  return base::WrapUnique(new RecentModel(std::move(sources)));
}

RecentModel::RecentModel(Profile* profile)
    : RecentModel(CreateDefaultSources(profile)) {}

RecentModel::RecentModel(std::vector<std::unique_ptr<RecentSource>> sources)
    : sources_(std::move(sources)), current_sequence_id_(0) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
}

RecentModel::~RecentModel() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(sources_.empty());
}

void RecentModel::GetRecentFiles(
    storage::FileSystemContext* file_system_context,
    const GURL& origin,
    FileType file_type,
    bool invalidate_cache,
    GetRecentFilesCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  /**
   * Use cache only if:
   *  * cache has value.
   *  * invalidate_cache = false.
   *  * cached file type matches the query file type.
   * Otherwise clear cache if it has values.
   */
  if (cached_files_.has_value()) {
    if (!invalidate_cache && cached_files_type_ == file_type) {
      std::move(callback).Run(cached_files_.value());
      return;
    }
    cached_files_.reset();
  }

  bool builder_already_running = !pending_callbacks_.empty();
  pending_callbacks_.emplace_back(std::move(callback));

  // If a builder is already running, just enqueue the callback and return.
  if (builder_already_running)
    return;

  // Start building a recent file list.
  DCHECK_EQ(0, num_inflight_sources_);
  DCHECK(intermediate_files_.empty());
  DCHECK(build_start_time_.is_null());

  build_start_time_ = base::TimeTicks::Now();

  num_inflight_sources_ = sources_.size();
  if (sources_.empty()) {
    OnGetRecentFilesCompleted(file_type);
    return;
  }

  base::Time cutoff_time = forced_cutoff_time_.has_value()
                               ? forced_cutoff_time_.value()
                               : base::Time::Now() - kCutoffTimeDelta;

  uint32_t run_on_sequence_id = current_sequence_id_;

  for (const auto& source : sources_) {
    source->GetRecentFiles(RecentSource::Params(
        file_system_context, origin, max_files_, cutoff_time, file_type,
        base::BindOnce(&RecentModel::OnGetRecentFiles,
                       weak_ptr_factory_.GetWeakPtr(), run_on_sequence_id,
                       max_files_, cutoff_time, file_type)));
  }
  if (scan_timeout_duration_) {
    deadline_timer_.Start(
        FROM_HERE, base::TimeTicks::Now() + *scan_timeout_duration_,
        base::BindOnce(&RecentModel::OnScanTimeout,
                       weak_ptr_factory_.GetWeakPtr(), file_type));
  }
}

void RecentModel::SetScanTimeout(const base::TimeDelta& delta) {
  scan_timeout_duration_ = delta;
}

void RecentModel::ClearScanTimeout() {
  scan_timeout_duration_.reset();
}

void RecentModel::OnScanTimeout(FileType file_type) {
  if (num_inflight_sources_ > 0) {
    num_inflight_sources_ = 0;
    OnGetRecentFilesCompleted(file_type);
  }
}

void RecentModel::Shutdown() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // Some RecentSource implementations have references to other KeyedServices,
  // so we destruct them here.
  sources_.clear();
}

void RecentModel::OnGetRecentFiles(uint32_t run_on_sequence_id,
                                   size_t max_files,
                                   const base::Time& cutoff_time,
                                   FileType file_type,
                                   std::vector<RecentFile> files) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (run_on_sequence_id != current_sequence_id_) {
    // This source replied too late. We are no longer accepting any recent
    // files for this call. The supplied files are ignored.
    DCHECK(!deadline_timer_.IsRunning());
    return;
  }

  for (const auto& file : files) {
    if (file.last_modified() >= cutoff_time)
      intermediate_files_.emplace(file);
  }

  while (intermediate_files_.size() > max_files)
    intermediate_files_.pop();

  --num_inflight_sources_;
  if (num_inflight_sources_ == 0)
    OnGetRecentFilesCompleted(file_type);
}

void RecentModel::OnGetRecentFilesCompleted(FileType file_type) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  DCHECK_EQ(0, num_inflight_sources_);
  DCHECK(!cached_files_.has_value());
  DCHECK(!build_start_time_.is_null());

  ++current_sequence_id_;
  deadline_timer_.Stop();

  std::vector<RecentFile> files;
  while (!intermediate_files_.empty()) {
    files.emplace_back(intermediate_files_.top());
    intermediate_files_.pop();
  }
  std::reverse(files.begin(), files.end());
  cached_files_ = std::move(files);
  cached_files_type_ = file_type;

  DCHECK(cached_files_.has_value());
  DCHECK(intermediate_files_.empty());

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
  for (auto& callback : callbacks_to_call)
    std::move(callback).Run(cached_files_.value());
}

void RecentModel::ClearCache() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  cached_files_.reset();
}

void RecentModel::SetMaxFilesForTest(size_t max_files) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  max_files_ = max_files;
}

void RecentModel::SetForcedCutoffTimeForTest(
    const base::Time& forced_cutoff_time) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  forced_cutoff_time_ = forced_cutoff_time;
}

}  // namespace ash
