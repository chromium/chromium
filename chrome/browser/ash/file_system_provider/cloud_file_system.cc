// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/file_system_provider/cloud_file_system.h"

#include <memory>
#include <utility>

#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/notreached.h"
#include "base/rand_util.h"
#include "base/strings/string_util.h"
#include "base/timer/timer.h"
#include "chrome/browser/ash/file_manager/fileapi_util.h"
#include "chrome/browser/ash/file_system_provider/cloud_file_info.h"
#include "chrome/browser/ash/file_system_provider/provided_file_system_interface.h"
#include "chrome/browser/ash/file_system_provider/queue.h"
#include "net/base/io_buffer.h"
#include "url/origin.h"

namespace ash::file_system_provider {

namespace {

// The frequency that the FSP syncs with the cloud when the File Manager is a
// watcher.
constexpr base::TimeDelta kFileManagerWatcherInterval = base::Seconds(15);

// TODO(b/317137739): Remove this once a proper API call is introduced.
// Temp custom action to request ODFS sync with the cloud.
constexpr char kODFSSyncWithCloudAction[] = "HIDDEN_SYNC_WITH_CLOUD";

const GURL GetContentCacheURL() {
  return GURL("chrome://content-cache/");
}

std::ostream& operator<<(std::ostream& out,
                         const std::vector<base::FilePath>& entry_paths) {
  for (size_t i = 0; i < entry_paths.size(); ++i) {
    out << entry_paths[i];
    if (i < entry_paths.size() - 1) {
      out << ", ";
    }
  }
  return out;
}

std::ostream& operator<<(std::ostream& out, OpenFileMode mode) {
  switch (mode) {
    case OpenFileMode::OPEN_FILE_MODE_READ:
      return out << "OPEN_FILE_MODE_READ";
    case OpenFileMode::OPEN_FILE_MODE_WRITE:
      return out << "OPEN_FILE_MODE_WRITE";
  }
  NOTREACHED() << "Unknown OpenFileMode: " << mode;
}

std::ostream& operator<<(std::ostream& out,
                         storage::WatcherManager::ChangeType type) {
  using ChangeType = storage::WatcherManager::ChangeType;
  switch (type) {
    case ChangeType::CHANGED:
      return out << "CHANGED";
    case ChangeType::DELETED:
      return out << "DELETED";
  }
  NOTREACHED() << "Unknown ChangeType: " << type;
}

std::ostream& operator<<(std::ostream& out, CloudFileInfo* cloud_file_info) {
  if (!cloud_file_info) {
    return out << "none";
  }
  return out << "{version_tag = '" << cloud_file_info->version_tag << "'}";
}

std::ostream& operator<<(std::ostream& out, EntryMetadata* metadata) {
  if (!metadata) {
    return out << "none";
  }
  out << "{ cloud_file_info = " << metadata->cloud_file_info.get();
  if (metadata->size) {
    out << ", size = '" << *metadata->size << "'";
  }
  return out << "}";
}

std::ostream& operator<<(std::ostream& out,
                         ProvidedFileSystemObserver::Changes* changes) {
  if (!changes) {
    return out << "none";
  }
  for (size_t i = 0; i < changes->size(); ++i) {
    const auto& [entry_path, change_type, cloud_file_info] = (*changes)[i];
    out << entry_path << ": change_type = " << change_type
        << ", cloud_file_info = " << cloud_file_info.get();
    if (i < changes->size() - 1) {
      out << ", ";
    }
  }
  return out;
}

const std::string GetVersionTag(EntryMetadata* metadata) {
  return (metadata && metadata->cloud_file_info)
             ? metadata->cloud_file_info->version_tag
             : "";
}

std::optional<int64_t> GetCloudSize(EntryMetadata* metadata) {
  // If the size doesn't exist, it may return -1, let's avoid this error case.
  if (metadata && metadata->size && *metadata->size > -1) {
    return *metadata->size;
  }
  return std::nullopt;
}

}  // namespace

CloudFileSystem::CloudFileSystem(
    std::unique_ptr<ProvidedFileSystemInterface> file_system)
    : CloudFileSystem(std::move(file_system), nullptr) {}

CloudFileSystem::CloudFileSystem(
    std::unique_ptr<ProvidedFileSystemInterface> file_system,
    CacheManager* cache_manager)
    : file_system_(std::move(file_system)) {
  if (!cache_manager) {
    return;
  }

  cache_manager->InitializeForProvider(
      file_system_->GetFileSystemInfo(),
      base::BindOnce(&CloudFileSystem::OnContentCacheInitialized,
                     weak_ptr_factory_.GetWeakPtr()));
}

CloudFileSystem::~CloudFileSystem() = default;

void CloudFileSystem::OnContentCacheInitialized(
    base::FileErrorOr<std::unique_ptr<ContentCache>> error_or_cache) {
  LOG_IF(ERROR, !error_or_cache.has_value())
      << "Error initializing the content cache: " << error_or_cache.error();
  if (error_or_cache.has_value()) {
    content_cache_ = std::move(error_or_cache.value());
    content_cache_->AddObserver(this);
    for (const base::FilePath& file_path :
         content_cache_->GetCachedFilePaths()) {
      AddWatcherOnCachedFile(file_path);
    }
  }
}

void CloudFileSystem::AddWatcherOnCachedFile(const base::FilePath& file_path) {
  AddWatcherOnCachedFileImpl(file_path, /*attempts=*/0,
                             /*result=*/base::File::FILE_ERROR_SECURITY);
}

void CloudFileSystem::AddWatcherOnCachedFileImpl(
    const base::FilePath& file_path,
    int attempts,
    base::File::Error result) {
  if (result == base::File::FILE_OK) {
    VLOG(1) << "Re-added file watcher on file '" << file_path << "'";
    return;
  }
  if (result != base::File::FILE_ERROR_SECURITY || attempts > 6) {
    LOG(ERROR) << "Failed to add file watcher on file with result: " << result
               << " after " << attempts << " attempts";
    VLOG(2) << "Failed to add file watcher on file '" << file_path
            << "' with result: " << result << " after " << attempts
            << " attempts";
    return;
  }
  // Set a random delay in the interval attempts*[0,2] seconds to stagger
  // AddWatcher requests.
  base::TimeDelta delay = attempts * base::Milliseconds(base::RandInt(1, 2000));
  // Notifications are received though Notify() so no notification_callback
  // is needed. Call this function recursively to continuously retry upon
  // FILE_ERROR_SECURITY errors until the max number of attempts have been made.
  base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(
          IgnoreResult(&CloudFileSystem::AddWatcher),
          weak_ptr_factory_.GetWeakPtr(), GetContentCacheURL(), file_path,
          /*recursive=*/false, /*persistent=*/false,
          base::BindOnce(&CloudFileSystem::AddWatcherOnCachedFileImpl,
                         weak_ptr_factory_.GetWeakPtr(), file_path,
                         attempts + 1),
          base::DoNothing()),
      delay);
}

