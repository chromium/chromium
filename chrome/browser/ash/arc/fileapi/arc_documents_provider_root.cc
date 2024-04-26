// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/fileapi/arc_documents_provider_root.h"

#include <algorithm>
#include <utility>

#include "ash/components/arc/arc_features.h"
#include "base/check_op.h"
#include "base/files/file.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chrome/browser/ash/arc/fileapi/arc_content_file_system_size_util.h"
#include "chrome/browser/ash/arc/fileapi/arc_documents_provider_util.h"
#include "content/public/browser/browser_thread.h"
#include "net/base/mime_util.h"
#include "url/gurl.h"

using content::BrowserThread;
using EntryList = storage::AsyncFileUtil::EntryList;

namespace arc {

namespace {

// Directory cache will be cleared this duration after it is built.
constexpr base::TimeDelta kCacheExpiration = base::Seconds(60);

void OnGetFileSizeFromOpenFile(
    ArcDocumentsProviderRoot::GetFileInfoCallback callback,
    base::File::Info info,
    base::File::Error error,
    int64_t size) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (error == base::File::FILE_OK) {
    info.size = size;
    std::move(callback).Run(error, info);
  } else {
    std::move(callback).Run(error, base::File::Info());
  }
}

void OnResolveToContentUrl(
    ArcDocumentsProviderRoot::GetFileInfoCallback callback,
    ArcFileSystemOperationRunner* runner,
    const base::File::Info& info,
    const GURL& content_url) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  GetFileSizeFromOpenFileOnUIThread(
      content_url, runner,
      base::BindOnce(&OnGetFileSizeFromOpenFile, std::move(callback), info));
}

}  // namespace

// Represents the status of a document watcher.
struct ArcDocumentsProviderRoot::WatcherData {
  // ID of a watcher in the remote file system service.
  //
  // Valid IDs are represented by positive integers. An invalid watcher is
  // represented by |kInvalidWatcherId|, which occurs in several cases:
  //
  // - AddWatcher request is still in-flight. In this case, a valid ID is set
  //   to |inflight_request_id|.
  //
  // - The remote file system service notified us that it stopped and all
  //   watchers were forgotten. Such watchers are still tracked here, but they
  //   are not known by the remote service.
  int64_t id;

  // A unique ID of AddWatcher() request.
  //
  // While AddWatcher() is in-flight, a positive integer is set to this
  // variable, and |id| is |kInvalidWatcherId|. Otherwise it is set to
  // |kInvalidWatcherRequestId|.
  uint64_t inflight_request_id;
};

// Cache of directory contents.
struct ArcDocumentsProviderRoot::DirectoryCache {
  // Files under the directory.
  NameToDocumentMap mapping;

  // Timer to delete this cache.
  base::OneShotTimer clear_timer;
};

// static
const int64_t ArcDocumentsProviderRoot::kInvalidWatcherId = -1;
// static
const uint64_t ArcDocumentsProviderRoot::kInvalidWatcherRequestId = 0;
// static
const ArcDocumentsProviderRoot::WatcherData
    ArcDocumentsProviderRoot::kInvalidWatcherData = {kInvalidWatcherId,
                                                     kInvalidWatcherRequestId};

ArcDocumentsProviderRoot::ArcDocumentsProviderRoot(
    ArcFileSystemOperationRunner* runner,
    const std::string& authority,
    const std::string& root_document_id,
    const std::string& root_id,
    bool read_only,
    const std::vector<std::string>& mime_types)
    : runner_(runner),
      authority_(authority),
      root_document_id_(root_document_id),
      root_id_(root_id),
      read_only_(read_only),
      mime_types_(mime_types) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  runner_->AddObserver(this);
}

ArcDocumentsProviderRoot::~ArcDocumentsProviderRoot() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  runner_->RemoveObserver(this);
}

void ArcDocumentsProviderRoot::GetFileInfo(
    const base::FilePath& path,
    storage::FileSystemOperation::GetMetadataFieldSet fields,
    GetFileInfoCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (path.IsAbsolute()) {
    std::move(callback).Run(base::File::FILE_ERROR_NOT_FOUND,
                            base::File::Info());
    return;
  }

  // Specially handle the root directory since Files app does not update the
  // list of file systems (left pane) until all volumes respond to GetMetadata
  // requests to root directories.
  if (path.empty()) {
    base::File::Info info;
    info.size = -1;
    info.is_directory = true;
    info.is_symbolic_link = false;
    info.last_modified = info.last_accessed = info.creation_time =
        base::Time::UnixEpoch();  // arbitrary
    std::move(callback).Run(base::File::FILE_OK, info);
    return;
  }

  GetDocument(
      path, base::BindOnce(&ArcDocumentsProviderRoot::GetFileInfoFromDocument,
                           weak_ptr_factory_.GetWeakPtr(), std::move(callback),
                           path, fields));
}

