// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/fileapi/recent_disk_source.h"

#include <utility>

#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "net/base/mime_util.h"
#include "storage/browser/file_system/external_mount_points.h"
#include "storage/browser/file_system/file_system_context.h"
#include "storage/browser/file_system/file_system_operation.h"
#include "storage/browser/file_system/file_system_operation_runner.h"
#include "storage/browser/file_system/file_system_url.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "ui/file_manager/file_types_data.h"
#include "url/origin.h"

using content::BrowserThread;

namespace ash {

namespace {

constexpr char kAudioMimeType[] = "audio/*";
constexpr char kImageMimeType[] = "image/*";
constexpr char kVideoMimeType[] = "video/*";

void OnReadDirectoryOnIOThread(
    const storage::FileSystemOperation::ReadDirectoryCallback& callback,
    base::File::Error result,
    storage::FileSystemOperation::FileEntryList entries,
    bool has_more) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
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

  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), result, info));
}

void GetMetadataOnIOThread(
    scoped_refptr<storage::FileSystemContext> file_system_context,
    const storage::FileSystemURL& url,
    storage::FileSystemOperation::GetMetadataFieldSet fields,
    storage::FileSystemOperation::GetMetadataCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  file_system_context->operation_runner()->GetMetadata(
      url, fields,
      base::BindOnce(&OnGetMetadataOnIOThread, std::move(callback)));
}

}  // namespace

RecentDiskSource::RecentDiskSource::CallContext::CallContext(
    const Params& params,
    GetRecentFilesCallback callback)
    : params(params),
      callback(std::move(callback)),
      build_start_time(base::TimeTicks::Now()),
      accumulator(params.max_files()) {}

RecentDiskSource::RecentDiskSource::CallContext::CallContext(
    CallContext&& context)
    : params(context.params),
      callback(std::move(context.callback)),
      build_start_time(context.build_start_time),
      inflight_readdirs(context.inflight_readdirs),
      inflight_stats(context.inflight_stats),
      accumulator(std::move(context.accumulator)) {}

RecentDiskSource::RecentDiskSource::CallContext::~CallContext() = default;

RecentDiskSource::RecentDiskSource(
    extensions::api::file_manager_private::VolumeType volume_type,
    std::string mount_point_name,
    bool ignore_dotfiles,
    int max_depth,
    std::string uma_histogram_name)
    : RecentSource(volume_type),
      mount_point_name_(std::move(mount_point_name)),
      ignore_dotfiles_(ignore_dotfiles),
      max_depth_(max_depth),
      uma_histogram_name_(std::move(uma_histogram_name)) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
}

RecentDiskSource::~RecentDiskSource() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
}

void RecentDiskSource::GetRecentFiles(const Params& params,
                                      GetRecentFilesCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(context_map_.Lookup(params.call_id()) == nullptr);

  // Return immediately if mount point does not exist.
  storage::ExternalMountPoints* mount_points =
      storage::ExternalMountPoints::GetSystemInstance();
  base::FilePath path;
  if (!mount_points->GetRegisteredPath(mount_point_name_, &path)) {
    std::move(callback).Run({});
    return;
  }

  // Create a unique context for this call.
  auto context = std::make_unique<CallContext>(params, std::move(callback));
  context_map_.AddWithID(std::move(context), params.call_id());

  ScanDirectory(params.call_id(), base::FilePath(), 1);
}

std::vector<RecentFile> RecentDiskSource::Stop(const int32_t call_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  CallContext* context = context_map_.Lookup(call_id);
  if (context == nullptr) {
    // The Stop method was called after we already responded. Just return empty
    // list of files.
    return {};
  }
  // Proper stop; get the files and erase the context.
  const std::vector<RecentFile> files = context->accumulator.Get();
  context_map_.Remove(call_id);
  return files;
}

void RecentDiskSource::ScanDirectory(const int32_t call_id,
                                     const base::FilePath& path,
                                     int depth) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // If context is gone, that is Stop() has been called, exit immediately.
  CallContext* context = context_map_.Lookup(call_id);
  if (context == nullptr) {
    return;
  }

  storage::FileSystemURL url = BuildDiskURL(context->params, path);

  ++context->inflight_readdirs;
  content::GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(
          &ReadDirectoryOnIOThread,
          base::WrapRefCounted(context->params.file_system_context()), url,
          base::BindRepeating(&RecentDiskSource::OnReadDirectory,
                              weak_ptr_factory_.GetWeakPtr(), call_id, path,
                              depth)));
}