AbortCallback CloudFileSystem::RequestUnmount(
    storage::AsyncFileUtil::StatusCallback callback) {
  VLOG(2) << "RequestUnmount {fsid = " << GetFileSystemId() << "}";
  return file_system_->RequestUnmount(std::move(callback));
}

AbortCallback CloudFileSystem::GetMetadata(const base::FilePath& entry_path,
                                            MetadataFieldMask fields,
                                            GetMetadataCallback callback) {
  VLOG(2) << "GetMetadata {fsid = '" << GetFileSystemId() << "', entry_path = '"
          << entry_path << "', fields = '" << fields << "'}";
  fields |= METADATA_FIELD_CLOUD_FILE_INFO;
  return file_system_->GetMetadata(
      entry_path, fields,
      base::BindOnce(&CloudFileSystem::OnGetMetadataCompleted,
                     weak_ptr_factory_.GetWeakPtr(), entry_path,
                     std::move(callback)));
}

AbortCallback CloudFileSystem::GetActions(
    const std::vector<base::FilePath>& entry_paths,
    GetActionsCallback callback) {
  VLOG(2) << "GetActions {fsid = '" << GetFileSystemId() << "', entry_paths = '"
          << entry_paths << "'}";
  return file_system_->GetActions(entry_paths, std::move(callback));
}

