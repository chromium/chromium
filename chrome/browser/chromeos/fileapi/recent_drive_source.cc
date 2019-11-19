// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/fileapi/recent_drive_source.h"

#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/metrics/histogram_macros.h"
#include "base/task/post_task.h"
#include "chrome/browser/chromeos/drive/drive_integration_service.h"
#include "chrome/browser/chromeos/drive/file_system_util.h"
#include "chrome/browser/chromeos/file_manager/fileapi_util.h"
#include "chrome/browser/chromeos/fileapi/recent_file.h"
#include "chromeos/components/drivefs/drivefs_util.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "storage/browser/file_system/file_system_operation.h"
#include "storage/browser/file_system/file_system_operation_runner.h"
#include "storage/browser/file_system/file_system_url.h"
#include "storage/common/file_system/file_system_types.h"

using content::BrowserThread;

namespace chromeos {

const char RecentDriveSource::kLoadHistogramName[] =
    "FileBrowser.Recent.LoadDrive";

RecentDriveSource::RecentDriveSource(Profile* profile) : profile_(profile) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
}

RecentDriveSource::~RecentDriveSource() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
}

void RecentDriveSource::GetRecentFiles(Params params) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(!params_.has_value());
  DCHECK(files_.empty());
  DCHECK(build_start_time_.is_null());

  params_.emplace(std::move(params));

  build_start_time_ = base::TimeTicks::Now();

  auto* integration_service =
      drive::util::GetIntegrationServiceByProfile(profile_);
  if (!integration_service) {
    // |integration_service| is nullptr if Drive is disabled.
    OnComplete();
    return;
  }

  auto query_params = drivefs::mojom::QueryParameters::New();
  query_params->page_size = params_->max_files();
  query_params->query_source =
      drivefs::mojom::QueryParameters::QuerySource::kLocalOnly;
  query_params->sort_field =
      drivefs::mojom::QueryParameters::SortField::kLastModified;
  query_params->sort_direction =
      drivefs::mojom::QueryParameters::SortDirection::kDescending;
  integration_service->GetDriveFsInterface()->StartSearchQuery(
      search_query_.BindNewPipeAndPassReceiver(), std::move(query_params));
  search_query_->GetNextPage(base::BindOnce(
      &RecentDriveSource::GotSearchResults, weak_ptr_factory_.GetWeakPtr()));
}

void RecentDriveSource::OnComplete() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(params_.has_value());
  DCHECK(!build_start_time_.is_null());

  UMA_HISTOGRAM_TIMES(kLoadHistogramName,
                      base::TimeTicks::Now() - build_start_time_);
  build_start_time_ = base::TimeTicks();

  Params params = std::move(params_.value());
  params_.reset();
  std::vector<RecentFile> files = std::move(files_);
  files_.clear();

  DCHECK(!params_.has_value());
  DCHECK(files_.empty());
  DCHECK(build_start_time_.is_null());

  std::move(params.callback()).Run(std::move(files));
}

void RecentDriveSource::GotSearchResults(
    drive::FileError error,
    base::Optional<std::vector<drivefs::mojom::QueryItemPtr>> results) {
  search_query_.reset();
  auto* integration_service =
      drive::util::GetIntegrationServiceByProfile(profile_);
  if (!results || !integration_service) {
    OnComplete();
    return;
  }

  files_.reserve(results->size());
  for (auto& result : *results) {
    if (!drivefs::IsAFile(result->metadata->type)) {
      continue;
    }
    base::FilePath path = integration_service->GetMountPointPath().BaseName();
    if (!base::FilePath("/").AppendRelativePath(result->path, &path)) {
      path = path.Append(result->path);
    }
    files_.emplace_back(
        params_.value().file_system_context()->CreateCrackedFileSystemURL(
            params_->origin(), storage::kFileSystemTypeExternal, path),
        result->metadata->last_viewed_by_me_time);
  }
  OnComplete();
}

}  // namespace chromeos
