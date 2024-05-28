// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/fileapi/recent_arc_media_source.h"

#include <algorithm>
#include <iterator>
#include <utility>

#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ash/arc/arc_util.h"
#include "chrome/browser/ash/arc/fileapi/arc_documents_provider_root.h"
#include "chrome/browser/ash/arc/fileapi/arc_documents_provider_root_map.h"
#include "chrome/browser/ash/arc/fileapi/arc_documents_provider_util.h"
#include "chrome/browser/ash/arc/fileapi/arc_media_view_util.h"
#include "chrome/browser/ash/fileapi/recent_file.h"
#include "chrome/common/extensions/api/file_manager_private.h"
#include "content/public/browser/browser_thread.h"
#include "storage/browser/file_system/external_mount_points.h"

using content::BrowserThread;

namespace ash {

namespace {

namespace fmp = extensions::api::file_manager_private;

const char kAndroidDownloadDirPrefix[] = "/storage/emulated/0/Download/";
// The path of the MyFiles directory inside Android. The UUID "0000....2019" is
// defined in ash/components/arc/volume_mounter/arc_volume_mounter_bridge.cc.
// TODO(crbug.com/929031): Move MyFiles constants to a common place.
const char kAndroidMyFilesDirPrefix[] =
    "/storage/0000000000000000000000000000CAFEF00D2019/";

base::FilePath GetRelativeMountPath(const std::string& root_id) {
  base::FilePath mount_path = arc::GetDocumentsProviderMountPath(
      arc::kMediaDocumentsProviderAuthority, root_id);
  base::FilePath relative_mount_path;
  base::FilePath(arc::kDocumentsProviderMountPointPath)
      .AppendRelativePath(mount_path, &relative_mount_path);
  return relative_mount_path;
}

bool IsInsideDownloadsOrMyFiles(const std::string& path) {
  if (base::StartsWith(path, kAndroidDownloadDirPrefix,
                       base::CompareCase::SENSITIVE)) {
    return true;
  }
  if (base::StartsWith(path, kAndroidMyFilesDirPrefix,
                       base::CompareCase::SENSITIVE)) {
    return true;
  }
  return false;
}

std::vector<RecentFile> ExtractFoundFiles(
    const std::map<std::string, std::optional<RecentFile>>&
        document_id_to_file) {
  std::vector<RecentFile> files;
  for (const auto& entry : document_id_to_file) {
    const std::optional<RecentFile>& file = entry.second;
    if (file.has_value()) {
      files.emplace_back(file.value());
    }
  }
  return files;
}

// Tidies up the vector of files by sorting them and limiting their number to
// the specified maximum.
std::vector<RecentFile> PrepareResponse(std::vector<RecentFile>&& files,
                                        size_t max_files) {
  std::sort(files.begin(), files.end(), RecentFileComparator());
  if (files.size() > max_files) {
    files.resize(max_files);
  }
  return files;
}

}  // namespace

RecentArcMediaSource::CallContext::CallContext(const Params& params,
                                               GetRecentFilesCallback callback)
    : params(params),
      callback(std::move(callback)),
      build_start_time(base::TimeTicks::Now()) {}
RecentArcMediaSource::CallContext::CallContext(CallContext&& context)
    : params(context.params),
      callback(std::move(context.callback)),
      build_start_time(std::move(context.build_start_time)) {}

RecentArcMediaSource::CallContext::~CallContext() = default;

const char RecentArcMediaSource::kLoadHistogramName[] =
    "FileBrowser.Recent.LoadArcMedia";

RecentArcMediaSource::RecentArcMediaSource(Profile* profile,
                                           const std::string& root_id)
    : RecentSource(fmp::VolumeType::kMediaView),
      profile_(profile),
      root_id_(root_id),
      relative_mount_path_(GetRelativeMountPath(root_id)) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
}

RecentArcMediaSource::~RecentArcMediaSource() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
}

bool RecentArcMediaSource::MatchesFileType(FileType file_type) const {
  switch (file_type) {
    case FileType::kAll:
      return true;
    case FileType::kImage:
      return root_id_ == arc::kImagesRootId;
    case FileType::kVideo:
      return root_id_ == arc::kVideosRootId;
    case FileType::kDocument:
      return root_id_ == arc::kDocumentsRootId;
    case FileType::kAudio:
      return root_id_ == arc::kAudioRootId;
    default:
      LOG(FATAL) << "Unhandled file_type: " << static_cast<int>(file_type);
  }
}