AbortCallback CloudFileSystem::ExecuteAction(
    const std::vector<base::FilePath>& entry_paths,
    const std::string& action_id,
    storage::AsyncFileUtil::StatusCallback callback) {
  VLOG(2) << "ExecuteAction {fsid = '" << GetFileSystemId()
          << "', entry_paths = '" << entry_paths << "', action_id = '"
          << action_id << "'}";
  return file_system_->ExecuteAction(entry_paths, action_id,
                                     std::move(callback));
}

AbortCallback CloudFileSystem::ReadDirectory(
    const base::FilePath& directory_path,
    storage::AsyncFileUtil::ReadDirectoryCallback callback) {
  VLOG(1) << "ReadDirectory {fsid = '" << GetFileSystemId()
          << "', directory_path = '" << directory_path << "'}";
  return file_system_->ReadDirectory(directory_path, callback);
}

bool CloudFileSystem::ShouldAttemptToServeReadFileFromCache(
    const OpenedCloudFileMap::const_iterator it) {
  return content_cache_ && it != opened_files_.end() &&
         it->second.mode == OpenFileMode::OPEN_FILE_MODE_READ &&
         !it->second.version_tag.empty() &&
         it->second.bytes_in_cloud.has_value();
}

AbortCallback CloudFileSystem::ReadFile(int file_handle,
                                        net::IOBuffer* buffer,
                                        int64_t offset,
                                        int length,
                                        ReadChunkReceivedCallback callback) {
  VLOG(1) << "ReadFile {fsid = '" << GetFileSystemId() << "', file_handle = '"
          << file_handle << "', offset = '" << offset << "', length = '"
          << length << "'}";

  // In the event the file isn't found in the `opened_files_` map, the content
  // cache hasn't or won't be initialized OR there is an empty `version_tag`,
  // then pass the request directly to the FSP.
  const OpenedCloudFileMap::const_iterator it = opened_files_.find(file_handle);
  if (!ShouldAttemptToServeReadFileFromCache(it)) {
    return file_system_->ReadFile(file_handle, buffer, offset, length,
                                  callback);
  }

  const OpenedCloudFile& opened_cloud_file = it->second;
  scoped_refptr<net::IOBuffer> buffer_ref = base::WrapRefCounted(buffer);
  content_cache_->ReadBytes(
      opened_cloud_file, buffer_ref, offset, length,
      base::BindRepeating(&CloudFileSystem::OnReadFileFromCacheCompleted,
                          weak_ptr_factory_.GetWeakPtr(), file_handle,
                          buffer_ref, offset, length, callback));
  return AbortCallback();
}

void CloudFileSystem::OnReadFileFromCacheCompleted(
    int file_handle,
    scoped_refptr<net::IOBuffer> buffer,
    int64_t offset,
    int length,
    ReadChunkReceivedCallback callback,
    int bytes_read,
    bool has_more,
    base::File::Error result) {
  VLOG(2) << "OnReadFileFromCacheCompleted {fsid = " << GetFileSystemId()
          << ", file_handle = '" << file_handle << "', result = '" << result
          << "}";
  if (result == base::File::FILE_OK) {
    // If the cached read file was successful, ensure that is passed to the
    // caller.
    callback.Run(bytes_read, has_more, result);
    return;
  }

  if (result == base::File::FILE_ERROR_NOT_FOUND) {
    // The file doesn't exist in the cache or is not available, we need to make
    // a cloud request first and attempt to write the result into the cache upon
    // successful return.
    file_system_->ReadFile(
        file_handle, buffer.get(), offset, length,
        base::BindRepeating(&CloudFileSystem::OnReadFileCompleted,
                            weak_ptr_factory_.GetWeakPtr(), file_handle, buffer,
                            offset, length, callback));
    return;
  }

  LOG(ERROR) << "Couldn't read the file from cache";
  file_system_->ReadFile(file_handle, buffer.get(), offset, length,
                         std::move(callback));
}

