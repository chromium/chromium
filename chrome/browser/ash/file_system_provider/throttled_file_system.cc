// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/file_system_provider/throttled_file_system.h"

#include <stddef.h>

#include <limits>
#include <utility>
#include <vector>

#include "base/files/file.h"
#include "base/functional/bind.h"
#include "chrome/browser/ash/file_system_provider/queue.h"

namespace ash::file_system_provider {

ThrottledFileSystem::ThrottledFileSystem(
    std::unique_ptr<ProvidedFileSystemInterface> file_system)
    : file_system_(std::move(file_system)) {
  const int opened_files_limit =
      file_system_->GetFileSystemInfo().opened_files_limit();
  open_queue_.reset(opened_files_limit
                        ? new Queue(static_cast<size_t>(opened_files_limit))
                        : new Queue(std::numeric_limits<size_t>::max()));
}

ThrottledFileSystem::~ThrottledFileSystem() = default;

AbortCallback ThrottledFileSystem::RequestUnmount(
    storage::AsyncFileUtil::StatusCallback callback) {
  return file_system_->RequestUnmount(std::move(callback));
}

AbortCallback ThrottledFileSystem::GetMetadata(const base::FilePath& entry_path,
                                               MetadataFieldMask fields,
                                               GetMetadataCallback callback) {
  return file_system_->GetMetadata(entry_path, fields, std::move(callback));
}

AbortCallback ThrottledFileSystem::GetActions(
    const std::vector<base::FilePath>& entry_paths,
    GetActionsCallback callback) {
  return file_system_->GetActions(entry_paths, std::move(callback));
}

AbortCallback ThrottledFileSystem::ExecuteAction(
    const std::vector<base::FilePath>& entry_paths,
    const std::string& action_id,
    storage::AsyncFileUtil::StatusCallback callback) {
  return file_system_->ExecuteAction(entry_paths, action_id,
                                     std::move(callback));
}

AbortCallback ThrottledFileSystem::ReadDirectory(
    const base::FilePath& directory_path,
    storage::AsyncFileUtil::ReadDirectoryCallback callback) {
  return file_system_->ReadDirectory(directory_path, callback);
}

AbortCallback ThrottledFileSystem::ReadFile(
    int file_handle,
    net::IOBuffer* buffer,
    int64_t offset,
    int length,
    ReadChunkReceivedCallback callback) {
  return file_system_->ReadFile(file_handle, buffer, offset, length, callback);
}

AbortCallback ThrottledFileSystem::OpenFile(const base::FilePath& file_path,
                                            OpenFileMode mode,
                                            OpenFileCallback callback) {
  const size_t task_token = open_queue_->NewToken();
  open_queue_->Enqueue(
      task_token,
      base::BindOnce(
          &ProvidedFileSystemInterface::OpenFile,
          base::Unretained(file_system_.get()),  // Outlives the queue.
          file_path, mode,
          base::BindOnce(&ThrottledFileSystem::OnOpenFileCompleted,
                         weak_ptr_factory_.GetWeakPtr(), task_token,
                         std::move(callback))));
  return base::BindOnce(&ThrottledFileSystem::Abort,
                        weak_ptr_factory_.GetWeakPtr(), task_token);
}

AbortCallback ThrottledFileSystem::CloseFile(
    int file_handle,
    storage::AsyncFileUtil::StatusCallback callback) {
  return file_system_->CloseFile(
      file_handle, base::BindOnce(&ThrottledFileSystem::OnCloseFileCompleted,
                                  weak_ptr_factory_.GetWeakPtr(), file_handle,
                                  std::move(callback)));
}

AbortCallback ThrottledFileSystem::CreateDirectory(
    const base::FilePath& directory_path,
    bool recursive,
    storage::AsyncFileUtil::StatusCallback callback) {
  return file_system_->CreateDirectory(directory_path, recursive,
                                       std::move(callback));
}

AbortCallback ThrottledFileSystem::DeleteEntry(
    const base::FilePath& entry_path,
    bool recursive,
    storage::AsyncFileUtil::StatusCallback callback) {
  return file_system_->DeleteEntry(entry_path, recursive, std::move(callback));
}

AbortCallback ThrottledFileSystem::CreateFile(
    const base::FilePath& file_path,
    storage::AsyncFileUtil::StatusCallback callback) {
  return file_system_->CreateFile(file_path, std::move(callback));
}

AbortCallback ThrottledFileSystem::CopyEntry(
    const base::FilePath& source_path,
    const base::FilePath& target_path,
    storage::AsyncFileUtil::StatusCallback callback) {
  return file_system_->CopyEntry(source_path, target_path, std::move(callback));
}

AbortCallback ThrottledFileSystem::WriteFile(
    int file_handle,
    net::IOBuffer* buffer,
    int64_t offset,
    int length,
    storage::AsyncFileUtil::StatusCallback callback) {
  return file_system_->WriteFile(file_handle, buffer, offset, length,
                                 std::move(callback));
}

AbortCallback ThrottledFileSystem::FlushFile(
    int file_handle,
    storage::AsyncFileUtil::StatusCallback callback) {
  return file_system_->FlushFile(file_handle, std::move(callback));
}

AbortCallback ThrottledFileSystem::MoveEntry(
    const base::FilePath& source_path,
    const base::FilePath& target_path,
    storage::AsyncFileUtil::StatusCallback callback) {
  return file_system_->MoveEntry(source_path, target_path, std::move(callback));
}

AbortCallback ThrottledFileSystem::Truncate(
    const base::FilePath& file_path,
    int64_t length,
    storage::AsyncFileUtil::StatusCallback callback) {
  return file_system_->Truncate(file_path, length, std::move(callback));
}

AbortCallback ThrottledFileSystem::AddWatcher(
    const GURL& origin,
    const base::FilePath& entry_path,
    bool recursive,
    bool persistent,
    storage::AsyncFileUtil::StatusCallback callback,
    storage::WatcherManager::NotificationCallback notification_callback) {
  return file_system_->AddWatcher(origin, entry_path, recursive, persistent,
                                  std::move(callback),
                                  std::move(notification_callback));
}

void ThrottledFileSystem::RemoveWatcher(
    const GURL& origin,
    const base::FilePath& entry_path,
    bool recursive,
    storage::AsyncFileUtil::StatusCallback callback) {
  file_system_->RemoveWatcher(origin, entry_path, recursive,
                              std::move(callback));
}

const ProvidedFileSystemInfo& ThrottledFileSystem::GetFileSystemInfo() const {
  return file_system_->GetFileSystemInfo();
}

OperationRequestManager* ThrottledFileSystem::GetRequestManager() {
  return file_system_->GetRequestManager();
}

Watchers* ThrottledFileSystem::GetWatchers() {
  return file_system_->GetWatchers();
}

const OpenedFiles& ThrottledFileSystem::GetOpenedFiles() const {
  return file_system_->GetOpenedFiles();
}

void ThrottledFileSystem::AddObserver(ProvidedFileSystemObserver* observer) {
  file_system_->AddObserver(observer);
}

void ThrottledFileSystem::RemoveObserver(ProvidedFileSystemObserver* observer) {
  file_system_->RemoveObserver(observer);
}

void ThrottledFileSystem::Notify(
    const base::FilePath& entry_path,
    bool recursive,
    storage::WatcherManager::ChangeType change_type,
    std::unique_ptr<ProvidedFileSystemObserver::Changes> changes,
    const std::string& tag,
    storage::AsyncFileUtil::StatusCallback callback) {
  return file_system_->Notify(entry_path, recursive, change_type,
                              std::move(changes), tag, std::move(callback));
}

void ThrottledFileSystem::Configure(
    storage::AsyncFileUtil::StatusCallback callback) {
  return file_system_->Configure(std::move(callback));
}

base::WeakPtr<ProvidedFileSystemInterface> ThrottledFileSystem::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

std::unique_ptr<ScopedUserInteraction>
ThrottledFileSystem::StartUserInteraction() {
  return file_system_->StartUserInteraction();
}

void ThrottledFileSystem::Abort(int queue_token) {
  open_queue_->Abort(queue_token);
}

void ThrottledFileSystem::OnOpenFileCompleted(
    int queue_token,
    OpenFileCallback callback,
    int file_handle,
    base::File::Error result,
    std::unique_ptr<EntryMetadata> metadata) {
  // If the file is opened successfully then hold the queue token until the file
  // is closed.
  if (result == base::File::FILE_OK)
    opened_files_[file_handle] = queue_token;
  else
    open_queue_->Complete(queue_token);

  std::move(callback).Run(file_handle, result, std::move(metadata));
}

void ThrottledFileSystem::OnCloseFileCompleted(
    int file_handle,
    storage::AsyncFileUtil::StatusCallback callback,
    base::File::Error result) {
  // Closing is always final. Even if an error happened, the file is considered
  // closed on the C++ side. Release the task from the queue, so other files
  // which are enqueued can be opened.
  const auto it = opened_files_.find(file_handle);
  DCHECK(it != opened_files_.end());

  const int queue_token = it->second;
  open_queue_->Complete(queue_token);
  opened_files_.erase(file_handle);

  std::move(callback).Run(result);
}

}  // namespace ash::file_system_provider