void ArcDocumentsProviderRoot::ReadDirectory(const base::FilePath& path,
                                             ReadDirectoryCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  ResolveToDocumentId(
      path,
      base::BindOnce(&ArcDocumentsProviderRoot::ReadDirectoryWithDocumentId,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void ArcDocumentsProviderRoot::DeleteFile(const base::FilePath& path,
                                          StatusCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (read_only_) {
    std::move(callback).Run(base::File::FILE_ERROR_ACCESS_DENIED);
    return;
  }
  ResolveToDocumentId(
      path, base::BindOnce(&ArcDocumentsProviderRoot::DeleteFileWithDocumentId,
                           weak_ptr_factory_.GetWeakPtr(), std::move(callback),
                           path));
}

void ArcDocumentsProviderRoot::CreateFile(const base::FilePath& path,
                                          StatusCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (read_only_) {
    std::move(callback).Run(base::File::FILE_ERROR_ACCESS_DENIED);
    return;
  }
  ResolveToDocumentId(
      path, base::BindOnce(
                &ArcDocumentsProviderRoot::CreateFileAfterConflictCheck,
                weak_ptr_factory_.GetWeakPtr(), std::move(callback), path));
}

void ArcDocumentsProviderRoot::CreateDirectory(const base::FilePath& path,
                                               StatusCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (read_only_) {
    std::move(callback).Run(base::File::FILE_ERROR_ACCESS_DENIED);
    return;
  }
  ResolveToDocumentId(
      path, base::BindOnce(
                &ArcDocumentsProviderRoot::CreateDirectoryAfterConflictCheck,
                weak_ptr_factory_.GetWeakPtr(), std::move(callback), path));
}

void ArcDocumentsProviderRoot::CopyFileLocal(const base::FilePath& src_path,
                                             const base::FilePath& dest_path,
                                             StatusCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (read_only_) {
    std::move(callback).Run(base::File::FILE_ERROR_ACCESS_DENIED);
    return;
  }
  if (src_path == dest_path) {
    std::move(callback).Run(base::File::FILE_ERROR_INVALID_OPERATION);
    return;
  }
  ResolveToDocumentId(
      src_path,
      base::BindOnce(&ArcDocumentsProviderRoot::CopyFileWithSourceDocumentId,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback),
                     dest_path, src_path.BaseName().value()));
}

void ArcDocumentsProviderRoot::MoveFileLocal(const base::FilePath& src_path,
                                             const base::FilePath& dest_path,
                                             StatusCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (read_only_) {
    std::move(callback).Run(base::File::FILE_ERROR_ACCESS_DENIED);
    return;
  }
  if (src_path.DirName() == dest_path.DirName()) {
    RenameFileInternal(src_path, dest_path.BaseName().value(),
                       std::move(callback));
  } else {
    MoveFileInternal(src_path, dest_path, std::move(callback));
  }
}

void ArcDocumentsProviderRoot::AddWatcher(
    const base::FilePath& path,
    WatcherNotificationCallback watcher_callback,
    WatcherStatusCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (path_to_watcher_data_.count(path)) {
    std::move(callback).Run(base::File::FILE_ERROR_FAILED);
    return;
  }
  uint64_t watcher_request_id = next_watcher_request_id_++;
  path_to_watcher_data_.insert(
      std::make_pair(path, WatcherData{kInvalidWatcherId, watcher_request_id}));
  ResolveToDocumentId(
      path, base::BindOnce(&ArcDocumentsProviderRoot::AddWatcherWithDocumentId,
                           weak_ptr_factory_.GetWeakPtr(), path,
                           watcher_request_id, std::move(watcher_callback)));

  // HACK: Invoke |callback| immediately.
  //
  // TODO(crbug.com/40509383): Remove this hack. It was introduced because Files
  // app freezes until AddWatcher() finishes, but it should be handled in Files
  // app rather than here.
  std::move(callback).Run(base::File::FILE_OK);
}

void ArcDocumentsProviderRoot::RemoveWatcher(const base::FilePath& path,
                                             WatcherStatusCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  auto iter = path_to_watcher_data_.find(path);
  if (iter == path_to_watcher_data_.end()) {
    std::move(callback).Run(base::File::FILE_ERROR_FAILED);
    return;
  }
  int64_t watcher_id = iter->second.id;
  path_to_watcher_data_.erase(iter);
  if (watcher_id == kInvalidWatcherId) {
    // This is an invalid watcher. No need to send a request to the remote
    // service.
    std::move(callback).Run(base::File::FILE_OK);
    return;
  }
  runner_->RemoveWatcher(
      watcher_id,
      base::BindOnce(&ArcDocumentsProviderRoot::OnWatcherRemoved,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void ArcDocumentsProviderRoot::ResolveToContentUrl(
    const base::FilePath& path,
    ResolveToContentUrlCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  ResolveToDocumentId(
      path, base::BindOnce(
                &ArcDocumentsProviderRoot::ResolveToContentUrlWithDocumentId,
                weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void ArcDocumentsProviderRoot::GetExtraFileMetadata(
    const base::FilePath& path,
    GetExtraMetadataCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (path.IsAbsolute()) {
    std::move(callback).Run(base::File::FILE_ERROR_NOT_FOUND, {});
    return;
  }
  GetDocument(path, base::BindOnce(
                        &ArcDocumentsProviderRoot::GetExtraMetadataFromDocument,
                        weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void ArcDocumentsProviderRoot::SetDirectoryCacheExpireSoonForTesting() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  directory_cache_expire_soon_ = true;
}

void ArcDocumentsProviderRoot::OnWatchersCleared() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  // Mark all watchers invalid.
  for (auto& entry : path_to_watcher_data_)
    entry.second = kInvalidWatcherData;
}

void ArcDocumentsProviderRoot::GetRootSize(GetRootSizeCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (root_id_.empty()) {
    // Exit early if ID is missing for the given provider authority.
    std::move(callback).Run(true /* error */, 0, 0);
    return;
  }

  runner_->GetRootSize(
      authority_, root_id_,
      base::BindOnce(&ArcDocumentsProviderRoot::OnGetRootSize,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void ArcDocumentsProviderRoot::OnGetRootSize(GetRootSizeCallback callback,
                                             mojom::RootSizePtr root_size) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (root_size.is_null() || root_size->available_bytes < 0) {
    // The root_size and its available bytes are required from the file system.
    std::move(callback).Run(true /* error */, 0, 0);
    return;
  }
  if (root_size->capacity_bytes < 0) {
    // If available bytes is provided but not capacity bytes, it's still valid.
    std::move(callback).Run(false, root_size->available_bytes, 0);
    return;
  }

  std::move(callback).Run(false,
                          static_cast<uint64_t>(root_size->available_bytes),
                          static_cast<uint64_t>(root_size->capacity_bytes));
}

void ArcDocumentsProviderRoot::GetFileInfoFromDocument(
    GetFileInfoCallback callback,
    const base::FilePath& path,
    storage::FileSystemOperation::GetMetadataFieldSet fields,
    base::File::Error error,
    const mojom::DocumentPtr& document) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (error != base::File::FILE_OK) {
    std::move(callback).Run(error, {});
    return;
  }
  DCHECK(document);

  base::File::Info info;
  if (fields.Has(storage::FileSystemOperation::GetMetadataField::kSize)) {
    info.size = document->size;
  }
  bool is_directory = document->mime_type == kAndroidDirectoryMimeType;
  if (fields.Has(
          storage::FileSystemOperation::GetMetadataField::kIsDirectory)) {
    info.is_directory = is_directory;
  }
  info.is_symbolic_link = false;
  if (fields.Has(
          storage::FileSystemOperation::GetMetadataField::kLastModified)) {
    info.last_modified = info.last_accessed = info.creation_time =
        base::Time::FromMillisecondsSinceUnixEpoch(
            base::checked_cast<int64_t>(document->last_modified));
  }

  if (base::FeatureList::IsEnabled(kDocumentsProviderUnknownSizeFeature) &&
      (fields.Has(storage::FileSystemOperation::GetMetadataField::kSize)) &&
      info.size == kUnknownFileSize && !is_directory) {
    // We don't know the size from metadata and the size is requested, find it
    // out by opening the file
    ResolveToContentUrl(
        path, base::BindOnce(&OnResolveToContentUrl, std::move(callback),
                             runner_, info));
  } else {
    std::move(callback).Run(base::File::FILE_OK, info);
  }
}

void ArcDocumentsProviderRoot::ReadDirectoryWithDocumentId(
    ReadDirectoryCallback callback,
    const std::string& document_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (document_id.empty()) {
    std::move(callback).Run(base::File::FILE_ERROR_NOT_FOUND, {});
    return;
  }
  ReadDirectoryInternal(
      document_id, true /* force_refresh */,
      base::BindOnce(
          &ArcDocumentsProviderRoot::ReadDirectoryWithNameToDocumentMap,
          weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void ArcDocumentsProviderRoot::ReadDirectoryWithNameToDocumentMap(
    ReadDirectoryCallback callback,
    base::File::Error error,
    const NameToDocumentMap& mapping) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (error != base::File::FILE_OK) {
    std::move(callback).Run(error, {});
    return;
  }

  std::vector<ThinFileInfo> files;
  for (const auto& pair : mapping) {
    const base::FilePath::StringType& name = pair.first;
    const mojom::DocumentPtr& document = pair.second;
    files.emplace_back(ThinFileInfo{
        name, document->document_id,
        document->mime_type == kAndroidDirectoryMimeType,
        base::Time::FromMillisecondsSinceUnixEpoch(
            base::checked_cast<int64_t>(document->last_modified))});
  }
  std::move(callback).Run(base::File::FILE_OK, std::move(files));
}

void ArcDocumentsProviderRoot::DeleteFileWithDocumentId(
    StatusCallback callback,
    const base::FilePath& path,
    const std::string& document_id) {
  ResolveToDocumentId(
      path.DirName(),
      base::BindOnce(&ArcDocumentsProviderRoot::DeleteFileWithParentDocumentId,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback),
                     document_id));
}

void ArcDocumentsProviderRoot::DeleteFileWithParentDocumentId(
    StatusCallback callback,
    const std::string& document_id,
    const std::string& parent_document_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (document_id.empty()) {
    std::move(callback).Run(base::File::FILE_ERROR_NOT_FOUND);
    return;
  }

  runner_->DeleteDocument(
      authority_, document_id,
      base::BindOnce(&ArcDocumentsProviderRoot::OnFileDeleted,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback),
                     parent_document_id));
}

void ArcDocumentsProviderRoot::OnFileDeleted(
    StatusCallback callback,
    const std::string& parent_document_id,
    bool success) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (success) {
    ClearDirectoryCache(parent_document_id);
  }
  std::move(callback).Run(success ? base::File::FILE_OK
                                  : base::File::FILE_ERROR_FAILED);
}

void ArcDocumentsProviderRoot::CreateFileAfterConflictCheck(
    StatusCallback callback,
    const base::FilePath& path,
    const std::string& document_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!document_id.empty()) {
    std::move(callback).Run(base::File::FILE_ERROR_EXISTS);
    return;
  }
  ResolveToDocumentId(
      path.DirName(),
      base::BindOnce(&ArcDocumentsProviderRoot::CreateFileWithParentDocumentId,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback),
                     path.BaseName()));
}

void ArcDocumentsProviderRoot::CreateFileWithParentDocumentId(
    StatusCallback callback,
    const base::FilePath& basename,
    const std::string& parent_document_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (parent_document_id.empty()) {
    std::move(callback).Run(base::File::FILE_ERROR_NOT_FOUND);
    return;
  }
  std::string mime_type;
  if (!net::GetMimeTypeFromFile(basename, &mime_type))
    mime_type = "application/octet-stream";
  runner_->CreateDocument(
      authority_, parent_document_id, mime_type, basename.value(),
      base::BindOnce(&ArcDocumentsProviderRoot::OnFileCreated,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback),
                     parent_document_id));
}

void ArcDocumentsProviderRoot::CreateDirectoryAfterConflictCheck(
    StatusCallback callback,
    const base::FilePath& path,
    const std::string& document_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!document_id.empty()) {
    std::move(callback).Run(base::File::FILE_ERROR_EXISTS);
    return;
  }
  ResolveToDocumentId(
      path.DirName(),
      base::BindOnce(
          &ArcDocumentsProviderRoot::CreateDirectoryWithParentDocumentId,
          weak_ptr_factory_.GetWeakPtr(), std::move(callback),
          path.BaseName()));
}

void ArcDocumentsProviderRoot::CreateDirectoryWithParentDocumentId(
    StatusCallback callback,
    const base::FilePath& basename,
    const std::string& parent_document_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (parent_document_id.empty()) {
    std::move(callback).Run(base::File::FILE_ERROR_NOT_FOUND);
    return;
  }
  runner_->CreateDocument(
      authority_, parent_document_id, kAndroidDirectoryMimeType,
      basename.value(),
      base::BindOnce(&ArcDocumentsProviderRoot::OnFileCreated,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback),
                     parent_document_id));
}

void ArcDocumentsProviderRoot::OnFileCreated(
    StatusCallback callback,
    const std::string& parent_document_id,
    mojom::DocumentPtr document) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (document.is_null()) {
    std::move(callback).Run(base::File::FILE_ERROR_FAILED);
    return;
  }
  // Invalidate directory cache as the directory content is updated.
  ClearDirectoryCache(parent_document_id);
  std::move(callback).Run(base::File::FILE_OK);
}

void ArcDocumentsProviderRoot::RenameFileInternal(
    const base::FilePath& path,
    const std::string& display_name,
    StatusCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  ResolveToDocumentId(
      path, base::BindOnce(&ArcDocumentsProviderRoot::RenameFileWithDocumentId,
                           weak_ptr_factory_.GetWeakPtr(), std::move(callback),
                           path, display_name));
}

void ArcDocumentsProviderRoot::RenameFileWithDocumentId(
    StatusCallback callback,
    const base::FilePath& path,
    const std::string& display_name,
    const std::string& document_id) {
  ResolveToDocumentId(
      path.DirName(),
      base::BindOnce(&ArcDocumentsProviderRoot::RenameFileWithParentDocumentId,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback),
                     display_name, document_id));
}

void ArcDocumentsProviderRoot::RenameFileWithParentDocumentId(
    StatusCallback callback,
    const std::string& display_name,
    const std::string& document_id,
    const std::string& parent_document_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (document_id.empty()) {
    std::move(callback).Run(base::File::FILE_ERROR_NOT_FOUND);
    return;
  }
  // TODO(fukino): Consider updating MIME type of the document based when the
  // file extension is changed.
  runner_->RenameDocument(
      authority_, document_id, display_name,
      base::BindOnce(&ArcDocumentsProviderRoot::OnFileRenamed,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback),
                     parent_document_id));
}

void ArcDocumentsProviderRoot::OnFileRenamed(
    StatusCallback callback,
    const std::string& parent_document_id,
    mojom::DocumentPtr document) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (document.is_null()) {
    std::move(callback).Run(base::File::FILE_ERROR_FAILED);
    return;
  }

  if (!parent_document_id.empty()) {
    ClearDirectoryCache(parent_document_id);
  }
  std::move(callback).Run(base::File::FILE_OK);
}