void CloudFileSystem::OnReadFileCompleted(int file_handle,
                                          scoped_refptr<net::IOBuffer> buffer,
                                          int64_t offset,
                                          int length,
                                          ReadChunkReceivedCallback callback,
                                          int bytes_read,
                                          bool has_more,
                                          base::File::Error result) {
  VLOG(2) << "OnReadFileCompleted {fsid = " << GetFileSystemId()
          << ", file_handle = '" << file_handle << "', result = '" << result
          << ", bytes_read = " << bytes_read << "}";

  const OpenedCloudFileMap::const_iterator it = opened_files_.find(file_handle);
  if (it == opened_files_.end() || result != base::File::FILE_OK ||
      !content_cache_) {
    callback.Run(bytes_read, has_more, result);
    return;
  }

  // The `ReadChunkReceivedCallback` should always respond with the result from
  // the FSP. If the content cache write fails, we should always be serving this
  // from the FSP.
  auto readchunk_success_callback = base::BindRepeating(
      std::move(callback), bytes_read, has_more, base::File::FILE_OK);

  const OpenedCloudFile& opened_cloud_file = it->second;
  content_cache_->WriteBytes(
      opened_cloud_file, buffer, offset, bytes_read,
      base::BindOnce(&CloudFileSystem::OnBytesWrittenToCache,
                     weak_ptr_factory_.GetWeakPtr(),
                     opened_cloud_file.file_path, readchunk_success_callback));
}

void CloudFileSystem::OnBytesWrittenToCache(
    const base::FilePath& file_path,
    base::RepeatingCallback<void()> readchunk_success_callback,
    base::File::Error result) {
  if (result == base::File::FILE_OK) {
    Watchers::const_iterator watcher =
        GetWatchers()->find(WatcherKey(file_path, /*recursive=*/false));
    if (watcher == GetWatchers()->end() ||
        !watcher->second.subscribers.contains(GetContentCacheURL())) {
      // This file is newly added to the cache so watch it to track any changes.
      // Notifications are received though Notify() so no notification_callback
      // is needed.
      AddWatcher(
          GetContentCacheURL(), file_path,
          /*recursive=*/false, /*persistent=*/false,
          base::BindOnce(
              [](const base::FilePath& file_path, base::File::Error result) {
                VLOG(1) << "Added file watcher on '" << file_path
                        << "' result: " << result;
              },
              file_path),
          base::DoNothing());
    }
  }
  readchunk_success_callback.Run();
}

AbortCallback CloudFileSystem::OpenFile(const base::FilePath& file_path,
                                         OpenFileMode mode,
                                         OpenFileCallback callback) {
  VLOG(1) << "OpenFile {fsid = '" << GetFileSystemId() << "', file_path = '"
          << file_path << "', mode = '" << mode << "'}";

  return file_system_->OpenFile(
      file_path, mode,
      base::BindOnce(&CloudFileSystem::OnOpenFileCompleted,
                     weak_ptr_factory_.GetWeakPtr(), file_path, mode,
                     std::move(callback)));
}

AbortCallback CloudFileSystem::CloseFile(
    int file_handle,
    storage::AsyncFileUtil::StatusCallback callback) {
  VLOG(1) << "CloseFile {fsid = '" << GetFileSystemId() << "', file_handle = '"
          << file_handle << "'}";
  return file_system_->CloseFile(
      file_handle, base::BindOnce(&CloudFileSystem::OnCloseFileCompleted,
                                  weak_ptr_factory_.GetWeakPtr(), file_handle,
                                  std::move(callback)));
}

AbortCallback CloudFileSystem::CreateDirectory(
    const base::FilePath& directory_path,
    bool recursive,
    storage::AsyncFileUtil::StatusCallback callback) {
  VLOG(1) << "CreateDirectory {fsid = '" << GetFileSystemId()
          << "', directory_path = '" << directory_path << "', recursive = '"
          << recursive << "'}";
  return file_system_->CreateDirectory(directory_path, recursive,
                                       std::move(callback));
}