void RecentArcMediaSource::GetRecentFiles(const Params& params,
                                          GetRecentFilesCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(context_map_.Lookup(params.call_id()) == nullptr);

  // If ARC file system operations will be deferred, return immediately without
  // recording UMA metrics.
  //
  // TODO(nya): Return files progressively rather than simply giving up.
  // Also, it is wrong to assume all following operations will not be deferred
  // just because this function returned true. However, in practice, it is rare
  // ArcFileSystemOperationRunner's deferring state switches from disabled to
  // enabled (one such case is when ARC container crashes).
  if (!WillArcFileSystemOperationsRunImmediately()) {
    std::move(callback).Run({});
    return;
  }

  auto context = std::make_unique<CallContext>(params, std::move(callback));
  context_map_.AddWithID(std::move(context), params.call_id());

  if (!MatchesFileType(params.file_type())) {
    // Return immediately without results when this root's id does not match the
    // requested file type.
    OnComplete(params.call_id());
    return;
  }

  auto* runner =
      arc::ArcFileSystemOperationRunner::GetForBrowserContext(profile_);
  if (!runner) {
    // This happens when ARC is not allowed in this profile.
    OnComplete(params.call_id());
    return;
  }

  runner->GetRecentDocuments(
      arc::kMediaDocumentsProviderAuthority, root_id_,
      base::BindOnce(&RecentArcMediaSource::OnRunnerDone,
                     weak_ptr_factory_.GetWeakPtr(), params.call_id()));
}

void RecentArcMediaSource::OnRunnerDone(
    const int32_t call_id,
    std::optional<std::vector<arc::mojom::DocumentPtr>> maybe_documents) {
  if (!lag_.is_positive()) {
    OnGotRecentDocuments(call_id, std::move(maybe_documents));
    return;
  }

  if (!timer_) {
    timer_ = std::make_unique<base::OneShotTimer>();
  }
  timer_->Start(FROM_HERE, lag_,
                base::BindOnce(&RecentArcMediaSource::OnGotRecentDocuments,
                               weak_ptr_factory_.GetWeakPtr(), call_id,
                               std::move(maybe_documents)));
}

void RecentArcMediaSource::OnGotRecentDocuments(
    const int32_t call_id,
    std::optional<std::vector<arc::mojom::DocumentPtr>> maybe_documents) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  CallContext* context = context_map_.Lookup(call_id);
  if (context == nullptr) {
    return;
  }

  // Initialize |document_id_to_file_| with recent document IDs returned.
  if (maybe_documents.has_value()) {
    const std::u16string q16 = base::UTF8ToUTF16(context->params.query());
    for (const auto& document : maybe_documents.value()) {
      // Exclude media files under Downloads or MyFiles directory since they are
      // covered by RecentDiskSource.
      if (document->android_file_system_path.has_value() &&
          IsInsideDownloadsOrMyFiles(
              document->android_file_system_path.value())) {
        continue;
      }
      if (!FileNameMatches(base::UTF8ToUTF16(document->display_name), q16)) {
        continue;
      }
      context->document_id_to_file.emplace(document->document_id, std::nullopt);
    }
  }

  if (context->document_id_to_file.empty()) {
    OnComplete(call_id);
    return;
  }

  // We have several recent documents, so start searching their real paths.
  ScanDirectory(call_id, base::FilePath());
}

void RecentArcMediaSource::ScanDirectory(const int32_t call_id,
                                         const base::FilePath& path) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  // If context was cleared while we were scanning directories, just abandon
  // this effort.
  CallContext* context = context_map_.Lookup(call_id);
  if (context == nullptr) {
    return;
  }

  ++context->num_inflight_readdirs;

  auto* root_map =
      arc::ArcDocumentsProviderRootMap::GetForBrowserContext(profile_);
  if (!root_map) {
    // We already checked ARC is allowed for this profile (indirectly), so
    // this should never happen.
    LOG(ERROR) << "ArcDocumentsProviderRootMap is not available";
    OnDirectoryRead(call_id, path, base::File::FILE_ERROR_FAILED, {});
    return;
  }

  auto* root =
      root_map->Lookup(arc::kMediaDocumentsProviderAuthority, root_id_);
  if (!root) {
    // Media roots should always exist.
    LOG(ERROR) << "ArcDocumentsProviderRoot is missing";
    OnDirectoryRead(call_id, path, base::File::FILE_ERROR_NOT_FOUND, {});
    return;
  }

  root->ReadDirectory(
      path, base::BindOnce(&RecentArcMediaSource::OnDirectoryRead,
                           weak_ptr_factory_.GetWeakPtr(), call_id, path));
}

