// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/smb_client/smb_file_system.h"

#include <memory>

#include "base/notreached.h"
#include "chrome/browser/ash/file_system_provider/service.h"
#include "chrome/browser/ash/smb_client/smb_file_system_id.h"
#include "components/services/filesystem/public/mojom/types.mojom.h"

namespace ash::smb_client {

using file_system_provider::AbortCallback;

SmbFileSystem::SmbFileSystem(
    const file_system_provider::ProvidedFileSystemInfo& file_system_info)
    : file_system_info_(file_system_info) {}

SmbFileSystem::~SmbFileSystem() = default;

AbortCallback SmbFileSystem::RequestUnmount(
    storage::AsyncFileUtil::StatusCallback callback) {
  NOTREACHED_IN_MIGRATION();
  return base::DoNothing();
}

AbortCallback SmbFileSystem::GetMetadata(
    const base::FilePath& entry_path,
    ProvidedFileSystemInterface::MetadataFieldMask fields,
    ProvidedFileSystemInterface::GetMetadataCallback callback) {
  NOTREACHED_IN_MIGRATION();
  return base::DoNothing();
}

AbortCallback SmbFileSystem::GetActions(
    const std::vector<base::FilePath>& entry_paths,
    GetActionsCallback callback) {
  const std::vector<file_system_provider::Action> actions;
  // No actions are currently supported.
  std::move(callback).Run(actions, base::File::FILE_OK);
  return base::DoNothing();
}

AbortCallback SmbFileSystem::ExecuteAction(
    const std::vector<base::FilePath>& entry_paths,
    const std::string& action_id,
    storage::AsyncFileUtil::StatusCallback callback) {
  NOTREACHED_IN_MIGRATION();
  return base::DoNothing();
}

AbortCallback SmbFileSystem::ReadDirectory(
    const base::FilePath& directory_path,
    storage::AsyncFileUtil::ReadDirectoryCallback callback) {
  NOTREACHED_IN_MIGRATION();
  return base::DoNothing();
}

AbortCallback SmbFileSystem::OpenFile(const base::FilePath& file_path,
                                      file_system_provider::OpenFileMode mode,
                                      OpenFileCallback callback) {
  NOTREACHED_IN_MIGRATION();
  return base::DoNothing();
}

AbortCallback SmbFileSystem::CloseFile(
    int file_handle,
    storage::AsyncFileUtil::StatusCallback callback) {
  NOTREACHED_IN_MIGRATION();
  return base::DoNothing();
}

AbortCallback SmbFileSystem::ReadFile(int file_handle,
                                      net::IOBuffer* buffer,
                                      int64_t offset,
                                      int length,
                                      ReadChunkReceivedCallback callback) {
  NOTREACHED_IN_MIGRATION();
  return base::DoNothing();
}

AbortCallback SmbFileSystem::CreateDirectory(
    const base::FilePath& directory_path,
    bool recursive,
    storage::AsyncFileUtil::StatusCallback callback) {
  NOTREACHED_IN_MIGRATION();
  return base::DoNothing();
}

AbortCallback SmbFileSystem::CreateFile(
    const base::FilePath& file_path,
    storage::AsyncFileUtil::StatusCallback callback) {
  NOTREACHED_IN_MIGRATION();
  return base::DoNothing();
}

AbortCallback SmbFileSystem::DeleteEntry(
    const base::FilePath& entry_path,
    bool recursive,
    storage::AsyncFileUtil::StatusCallback callback) {
  NOTREACHED_IN_MIGRATION();
  return base::DoNothing();
}

AbortCallback SmbFileSystem::CopyEntry(
    const base::FilePath& source_path,
    const base::FilePath& target_path,
    storage::AsyncFileUtil::StatusCallback callback) {
  NOTREACHED_IN_MIGRATION();
  return base::DoNothing();
}

AbortCallback SmbFileSystem::MoveEntry(
    const base::FilePath& source_path,
    const base::FilePath& target_path,
    storage::AsyncFileUtil::StatusCallback callback) {
  NOTREACHED_IN_MIGRATION();
  return base::DoNothing();
}

AbortCallback SmbFileSystem::Truncate(
    const base::FilePath& file_path,
    int64_t length,
    storage::AsyncFileUtil::StatusCallback callback) {
  NOTREACHED_IN_MIGRATION();
  return base::DoNothing();
}

AbortCallback SmbFileSystem::WriteFile(
    int file_handle,
    net::IOBuffer* buffer,
    int64_t offset,
    int length,
    storage::AsyncFileUtil::StatusCallback callback) {
  NOTREACHED_IN_MIGRATION();
  return base::DoNothing();
}

AbortCallback SmbFileSystem::FlushFile(
    int file_handle,
    storage::AsyncFileUtil::StatusCallback callback) {
  NOTREACHED_IN_MIGRATION();
  return base::DoNothing();
}

AbortCallback SmbFileSystem::AddWatcher(
    const GURL& origin,
    const base::FilePath& entry_path,
    bool recursive,
    bool persistent,
    storage::AsyncFileUtil::StatusCallback callback,
    storage::WatcherManager::NotificationCallback notification_callback) {
  // Watchers are not supported.
  NOTREACHED_IN_MIGRATION();
  std::move(callback).Run(base::File::FILE_ERROR_INVALID_OPERATION);
  return base::DoNothing();
}

void SmbFileSystem::RemoveWatcher(
    const GURL& origin,
    const base::FilePath& entry_path,
    bool recursive,
    storage::AsyncFileUtil::StatusCallback callback) {
  // Watchers are not supported.
  NOTREACHED_IN_MIGRATION();
  std::move(callback).Run(base::File::FILE_ERROR_INVALID_OPERATION);
}

const file_system_provider::ProvidedFileSystemInfo&
SmbFileSystem::GetFileSystemInfo() const {
  return file_system_info_;
}

file_system_provider::OperationRequestManager*
SmbFileSystem::GetRequestManager() {
  NOTREACHED_IN_MIGRATION();
  return nullptr;
}

file_system_provider::Watchers* SmbFileSystem::GetWatchers() {
  // Watchers are not supported.
  NOTREACHED_IN_MIGRATION();
  return nullptr;
}

const file_system_provider::OpenedFiles& SmbFileSystem::GetOpenedFiles() const {
  NOTREACHED_IN_MIGRATION();
  return opened_files_;
}

void SmbFileSystem::AddObserver(
    file_system_provider::ProvidedFileSystemObserver* observer) {
  NOTREACHED_IN_MIGRATION();
}

void SmbFileSystem::RemoveObserver(
    file_system_provider::ProvidedFileSystemObserver* observer) {
  NOTREACHED_IN_MIGRATION();
}

void SmbFileSystem::SmbFileSystem::Notify(
    const base::FilePath& entry_path,
    bool recursive,
    storage::WatcherManager::ChangeType change_type,
    std::unique_ptr<file_system_provider::ProvidedFileSystemObserver::Changes>
        changes,
    const std::string& tag,
    storage::AsyncFileUtil::StatusCallback callback) {
  NOTREACHED_IN_MIGRATION();
}

void SmbFileSystem::Configure(storage::AsyncFileUtil::StatusCallback callback) {
  NOTREACHED_IN_MIGRATION();
}

base::WeakPtr<file_system_provider::ProvidedFileSystemInterface>
SmbFileSystem::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

std::unique_ptr<file_system_provider::ScopedUserInteraction>
SmbFileSystem::StartUserInteraction() {
  NOTREACHED_IN_MIGRATION();
  return nullptr;
}

}  // namespace ash::smb_client