AbortCallback CloudFileSystem::DeleteEntry(
    const base::FilePath& entry_path,
    bool recursive,
    storage::AsyncFileUtil::StatusCallback callback) {
  VLOG(1) << "DeleteEntry {fsid = '" << GetFileSystemId() << "', entry_path = '"
          << entry_path << "', recursive = '" << recursive << "'}";
  return file_system_->DeleteEntry(
      entry_path, recursive,
      base::BindOnce(&CloudFileSystem::OnDeleteEntryCompleted,
                     weak_ptr_factory_.GetWeakPtr(), entry_path,
                     std::move(callback)));
}

AbortCallback CloudFileSystem::CreateFile(
    const base::FilePath& file_path,
    storage::AsyncFileUtil::StatusCallback callback) {
  VLOG(1) << "CreateFile {fsid = '" << GetFileSystemId() << "', file_path = '"
          << file_path << "'}";
  return file_system_->CreateFile(file_path, std::move(callback));
}

AbortCallback CloudFileSystem::CopyEntry(
    const base::FilePath& source_path,
    const base::FilePath& target_path,
    storage::AsyncFileUtil::StatusCallback callback) {
  VLOG(1) << "CopyEntry {fsid = '" << GetFileSystemId() << "', source_path = '"
          << source_path << "', target_path = '" << target_path << "'}";
  return file_system_->CopyEntry(source_path, target_path, std::move(callback));
}

AbortCallback CloudFileSystem::WriteFile(
    int file_handle,
    net::IOBuffer* buffer,
    int64_t offset,
    int length,
    storage::AsyncFileUtil::StatusCallback callback) {
  VLOG(1) << "WriteFile {fsid = '" << GetFileSystemId() << "', file_handle = '"
          << file_handle << "', offset = '" << offset << "', length = '"
          << length << "'}";
  return file_system_->WriteFile(
      file_handle, buffer, offset, length,
      base::BindOnce(&CloudFileSystem::OnWriteFileCompleted,
                     weak_ptr_factory_.GetWeakPtr(), file_handle,
                     std::move(callback)));
}

AbortCallback CloudFileSystem::FlushFile(
    int file_handle,
    storage::AsyncFileUtil::StatusCallback callback) {
  VLOG(1) << "FlushFile {fsid = '" << GetFileSystemId() << "', file_handle = '"
          << file_handle << "'}";
  return file_system_->FlushFile(file_handle, std::move(callback));
}

AbortCallback CloudFileSystem::MoveEntry(
    const base::FilePath& source_path,
    const base::FilePath& target_path,
    storage::AsyncFileUtil::StatusCallback callback) {
  VLOG(1) << "MoveEntry {fsid = '" << GetFileSystemId() << "', source_path = '"
          << source_path << "', target_path = '" << target_path << "'}";
  return file_system_->MoveEntry(source_path, target_path, std::move(callback));
}

AbortCallback CloudFileSystem::Truncate(
    const base::FilePath& file_path,
    int64_t length,
    storage::AsyncFileUtil::StatusCallback callback) {
  VLOG(1) << "Truncate {fsid = '" << GetFileSystemId() << "', file_path = '"
          << file_path << "', length = '" << length << "'}";
  return file_system_->Truncate(file_path, length, std::move(callback));
}

AbortCallback CloudFileSystem::AddWatcher(
    const GURL& origin,
    const base::FilePath& entry_path,
    bool recursive,
    bool persistent,
    storage::AsyncFileUtil::StatusCallback callback,
    storage::WatcherManager::NotificationCallback notification_callback) {
  VLOG(2) << "AddWatcher {fsid = '" << GetFileSystemId() << "', origin = '"
          << origin.spec() << "', entry_path = '" << entry_path
          << "', recursive = '" << recursive << "', persistent = '"
          << persistent << "'}";

  // Set timer if the File Manager is a watcher.
  file_manager_watchers_ +=
      file_manager::util::IsFileManagerURL(origin) ? 1 : 0;
  if (file_manager_watchers_ > 0 && !timer_.IsRunning()) {
    timer_.Start(FROM_HERE, kFileManagerWatcherInterval,
                 base::BindRepeating(&CloudFileSystem::OnTimer,
                                     weak_ptr_factory_.GetWeakPtr()));
  }

  return file_system_->AddWatcher(origin, entry_path, recursive, persistent,
                                  std::move(callback),
                                  std::move(notification_callback));
}