void ArcDocumentsProviderRoot::CopyFileWithSourceDocumentId(
    StatusCallback callback,
    const base::FilePath& target_path,
    const std::string& source_display_name,
    const std::string& source_document_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (source_document_id.empty()) {
    std::move(callback).Run(base::File::FILE_ERROR_NOT_FOUND);
    return;
  }
  const std::string target_display_name = target_path.BaseName().value();
  const std::string target_display_name_to_rename =
      source_display_name == target_display_name ? "" : target_display_name;
  ResolveToDocumentId(
      target_path.DirName(),
      base::BindOnce(
          &ArcDocumentsProviderRoot::CopyFileWithTargetParentDocumentId,
          weak_ptr_factory_.GetWeakPtr(), std::move(callback),
          source_document_id, target_display_name_to_rename));
}

void ArcDocumentsProviderRoot::CopyFileWithTargetParentDocumentId(
    StatusCallback callback,
    const std::string& source_document_id,
    const std::string& target_display_name_to_rename,
    const std::string& target_parent_document_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (target_parent_document_id.empty()) {
    std::move(callback).Run(base::File::FILE_ERROR_NOT_FOUND);
    return;
  }
  runner_->CopyDocument(
      authority_, source_document_id, target_parent_document_id,
      base::BindOnce(&ArcDocumentsProviderRoot::OnFileCopied,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback),
                     target_display_name_to_rename, target_parent_document_id));
}

