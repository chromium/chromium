// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/fileapi/recent_drive_source.h"

#include <iterator>
#include <utility>
#include <vector>

#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_macros.h"
#include "chrome/browser/ash/drive/drive_integration_service.h"
#include "chrome/browser/ash/drive/file_system_util.h"
#include "chrome/browser/ash/file_manager/fileapi_util.h"
#include "chrome/browser/ash/fileapi/recent_file.h"
#include "chromeos/ash/components/drivefs/drivefs_util.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "recent_drive_source.h"
#include "storage/browser/file_system/file_system_operation.h"
#include "storage/browser/file_system/file_system_operation_runner.h"
#include "storage/browser/file_system/file_system_url.h"
#include "storage/common/file_system/file_system_types.h"
#include "ui/file_manager/file_types_data.h"
#include "url/origin.h"

using content::BrowserThread;

namespace ash {

const char RecentDriveSource::kLoadHistogramName[] =
    "FileBrowser.Recent.LoadDrive";

const char kAudioMimeType[] = "audio";
const char kImageMimeType[] = "image";
const char kVideoMimeType[] = "video";

RecentDriveSource::CallContext::CallContext(GetRecentFilesCallback callback)
    : callback(std::move(callback)), build_start_time(base::TimeTicks::Now()) {}

RecentDriveSource::CallContext::CallContext(CallContext&& context)
    : callback(std::move(context.callback)),
      build_start_time(context.build_start_time),
      files(std::move(context.files)),
      search_query(std::move(context.search_query)) {}

RecentDriveSource::CallContext::~CallContext() = default;

RecentDriveSource::RecentDriveSource(Profile* profile)
    : RecentSource(extensions::api::file_manager_private::VolumeType::kDrive),
      profile_(profile) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
}

RecentDriveSource::~RecentDriveSource() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
}

std::vector<std::string> RecentDriveSource::CreateTypeFilters(
    RecentSource::FileType file_type) {
  std::vector<std::string> type_filters;
  switch (file_type) {
    case FileType::kAudio:
      type_filters.push_back(kAudioMimeType);
      break;
    case FileType::kImage:
      type_filters.push_back(kImageMimeType);
      break;
    case FileType::kVideo:
      type_filters.push_back(kVideoMimeType);
      break;
    case FileType::kDocument: {
      type_filters.insert(type_filters.end(),
                          file_types_data::kDocumentMIMETypes.begin(),
                          file_types_data::kDocumentMIMETypes.end());
      break;
    }
    default:
      // Leave the filters empty.
      break;
  }
  return type_filters;
}

void RecentDriveSource::GetRecentFiles(const Params& params,
                                       GetRecentFilesCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  auto context = std::make_unique<CallContext>(std::move(callback));
  CallContext* context_ptr = context.get();
  context_map_.AddWithID(std::move(context), params.call_id());

  auto* integration_service =
      drive::util::GetIntegrationServiceByProfile(profile_);
  if (!integration_service) {
    // |integration_service| is nullptr if Drive is disabled.
    OnComplete(params.call_id());
    return;
  }

  auto query_params = drivefs::mojom::QueryParameters::New();
  query_params->page_size = params.max_files();
  query_params->query_source =
      drivefs::mojom::QueryParameters::QuerySource::kLocalOnly;
  query_params->sort_field =
      drivefs::mojom::QueryParameters::SortField::kLastViewedByMe;
  query_params->sort_direction =
      drivefs::mojom::QueryParameters::SortDirection::kDescending;
  std::vector<std::string> type_filters =
      RecentDriveSource::CreateTypeFilters(params.file_type());
  query_params->viewed_time = params.cutoff_time();
  query_params->title = params.query();
  query_params->viewed_time_operator =
      drivefs::mojom::QueryParameters::DateComparisonOperator::kGreaterThan;
  if (type_filters.size() == 1) {
    query_params->mime_type = type_filters.front();
  } else if (type_filters.size() > 1) {
    query_params->mime_types = std::move(type_filters);
  }
  integration_service->GetDriveFsInterface()->StartSearchQuery(
      context_ptr->search_query.BindNewPipeAndPassReceiver(),
      std::move(query_params));
  context_ptr->search_query->GetNextPage(
      base::BindOnce(&RecentDriveSource::GotSearchResults,
                     weak_ptr_factory_.GetWeakPtr(), params));
}

std::vector<RecentFile> RecentDriveSource::Stop(const int32_t call_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  CallContext* context = context_map_.Lookup(call_id);
  if (context == nullptr) {
    return {};
  }
  std::vector<RecentFile> files = std::move(context->files);
  context_map_.Remove(call_id);
  return files;
}

void RecentDriveSource::OnComplete(const int32_t call_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  CallContext* context = context_map_.Lookup(call_id);
  if (context == nullptr) {
    return;
  }
  DCHECK(!context->build_start_time.is_null());

  UMA_HISTOGRAM_TIMES(kLoadHistogramName,
                      base::TimeTicks::Now() - context->build_start_time);
  std::move(context->callback).Run(std::move(context->files));
  context_map_.Remove(call_id);
}

void RecentDriveSource::GotSearchResults(
    const Params& params,
    drive::FileError error,
    std::optional<std::vector<drivefs::mojom::QueryItemPtr>> results) {
  CallContext* context = context_map_.Lookup(params.call_id());
  if (context == nullptr) {
    return;
  }

  auto* integration_service =
      drive::util::GetIntegrationServiceByProfile(profile_);
  if (!results || !integration_service) {
    OnComplete(params.call_id());
    return;
  }

  context->files.reserve(results->size());
  for (auto& result : *results) {
    if (!drivefs::IsAFile(result->metadata->type)) {
      continue;
    }
    base::FilePath path = integration_service->GetMountPointPath().BaseName();
    if (!base::FilePath("/").AppendRelativePath(result->path, &path)) {
      path = path.Append(result->path);
    }
    context->files.emplace_back(
        params.file_system_context()->CreateCrackedFileSystemURL(
            blink::StorageKey::CreateFirstParty(
                url::Origin::Create(params.origin())),
            storage::kFileSystemTypeExternal, path),
        // Do not use "modification_time" field here because that will cause
        // files modified by others recently (e.g. Shared with me) being
        // treated as recent files.
        result->metadata->last_viewed_by_me_time);
  }
  OnComplete(params.call_id());
}

}  // namespace ash