void CloudFileSystem::RemoveWatcher(
    const GURL& origin,
    const base::FilePath& entry_path,
    bool recursive,
    storage::AsyncFileUtil::StatusCallback callback) {
  VLOG(2) << "RemoveWatcher {fsid = '" << GetFileSystemId() << "', origin = '"
          << origin.spec() << "', entry_path = '" << entry_path
          << "', recursive = '" << recursive << "'}";

  // Stop timer if the File Manager is not a watcher.
  file_manager_watchers_ -=
      file_manager::util::IsFileManagerURL(origin) ? 1 : 0;
  if (file_manager_watchers_ == 0 && timer_.IsRunning()) {
    timer_.Stop();
  }

  file_system_->RemoveWatcher(origin, entry_path, recursive,
                              std::move(callback));
}

const ProvidedFileSystemInfo& CloudFileSystem::GetFileSystemInfo() const {
  return file_system_->GetFileSystemInfo();
}

OperationRequestManager* CloudFileSystem::GetRequestManager() {
  return file_system_->GetRequestManager();
}

Watchers* CloudFileSystem::GetWatchers() {
  return file_system_->GetWatchers();
}

const OpenedFiles& CloudFileSystem::GetOpenedFiles() const {
  return file_system_->GetOpenedFiles();
}

void CloudFileSystem::AddObserver(ProvidedFileSystemObserver* observer) {
  file_system_->AddObserver(observer);
}

void CloudFileSystem::RemoveObserver(ProvidedFileSystemObserver* observer) {
  file_system_->RemoveObserver(observer);
}

void CloudFileSystem::Notify(
    const base::FilePath& entry_path,
    bool recursive,
    storage::WatcherManager::ChangeType change_type,
    std::unique_ptr<ProvidedFileSystemObserver::Changes> changes,
    const std::string& tag,
    storage::AsyncFileUtil::StatusCallback callback) {
  VLOG(2) << "Notify {fsid = '" << GetFileSystemId() << "', recursive = '"
          << recursive << "', change_type = '" << change_type << "', tag = '"
          << tag << "', changes = {" << changes.get() << "}}";

  if (content_cache_ && changes) {
    content_cache_->Notify(*changes);
  }

  return file_system_->Notify(entry_path, recursive, change_type,
                              std::move(changes), tag, std::move(callback));
}

void CloudFileSystem::Configure(
    storage::AsyncFileUtil::StatusCallback callback) {
  return file_system_->Configure(std::move(callback));
}

base::WeakPtr<ProvidedFileSystemInterface> CloudFileSystem::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

std::unique_ptr<ScopedUserInteraction>
CloudFileSystem::StartUserInteraction() {
  return file_system_->StartUserInteraction();
}

const std::string CloudFileSystem::GetFileSystemId() const {
  return file_system_->GetFileSystemInfo().file_system_id();
}

void CloudFileSystem::OnTimer() {
  VLOG(2) << "OnTimer";
  // TODO(b/317137739): Replace this with a proper API call once one is
  // introduced.
  // Request that the file system syncs with the Cloud. The entry path is
  // insignficant, just pass it root.
  ExecuteAction({base::FilePath("/")}, kODFSSyncWithCloudAction,
                base::BindOnce([](base::File::Error result) {
                  VLOG(1) << "Action " << kODFSSyncWithCloudAction
                          << " completed: " << result;
                }));
}