void ArcDocumentsProviderRoot::OnFileCopied(
    StatusCallback callback,
    const std::string& target_display_name_to_rename,
    const std::string& target_parent_document_id,
    mojom::DocumentPtr document) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (document.is_null()) {
    std::move(callback).Run(base::File::FILE_ERROR_FAILED);
    return;
  }
  if (target_display_name_to_rename.empty()) {
    ClearDirectoryCache(target_parent_document_id);
    std::move(callback).Run(base::File::FILE_OK);
    return;
  }
  RenameFileWithParentDocumentId(
      std::move(callback), target_display_name_to_rename, document->document_id,
      target_parent_document_id);
}

void ArcDocumentsProviderRoot::MoveFileInternal(
    const base::FilePath& source_path,
    const base::FilePath& target_path,
    StatusCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  ResolveToDocumentId(
      source_path,
      base::BindOnce(&ArcDocumentsProviderRoot::MoveFileWithSourceDocumentId,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback),
                     source_path.DirName(), target_path,
                     source_path.BaseName().value()));
}

void ArcDocumentsProviderRoot::MoveFileWithSourceDocumentId(
    StatusCallback callback,
    const base::FilePath& source_parent_path,
    const base::FilePath& target_path,
    const std::string& source_display_name,
    const std::string& source_document_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (source_document_id.empty()) {
    std::move(callback).Run(base::File::FILE_ERROR_NOT_FOUND);
    return;
  }
  ResolveToDocumentId(
      source_parent_path,
      base::BindOnce(
          &ArcDocumentsProviderRoot::MoveFileWithSourceParentDocumentId,
          weak_ptr_factory_.GetWeakPtr(), std::move(callback),
          source_document_id, target_path, source_display_name));
}