void RecentDiskSource::OnReadDirectory(
    const int32_t call_id,
    const base::FilePath& path,
    const int depth,
    base::File::Error result,
    storage::FileSystemOperation::FileEntryList entries,
    bool has_more) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  // If context is gone, that is Stop() has been called, exit immediately.
  CallContext* context = context_map_.Lookup(call_id);
  if (context == nullptr) {
    return;
  }

  const std::u16string q16 = base::UTF8ToUTF16(context->params.query());
  for (const auto& entry : entries) {
    // Ignore directories and files that start with dot.
    if (ignore_dotfiles_ &&
        base::StartsWith(entry.name.value(), ".",
                         base::CompareCase::INSENSITIVE_ASCII)) {
      continue;
    }
    base::FilePath subpath = path.Append(entry.name);

    if (entry.type == filesystem::mojom::FsFileType::DIRECTORY) {
      if ((max_depth_ > 0 && depth >= max_depth_) || context->params.IsLate()) {
        continue;
      }
      ScanDirectory(call_id, subpath, depth + 1);
    } else {
      if (!MatchesFileType(entry.name, context->params.file_type())) {
        continue;
      }
      if (!FileNameMatches(base::UTF8ToUTF16(entry.name.value()), q16)) {
        continue;
      }
      storage::FileSystemURL url = BuildDiskURL(context->params, subpath);
      ++context->inflight_stats;
      content::GetIOThreadTaskRunner({})->PostTask(
          FROM_HERE,
          base::BindOnce(
              &GetMetadataOnIOThread,
              base::WrapRefCounted(context->params.file_system_context()), url,
              storage::FileSystemOperation::GetMetadataFieldSet(
                  {storage::FileSystemOperation::GetMetadataField::
                       kLastModified}),
              base::BindOnce(&RecentDiskSource::OnGotMetadata,
                             weak_ptr_factory_.GetWeakPtr(), call_id, url)));
    }
  }

  if (has_more) {
    return;
  }

  --context->inflight_readdirs;
  if (context->inflight_stats == 0 && context->inflight_readdirs == 0) {
    OnReadOrStatFinished(call_id);
  }
}

void RecentDiskSource::OnGotMetadata(const int32_t call_id,
                                     const storage::FileSystemURL& url,
                                     base::File::Error result,
                                     const base::File::Info& info) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  // If context is gone, that is Stop() has been called, exit immediately.
  CallContext* context = context_map_.Lookup(call_id);
  if (context == nullptr) {
    return;
  }

  if (result == base::File::FILE_OK &&
      info.last_modified >= context->params.cutoff_time()) {
    context->accumulator.Add(RecentFile(url, info.last_modified));
  }

  --context->inflight_stats;
  if (context->inflight_stats == 0 && context->inflight_readdirs == 0) {
    OnReadOrStatFinished(call_id);
  }
}

void RecentDiskSource::OnReadOrStatFinished(const int32_t call_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  CallContext* context = context_map_.Lookup(call_id);
  // If context is gone, that is Stop() has been called, exit immediately.
  if (context == nullptr) {
    return;
  }

  DCHECK(context->inflight_stats == 0);
  DCHECK(context->inflight_readdirs == 0);
  DCHECK(!context->build_start_time.is_null());

  // All reads/scans completed.
  UmaHistogramTimes(uma_histogram_name_,
                    base::TimeTicks::Now() - context->build_start_time);

  std::move(context->callback).Run(context->accumulator.Get());
  context_map_.Remove(call_id);
}

storage::FileSystemURL RecentDiskSource::BuildDiskURL(
    const Params& params,
    const base::FilePath& path) const {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  storage::ExternalMountPoints* mount_points =
      storage::ExternalMountPoints::GetSystemInstance();
  return mount_points->CreateExternalFileSystemURL(
      blink::StorageKey::CreateFirstParty(url::Origin::Create(params.origin())),
      mount_point_name_, path);
}

bool RecentDiskSource::MatchesFileType(const base::FilePath& path,
                                       RecentSource::FileType file_type) {
  if (file_type == RecentSource::FileType::kAll) {
    return true;
  }

  // File type for |path| is guessed by data generated from file_types.json5.
  // It guesses mime types based on file extensions, but it has a limited set
  // of file extensions.
  // TODO(fukino): It is better to have better coverage of file extensions to be
  // consistent with file-type detection on Android system. crbug.com/1034874.
  const auto ext = base::ToLowerASCII(path.Extension());
  if (!file_types_data::kExtensionToMIME.contains(ext)) {
    return false;
  }
  std::string mime_type = file_types_data::kExtensionToMIME.at(ext);

  switch (file_type) {
    case RecentSource::FileType::kAudio:
      return net::MatchesMimeType(kAudioMimeType, mime_type);
    case RecentSource::FileType::kImage:
      return net::MatchesMimeType(kImageMimeType, mime_type);
    case RecentSource::FileType::kVideo:
      return net::MatchesMimeType(kVideoMimeType, mime_type);
    case RecentSource::FileType::kDocument:
      return file_types_data::kDocumentMIMETypes.contains(mime_type);
    default:
      return false;
  }
}

}  // namespace ash