void RecentArcMediaSource::OnDirectoryRead(
    const int32_t call_id,
    const base::FilePath& path,
    base::File::Error result,
    std::vector<arc::ArcDocumentsProviderRoot::ThinFileInfo> files) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  // If callback was cleared while we were scanning directories just abandon
  // this effort.
  CallContext* context = context_map_.Lookup(call_id);
  if (context == nullptr) {
    return;
  }

  for (const auto& file : files) {
    base::FilePath subpath = path.Append(file.name);
    if (file.is_directory) {
      if (!context->params.IsLate()) {
        ScanDirectory(call_id, subpath);
      }
      continue;
    }

    auto doc_it = context->document_id_to_file.find(file.document_id);
    if (doc_it == context->document_id_to_file.end()) {
      continue;
    }

    // Update |document_id_to_file_|.
    // We keep the lexicographically smallest URL to stabilize the results when
    // there are multiple files with the same document ID.
    auto url = BuildDocumentsProviderUrl(context->params, subpath);
    std::optional<RecentFile>& entry = doc_it->second;
    if (!entry.has_value() ||
        storage::FileSystemURL::Comparator()(url, entry.value().url())) {
      entry = RecentFile(url, file.last_modified);
    }
  }

  --context->num_inflight_readdirs;
  DCHECK_LE(0, context->num_inflight_readdirs);

  if (context->num_inflight_readdirs == 0) {
    OnComplete(call_id);
  }
}

std::vector<RecentFile> RecentArcMediaSource::Stop(const int32_t call_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  CallContext* context = context_map_.Lookup(call_id);
  if (context == nullptr) {
    // Here we assume that the call to stop came just after we returned the
    // results in the OnComplete method.
    return {};
  }

  size_t max_files = context->params.max_files();
  // We do not call the callback, so just clean it up.
  context->callback.Reset();

  // Copy the files we collected so far.
  std::vector<RecentFile> files =
      ExtractFoundFiles(context->document_id_to_file);

  context_map_.Remove(call_id);

  return PrepareResponse(std::move(files), max_files);
}

void RecentArcMediaSource::OnComplete(const int32_t call_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  CallContext* context = context_map_.Lookup(call_id);
  if (context == nullptr) {
    // If we cannot find the context that means the Stop method has been called.
    // Just return immediately.
    return;
  }

  UMA_HISTOGRAM_TIMES(kLoadHistogramName,
                      base::TimeTicks::Now() - context->build_start_time);

  DCHECK_EQ(0, context->num_inflight_readdirs);
  DCHECK(!context->callback.is_null());

  std::vector<RecentFile> files =
      ExtractFoundFiles(context->document_id_to_file);
  std::move(context->callback)
      .Run(PrepareResponse(std::move(files), context->params.max_files()));
  context_map_.Remove(call_id);
}

bool RecentArcMediaSource::WillArcFileSystemOperationsRunImmediately() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  auto* runner =
      arc::ArcFileSystemOperationRunner::GetForBrowserContext(profile_);

  // If ARC is not allowed the user, |runner| is nullptr.
  if (!runner) {
    return false;
  }

  return !runner->WillDefer();
}

void RecentArcMediaSource::SetLagForTesting(const base::TimeDelta& lag) {
  lag_ = lag;
}

storage::FileSystemURL RecentArcMediaSource::BuildDocumentsProviderUrl(
    const Params& params,
    const base::FilePath& path) const {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  storage::ExternalMountPoints* mount_points =
      storage::ExternalMountPoints::GetSystemInstance();
  DCHECK(mount_points);

  return mount_points->CreateExternalFileSystemURL(
      blink::StorageKey::CreateFirstParty(url::Origin::Create(params.origin())),
      arc::kDocumentsProviderMountPointName, relative_mount_path_.Append(path));
}

}  // namespace ash