void ArcDocumentsProviderRoot::MoveFileWithSourceParentDocumentId(
    StatusCallback callback,
    const std::string& source_document_id,
    const base::FilePath& target_path,
    const std::string& source_display_name,
    const std::string& source_parent_document_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (source_parent_document_id.empty()) {
    std::move(callback).Run(base::File::FILE_ERROR_NOT_FOUND);
    return;
  }
  const std::string target_display_name = target_path.BaseName().value();
  const std::string target_display_name_to_rename =
      source_display_name == target_display_name ? "" : target_display_name;
  ResolveToDocumentId(
      target_path.DirName(),
      base::BindOnce(
          &ArcDocumentsProviderRoot::MoveFileWithTargetParentDocumentId,
          weak_ptr_factory_.GetWeakPtr(), std::move(callback),
          source_document_id, source_parent_document_id,
          target_display_name_to_rename));
}

void ArcDocumentsProviderRoot::MoveFileWithTargetParentDocumentId(
    StatusCallback callback,
    const std::string& source_document_id,
    const std::string& source_parent_document_id,
    const std::string& target_display_name_to_rename,
    const std::string& target_parent_document_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (target_parent_document_id.empty()) {
    std::move(callback).Run(base::File::FILE_ERROR_NOT_FOUND);
    return;
  }
  runner_->MoveDocument(
      authority_, source_document_id, source_parent_document_id,
      target_parent_document_id,
      base::BindOnce(&ArcDocumentsProviderRoot::OnFileMoved,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback),
                     target_display_name_to_rename, source_parent_document_id,
                     target_parent_document_id));
}

