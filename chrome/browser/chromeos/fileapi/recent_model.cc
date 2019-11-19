// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/fileapi/recent_model.h"

#include <algorithm>
#include <iterator>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "chrome/browser/chromeos/file_manager/path_util.h"
#include "chrome/browser/chromeos/fileapi/recent_arc_media_source.h"
#include "chrome/browser/chromeos/fileapi/recent_disk_source.h"
#include "chrome/browser/chromeos/fileapi/recent_drive_source.h"
#include "chrome/browser/chromeos/fileapi/recent_file.h"
#include "chrome/browser/chromeos/fileapi/recent_model_factory.h"
#include "content/public/browser/browser_thread.h"
#include "storage/browser/file_system/file_system_context.h"

using content::BrowserThread;

namespace chromeos {

namespace {

// Cut-off time. Files older than this are filtered out.
constexpr base::TimeDelta kCutoffTimeDelta = base::TimeDelta::FromDays(30);

// Recent file cache will be cleared this duration after it is built.
// Note: Do not make this value large. When cache is used, cut-off criteria is
// not strictly honored.
constexpr base::TimeDelta kCacheExpiration = base::TimeDelta::FromSeconds(10);

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
      false /* ignore_dotfiles */, 0 /* max_depth unlimited */,
      "FileBrowser.Recent.LoadDownloads"));
  sources.emplace_back(std::make_unique<RecentDriveSource>(profile));
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
    : sources_(std::move(sources)) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
}

RecentModel::~RecentModel() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(sources_.empty());
}

void RecentModel::GetRecentFiles(
    storage::FileSystemContext* file_system_context,
    const GURL& origin,
    GetRecentFilesCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // Use cache if available.
  if (cached_files_.has_value()) {
    std::move(callback).Run(cached_files_.value());
    return;
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
    OnGetRecentFilesCompleted();
    return;
  }

  base::Time cutoff_time = forced_cutoff_time_.has_value()
                               ? forced_cutoff_time_.value()
                               : base::Time::Now() - kCutoffTimeDelta;

  for (const auto& source : sources_) {
    source->GetRecentFiles(RecentSource::Params(
        file_system_context, origin, max_files_, cutoff_time,
        base::BindOnce(&RecentModel::OnGetRecentFiles,
                       weak_ptr_factory_.GetWeakPtr(), max_files_,
                       cutoff_time)));
  }
}

void RecentModel::Shutdown() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // Some RecentSource implementations have references to other KeyedServices,
  // so we destruct them here.
  sources_.clear();
}

void RecentModel::OnGetRecentFiles(size_t max_files,
                                   const base::Time& cutoff_time,
                                   std::vector<RecentFile> files) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  DCHECK_LT(0, num_inflight_sources_);

  for (const auto& file : files) {
    if (file.last_modified() >= cutoff_time)
      intermediate_files_.emplace(file);
  }

  while (intermediate_files_.size() > max_files)
    intermediate_files_.pop();

  --num_inflight_sources_;
  if (num_inflight_sources_ == 0)
    OnGetRecentFilesCompleted();
}

void RecentModel::OnGetRecentFilesCompleted() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  DCHECK_EQ(0, num_inflight_sources_);
  DCHECK(!cached_files_.has_value());
  DCHECK(!build_start_time_.is_null());

  std::vector<RecentFile> files;
  while (!intermediate_files_.empty()) {
    files.emplace_back(intermediate_files_.top());
    intermediate_files_.pop();
  }
  std::reverse(files.begin(), files.end());
  cached_files_ = std::move(files);

  DCHECK(cached_files_.has_value());
  DCHECK(intermediate_files_.empty());

  UMA_HISTOGRAM_TIMES(kLoadHistogramName,
                      base::TimeTicks::Now() - build_start_time_);
  build_start_time_ = base::TimeTicks();

  // Starts a timer to clear cache.
  cache_clear_timer_.Start(
      FROM_HERE, kCacheExpiration,
      base::Bind(&RecentModel::ClearCache, weak_ptr_factory_.GetWeakPtr()));

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

}  // namespace chromeos