void CloudFileSystem::OnOpenFileCompleted(
    const base::FilePath& file_path,
    OpenFileMode mode,
    OpenFileCallback callback,
    int file_handle,
    base::File::Error result,
    std::unique_ptr<EntryMetadata> metadata) {
  VLOG(2) << "OnOpenFileCompleted {fsid = " << GetFileSystemId()
          << ", file_handle = '" << file_handle << "', result = '" << result
          << "', metadata = " << metadata.get() << "}";

  if (result == base::File::FILE_OK) {
    const std::string version_tag = GetVersionTag(metadata.get());
    opened_files_.try_emplace(
        file_handle, OpenedCloudFile(file_path, mode, file_handle, version_tag,
                                     GetCloudSize(metadata.get())));
    // Notify the cache with the observed version tag.
    content_cache_->ObservedVersionTag(file_path, version_tag);
  } else if (content_cache_ && result == base::File::FILE_ERROR_NOT_FOUND) {
    // The file doesn't exist on the FSP, evict it from the cache.
    content_cache_->Evict(file_path);
  }
  std::move(callback).Run(file_handle, result, std::move(metadata));
}

void CloudFileSystem::OnCloseFileCompleted(
    int file_handle,
    storage::AsyncFileUtil::StatusCallback callback,
    base::File::Error result) {
  VLOG(2) << "OnCloseFileCompleted {fsid = " << GetFileSystemId()
          << ", file_handle = '" << file_handle << "', result = '" << result
          << "}";
  // Closing is always final. Even if an error happened, we remove it from the
  // list of opened files.
  if (content_cache_) {
    const auto& opened_file = opened_files_.extract(file_handle);
    if (!opened_file.empty()) {
      content_cache_->CloseFile(opened_file.mapped());
    }
  }
  std::move(callback).Run(result);
}

void CloudFileSystem::OnGetMetadataCompleted(
    const base::FilePath& entry_path,
    GetMetadataCallback callback,
    std::unique_ptr<EntryMetadata> entry_metadata,
    base::File::Error result) {
  VLOG(2) << "OnGetMetadataCompleted {fsid = " << GetFileSystemId()
          << ", entry_path = '" << entry_path << "', result = '" << result
          << "', metadata = " << entry_metadata.get() << "}";

  if (content_cache_) {
    if (result == base::File::FILE_ERROR_NOT_FOUND) {
      // The file doesn't exist on the FSP, evict it from the cache.
      content_cache_->Evict(entry_path);
    } else if (result == base::File::FILE_OK) {
      // Notify the cache with the observed version tag.
      content_cache_->ObservedVersionTag(entry_path,
                                         GetVersionTag(entry_metadata.get()));
    }
  }
  std::move(callback).Run(std::move(entry_metadata), result);
}

void CloudFileSystem::OnWriteFileCompleted(
    int file_handle,
    storage::AsyncFileUtil::StatusCallback callback,
    base::File::Error result) {
  VLOG(2) << "OnWriteFileCompleted {fsid = " << GetFileSystemId()
          << ", file_handle = '" << file_handle << "', result = '" << result
          << "}";
  if (content_cache_ && result == base::File::FILE_OK) {
    const auto& opened_file = opened_files_.extract(file_handle);
    if (!opened_file.empty()) {
      // The cached file is now out of date.
      content_cache_->Evict(opened_file.mapped().file_path);
    }
  }
  std::move(callback).Run(result);
}

void CloudFileSystem::OnDeleteEntryCompleted(
    const base::FilePath& entry_path,
    storage::AsyncFileUtil::StatusCallback callback,
    base::File::Error result) {
  VLOG(2) << "OnDeleteEntryCompleted {fsid = " << GetFileSystemId()
          << ", entry_path = '" << entry_path << "', result = '" << result
          << "}";
  if (content_cache_ && result == base::File::FILE_OK) {
    // The cached file should be deleted.
    content_cache_->Evict(entry_path);
  }
  std::move(callback).Run(result);
}

void CloudFileSystem::OnItemEvicted(const base::FilePath& fsp_path) {
  VLOG(1) << fsp_path << " evicted from the content cache";
  RemoveWatcher(
      GetContentCacheURL(), fsp_path, /*recursive=*/false,
      base::BindOnce(
          [](const base::FilePath& fsp_path, base::File::Error result) {
            VLOG(1) << "Removed file watcher on '" << fsp_path
                    << "' result: " << result;
          },
          fsp_path));
}

}  // namespace ash::file_system_provider