void ArcDocumentsProviderRoot::OnFileMoved(
    StatusCallback callback,
    const std::string& target_display_name_to_rename,
    const std::string& source_parent_document_id,
    const std::string& target_parent_document_id,
    mojom::DocumentPtr document) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (document.is_null()) {
    std::move(callback).Run(base::File::FILE_ERROR_FAILED);
    return;
  }
  ClearDirectoryCache(source_parent_document_id);

  if (target_display_name_to_rename.empty()) {
    ClearDirectoryCache(target_parent_document_id);
    std::move(callback).Run(base::File::FILE_OK);
    return;
  }
  RenameFileWithParentDocumentId(
      std::move(callback), target_display_name_to_rename, document->document_id,
      target_parent_document_id);
}

void ArcDocumentsProviderRoot::AddWatcherWithDocumentId(
    const base::FilePath& path,
    uint64_t watcher_request_id,
    WatcherNotificationCallback watcher_callback,
    const std::string& document_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (IsWatcherInflightRequestCanceled(path, watcher_request_id))
    return;

  if (document_id.empty()) {
    DCHECK(path_to_watcher_data_.count(path));
    path_to_watcher_data_[path] = kInvalidWatcherData;
    return;
  }

  runner_->AddWatcher(
      authority_, document_id, std::move(watcher_callback),
      base::BindOnce(&ArcDocumentsProviderRoot::OnWatcherAdded,
                     weak_ptr_factory_.GetWeakPtr(), path, watcher_request_id));
}

void ArcDocumentsProviderRoot::OnWatcherAdded(const base::FilePath& path,
                                              uint64_t watcher_request_id,
                                              int64_t watcher_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (IsWatcherInflightRequestCanceled(path, watcher_request_id)) {
    runner_->RemoveWatcher(
        watcher_id,
        base::BindOnce(&ArcDocumentsProviderRoot::OnWatcherAddedButRemoved,
                       weak_ptr_factory_.GetWeakPtr()));
    return;
  }

  DCHECK(path_to_watcher_data_.count(path));
  path_to_watcher_data_[path] =
      WatcherData{watcher_id < 0 ? kInvalidWatcherId : watcher_id,
                  kInvalidWatcherRequestId};
}

