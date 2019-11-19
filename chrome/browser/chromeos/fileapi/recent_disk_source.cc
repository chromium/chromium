// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/fileapi/recent_disk_source.h"

#include <utility>

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/string_util.h"
#include "base/task/post_task.h"
#include "base/time/time.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "storage/browser/file_system/external_mount_points.h"
#include "storage/browser/file_system/file_system_context.h"
#include "storage/browser/file_system/file_system_operation.h"
#include "storage/browser/file_system/file_system_operation_runner.h"
#include "storage/browser/file_system/file_system_url.h"

using content::BrowserThread;

namespace chromeos {

namespace {

void OnReadDirectoryOnIOThread(
    const storage::FileSystemOperation::ReadDirectoryCallback& callback,
    base::File::Error result,
    storage::FileSystemOperation::FileEntryList entries,
    bool has_more) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  base::PostTask(
      FROM_HERE, {BrowserThread::UI},
      base::BindOnce(callback, result, std::move(entries), has_more));
}

void ReadDirectoryOnIOThread(
    scoped_refptr<storage::FileSystemContext> file_system_context,
    const storage::FileSystemURL& url,
    const storage::FileSystemOperation::ReadDirectoryCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  file_system_context->operation_runner()->ReadDirectory(
      url, base::BindRepeating(&OnReadDirectoryOnIOThread, callback));
}

void OnGetMetadataOnIOThread(
    storage::FileSystemOperation::GetMetadataCallback callback,
    base::File::Error result,
    const base::File::Info& info) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  base::PostTask(FROM_HERE, {BrowserThread::UI},
                 base::BindOnce(std::move(callback), result, info));
}

void GetMetadataOnIOThread(
    scoped_refptr<storage::FileSystemContext> file_system_context,
    const storage::FileSystemURL& url,
    int fields,
    storage::FileSystemOperation::GetMetadataCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  file_system_context->operation_runner()->GetMetadata(
      url, fields,
      base::BindOnce(&OnGetMetadataOnIOThread, std::move(callback)));
}

}  // namespace

RecentDiskSource::RecentDiskSource(std::string mount_point_name,
                                   bool ignore_dotfiles,
                                   int max_depth,
                                   std::string uma_histogram_name)
    : mount_point_name_(std::move(mount_point_name)),
      ignore_dotfiles_(ignore_dotfiles),
      max_depth_(max_depth),
      uma_histogram_name_(std::move(uma_histogram_name)) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
}

RecentDiskSource::~RecentDiskSource() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
}

void RecentDiskSource::GetRecentFiles(Params params) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(!params_.has_value());
  DCHECK(build_start_time_.is_null());
  DCHECK_EQ(0, inflight_readdirs_);
  DCHECK_EQ(0, inflight_stats_);
  DCHECK(recent_files_.empty());

  // Return immediately if mount point does not exist.
  storage::ExternalMountPoints* mount_points =
      storage::ExternalMountPoints::GetSystemInstance();
  base::FilePath path;
  if (!mount_points->GetRegisteredPath(mount_point_name_, &path)) {
    std::move(params.callback()).Run({});
    return;
  }

  params_.emplace(std::move(params));

  DCHECK(params_.has_value());

  build_start_time_ = base::TimeTicks::Now();

  ScanDirectory(base::FilePath(), 1);
}

void RecentDiskSource::ScanDirectory(const base::FilePath& path, int depth) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(params_.has_value());

  storage::FileSystemURL url = BuildDiskURL(path);

  ++inflight_readdirs_;
  base::PostTask(
      FROM_HERE, {BrowserThread::IO},
      base::BindOnce(
          &ReadDirectoryOnIOThread,
          base::WrapRefCounted(params_.value().file_system_context()), url,
          base::BindRepeating(&RecentDiskSource::OnReadDirectory,
                              weak_ptr_factory_.GetWeakPtr(), path, depth)));
}

void RecentDiskSource::OnReadDirectory(
    const base::FilePath& path,
    const int depth,
    base::File::Error result,
    storage::FileSystemOperation::FileEntryList entries,
    bool has_more) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(params_.has_value());

  for (const auto& entry : entries) {
    // Ignore directories and files that start with dot.
    if (ignore_dotfiles_ &&
        base::StartsWith(entry.name.value(), ".",
                         base::CompareCase::INSENSITIVE_ASCII)) {
      continue;
    }
    base::FilePath subpath = path.Append(entry.name);

    if (entry.type == filesystem::mojom::FsFileType::DIRECTORY) {
      if (max_depth_ > 0 && depth >= max_depth_) {
        continue;
      }
      ScanDirectory(subpath, depth + 1);
    } else {
      storage::FileSystemURL url = BuildDiskURL(subpath);
      ++inflight_stats_;
      base::PostTask(
          FROM_HERE, {BrowserThread::IO},
          base::BindOnce(
              &GetMetadataOnIOThread,
              base::WrapRefCounted(params_.value().file_system_context()), url,
              storage::FileSystemOperation::GET_METADATA_FIELD_LAST_MODIFIED,
              base::BindOnce(&RecentDiskSource::OnGetMetadata,
                             weak_ptr_factory_.GetWeakPtr(), url)));
    }
  }

  if (has_more)
    return;

  --inflight_readdirs_;
  OnReadOrStatFinished();
}

void RecentDiskSource::OnGetMetadata(const storage::FileSystemURL& url,
                                     base::File::Error result,
                                     const base::File::Info& info) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(params_.has_value());

  if (result == base::File::FILE_OK &&
      info.last_modified >= params_.value().cutoff_time()) {
    recent_files_.emplace(RecentFile(url, info.last_modified));
    while (recent_files_.size() > params_.value().max_files())
      recent_files_.pop();
  }

  --inflight_stats_;
  OnReadOrStatFinished();
}

void RecentDiskSource::OnReadOrStatFinished() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (inflight_readdirs_ > 0 || inflight_stats_ > 0)
    return;

  // All reads/scans completed.
  std::vector<RecentFile> files;
  while (!recent_files_.empty()) {
    files.emplace_back(recent_files_.top());
    recent_files_.pop();
  }

  DCHECK(!build_start_time_.is_null());
  UmaHistogramTimes(uma_histogram_name_,
                    base::TimeTicks::Now() - build_start_time_);
  build_start_time_ = base::TimeTicks();

  Params params = std::move(params_.value());
  params_.reset();

  DCHECK(!params_.has_value());
  DCHECK(build_start_time_.is_null());
  DCHECK_EQ(0, inflight_readdirs_);
  DCHECK_EQ(0, inflight_stats_);
  DCHECK(recent_files_.empty());

  std::move(params.callback()).Run(std::move(files));
}

storage::FileSystemURL RecentDiskSource::BuildDiskURL(
    const base::FilePath& path) const {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(params_.has_value());

  storage::ExternalMountPoints* mount_points =
      storage::ExternalMountPoints::GetSystemInstance();
  return mount_points->CreateExternalFileSystemURL(params_.value().origin(),
                                                   mount_point_name_, path);
}

}  // namespace chromeos