void ArcDocumentsProviderRoot::OnWatcherAddedButRemoved(bool success) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  // Ignore |success|.
}

void ArcDocumentsProviderRoot::OnWatcherRemoved(WatcherStatusCallback callback,
                                                bool success) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  std::move(callback).Run(success ? base::File::FILE_OK
                                  : base::File::FILE_ERROR_FAILED);
}

bool ArcDocumentsProviderRoot::IsWatcherInflightRequestCanceled(
    const base::FilePath& path,
    uint64_t watcher_request_id) const {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  auto iter = path_to_watcher_data_.find(path);
  return (iter == path_to_watcher_data_.end() ||
          iter->second.inflight_request_id != watcher_request_id);
}

void ArcDocumentsProviderRoot::ResolveToContentUrlWithDocumentId(
    ResolveToContentUrlCallback callback,
    const std::string& document_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (document_id.empty()) {
    std::move(callback).Run(GURL());
    return;
  }
  std::move(callback).Run(BuildDocumentUrl(authority_, document_id));
}

void ArcDocumentsProviderRoot::GetExtraMetadataFromDocument(
    GetExtraMetadataCallback callback,
    base::File::Error error,
    const mojom::DocumentPtr& document) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (error != base::File::FILE_OK) {
    std::move(callback).Run(error, {});
    return;
  }
  DCHECK(document);

  ExtraFileMetadata metadata;
  metadata.supports_delete = document->supports_delete;
  metadata.supports_rename = document->supports_rename;
  metadata.dir_supports_create = document->dir_supports_create;
  metadata.supports_thumbnail = document->supports_thumbnail;
  if (document->last_modified > 0) {
    metadata.last_modified = base::Time::FromMillisecondsSinceUnixEpoch(
        base::checked_cast<int64_t>(document->last_modified));
  }
  metadata.size = document->size;
  std::move(callback).Run(base::File::FILE_OK, metadata);
}

void ArcDocumentsProviderRoot::GetDocument(const base::FilePath& path,
                                           GetDocumentCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  base::FilePath basename = path.BaseName();
  base::FilePath parent = path.DirName();
  if (parent.value() == base::FilePath::kCurrentDirectory)
    parent = base::FilePath();

  ResolveToDocumentId(
      parent,
      base::BindOnce(&ArcDocumentsProviderRoot::GetDocumentWithParentDocumentId,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback),
                     basename));
}

void ArcDocumentsProviderRoot::GetDocumentWithParentDocumentId(
    GetDocumentCallback callback,
    const base::FilePath& basename,
    const std::string& parent_document_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (parent_document_id.empty()) {
    std::move(callback).Run(base::File::FILE_ERROR_NOT_FOUND,
                            mojom::DocumentPtr());
    return;
  }
  ReadDirectoryInternal(
      parent_document_id, false /* force_refresh */,
      base::BindOnce(
          &ArcDocumentsProviderRoot::GetDocumentWithNameToDocumentMap,
          weak_ptr_factory_.GetWeakPtr(), std::move(callback), basename));
}

void ArcDocumentsProviderRoot::GetDocumentWithNameToDocumentMap(
    GetDocumentCallback callback,
    const base::FilePath& basename,
    base::File::Error error,
    const NameToDocumentMap& mapping) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (error != base::File::FILE_OK) {
    std::move(callback).Run(error, mojom::DocumentPtr());
    return;
  }

  auto iter = mapping.find(basename.value());
  if (iter == mapping.end()) {
    std::move(callback).Run(base::File::FILE_ERROR_NOT_FOUND,
                            mojom::DocumentPtr());
    return;
  }

  std::move(callback).Run(base::File::FILE_OK, iter->second);
}

void ArcDocumentsProviderRoot::ResolveToDocumentId(
    const base::FilePath& path,
    ResolveToDocumentIdCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  ResolveToDocumentIdRecursively(root_document_id_, path.GetComponents(),
                                 std::move(callback));
}

void ArcDocumentsProviderRoot::ResolveToDocumentIdRecursively(
    const std::string& document_id,
    const std::vector<base::FilePath::StringType>& components,
    ResolveToDocumentIdCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (components.empty()) {
    std::move(callback).Run(document_id);
    return;
  }
  ReadDirectoryInternal(
      document_id, false /* force_refresh */,
      base::BindOnce(&ArcDocumentsProviderRoot::
                         ResolveToDocumentIdRecursivelyWithNameToDocumentMap,
                     weak_ptr_factory_.GetWeakPtr(), components,
                     std::move(callback)));
}

void ArcDocumentsProviderRoot::
    ResolveToDocumentIdRecursivelyWithNameToDocumentMap(
        const std::vector<base::FilePath::StringType>& components,
        ResolveToDocumentIdCallback callback,
        base::File::Error error,
        const NameToDocumentMap& mapping) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(!components.empty());
  if (error != base::File::FILE_OK) {
    std::move(callback).Run(std::string());
    return;
  }
  auto iter = mapping.find(components[0]);
  if (iter == mapping.end()) {
    std::move(callback).Run(std::string());
    return;
  }
  ResolveToDocumentIdRecursively(iter->second->document_id,
                                 std::vector<base::FilePath::StringType>(
                                     components.begin() + 1, components.end()),
                                 std::move(callback));
}

void ArcDocumentsProviderRoot::ReadDirectoryInternal(
    const std::string& document_id,
    bool force_refresh,
    ReadDirectoryInternalCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // Use cache if possible. Note that we do not invalidate it immediately
  // even if we decide not to use it for now.
  if (!force_refresh) {
    auto iter = directory_cache_.find(document_id);
    if (iter != directory_cache_.end()) {
      std::move(callback).Run(base::File::FILE_OK, iter->second.mapping);
      return;
    }
  }

  auto& pending_callbacks = pending_callbacks_map_[document_id];
  bool read_in_flight = !pending_callbacks.empty();
  pending_callbacks.emplace_back(std::move(callback));

  if (read_in_flight) {
    // There is already an in-flight ReadDirectoryInternal() call, so
    // just enqueue the callback and return.
    return;
  }

  runner_->GetChildDocuments(
      authority_, document_id,
      base::BindOnce(
          &ArcDocumentsProviderRoot::ReadDirectoryInternalWithChildDocuments,
          weak_ptr_factory_.GetWeakPtr(), document_id));
}

void ArcDocumentsProviderRoot::ReadDirectoryInternalWithChildDocuments(
    const std::string& document_id,
    std::optional<std::vector<mojom::DocumentPtr>> maybe_children) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  auto iter = pending_callbacks_map_.find(document_id);
  DCHECK(iter != pending_callbacks_map_.end());

  std::vector<ReadDirectoryInternalCallback> pending_callbacks =
      std::move(iter->second);
  DCHECK(!pending_callbacks.empty());

  pending_callbacks_map_.erase(iter);

  if (!maybe_children) {
    for (auto& callback : pending_callbacks)
      std::move(callback).Run(base::File::FILE_ERROR_NOT_FOUND,
                              NameToDocumentMap());
    return;
  }

  std::vector<mojom::DocumentPtr> children = std::move(maybe_children.value());

  // Sort entries to keep the mapping stable as far as possible.
  std::sort(children.begin(), children.end(),
            [](const mojom::DocumentPtr& a, const mojom::DocumentPtr& b) {
              return a->document_id < b->document_id;
            });

  NameToDocumentMap mapping;
  std::map<base::FilePath::StringType, int> suffix_counters;

  for (mojom::DocumentPtr& document : children) {
    base::FilePath::StringType filename = GetFileNameForDocument(document);

    if (mapping.count(filename) > 0) {
      // Resolve a conflict by adding a suffix.
      int& suffix_counter = suffix_counters[filename];
      while (true) {
        ++suffix_counter;
        std::string suffix = base::StringPrintf(" (%d)", suffix_counter);
        base::FilePath::StringType new_filename =
            base::FilePath(filename).InsertBeforeExtensionASCII(suffix).value();
        if (mapping.count(new_filename) == 0) {
          filename = new_filename;
          break;
        }
      }
    }

    DCHECK_EQ(0u, mapping.count(filename));

    mapping[filename] = std::move(document);
  }

  // This may create a new cache, or just update an existing cache.
  DirectoryCache& cache = directory_cache_[document_id];
  cache.mapping = std::move(mapping);
  cache.clear_timer.Start(
      FROM_HERE,
      directory_cache_expire_soon_ ? base::TimeDelta() : kCacheExpiration,
      base::BindOnce(&ArcDocumentsProviderRoot::ClearDirectoryCache,
                     weak_ptr_factory_.GetWeakPtr(), document_id));

  for (auto& callback : pending_callbacks)
    std::move(callback).Run(base::File::FILE_OK, cache.mapping);
}

void ArcDocumentsProviderRoot::ClearDirectoryCache(
    const std::string& document_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  directory_cache_.erase(document_id);
}

}  // namespace arc
