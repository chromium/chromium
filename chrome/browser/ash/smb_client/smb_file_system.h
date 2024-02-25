// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_SMB_CLIENT_SMB_FILE_SYSTEM_H_
#define CHROME_BROWSER_ASH_SMB_CLIENT_SMB_FILE_SYSTEM_H_

#include <stdint.h>

#include <map>
#include <string>
#include <vector>

#include "base/files/file.h"
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/file_system_provider/abort_callback.h"
#include "chrome/browser/ash/file_system_provider/provided_file_system_info.h"
#include "chrome/browser/ash/file_system_provider/provided_file_system_interface.h"
#include "chrome/browser/ash/file_system_provider/watcher.h"
#include "chrome/browser/ash/smb_client/smb_service.h"
#include "chromeos/ash/components/dbus/smbprovider/smb_provider_client.h"
#include "storage/browser/file_system/async_file_util.h"
#include "storage/browser/file_system/watcher_manager.h"
#include "url/gurl.h"

namespace net {
class IOBuffer;
}  // namespace net

namespace ash {

namespace file_system_manager {
class OperationRequestManager;
}  // namespace file_system_manager

namespace smb_client {

// SMB provided file system implementation. For communication with SMB
// filesystems.
// SMB is an application level protocol used by Windows and Samba fileservers.
// Allows Files App to mount SMB filesystems.
class SmbFileSystem : public file_system_provider::ProvidedFileSystemInterface {
 public:
  explicit SmbFileSystem(
      const file_system_provider::ProvidedFileSystemInfo& file_system_info);
  SmbFileSystem(const SmbFileSystem&) = delete;
  SmbFileSystem& operator=(const SmbFileSystem&) = delete;
  ~SmbFileSystem() override;

  // ProvidedFileSystemInterface overrides.
  file_system_provider::AbortCallback RequestUnmount(
      storage::AsyncFileUtil::StatusCallback callback) override;

  file_system_provider::AbortCallback GetMetadata(
      const base::FilePath& entry_path,
      ProvidedFileSystemInterface::MetadataFieldMask fields,
      ProvidedFileSystemInterface::GetMetadataCallback callback) override;

  file_system_provider::AbortCallback GetActions(
      const std::vector<base::FilePath>& entry_paths,
      GetActionsCallback callback) override;

  file_system_provider::AbortCallback ExecuteAction(
      const std::vector<base::FilePath>& entry_paths,
      const std::string& action_id,
      storage::AsyncFileUtil::StatusCallback callback) override;

  file_system_provider::AbortCallback ReadDirectory(
      const base::FilePath& directory_path,
      storage::AsyncFileUtil::ReadDirectoryCallback callback) override;

  file_system_provider::AbortCallback OpenFile(
      const base::FilePath& file_path,
      file_system_provider::OpenFileMode mode,
      OpenFileCallback callback) override;

  file_system_provider::AbortCallback CloseFile(
      int file_handle,
      storage::AsyncFileUtil::StatusCallback callback) override;

  file_system_provider::AbortCallback ReadFile(
      int file_handle,
      net::IOBuffer* buffer,
      int64_t offset,
      int length,
      ReadChunkReceivedCallback callback) override;

  file_system_provider::AbortCallback CreateDirectory(
      const base::FilePath& directory_path,
      bool recursive,
      storage::AsyncFileUtil::StatusCallback callback) override;

  file_system_provider::AbortCallback CreateFile(
      const base::FilePath& file_path,
      storage::AsyncFileUtil::StatusCallback callback) override;

  file_system_provider::AbortCallback DeleteEntry(
      const base::FilePath& entry_path,
      bool recursive,
      storage::AsyncFileUtil::StatusCallback callback) override;

  file_system_provider::AbortCallback CopyEntry(
      const base::FilePath& source_path,
      const base::FilePath& target_path,
      storage::AsyncFileUtil::StatusCallback callback) override;

  file_system_provider::AbortCallback MoveEntry(
      const base::FilePath& source_path,
      const base::FilePath& target_path,
      storage::AsyncFileUtil::StatusCallback callback) override;

  file_system_provider::AbortCallback Truncate(
      const base::FilePath& file_path,
      int64_t length,
      storage::AsyncFileUtil::StatusCallback callback) override;

  file_system_provider::AbortCallback WriteFile(
      int file_handle,
      net::IOBuffer* buffer,
      int64_t offset,
      int length,
      storage::AsyncFileUtil::StatusCallback callback) override;

  file_system_provider::AbortCallback FlushFile(
      int file_handle,
      storage::AsyncFileUtil::StatusCallback callback) override;

  file_system_provider::AbortCallback AddWatcher(
      const GURL& origin,
      const base::FilePath& entry_path,
      bool recursive,
      bool persistent,
      storage::AsyncFileUtil::StatusCallback callback,
      storage::WatcherManager::NotificationCallback notification_callback)
      override;

  void RemoveWatcher(const GURL& origin,
                     const base::FilePath& entry_path,
                     bool recursive,
                     storage::AsyncFileUtil::StatusCallback callback) override;

  const file_system_provider::ProvidedFileSystemInfo& GetFileSystemInfo()
      const override;

  file_system_provider::OperationRequestManager* GetRequestManager() override;

  file_system_provider::Watchers* GetWatchers() override;

  const file_system_provider::OpenedFiles& GetOpenedFiles() const override;

  void AddObserver(
      file_system_provider::ProvidedFileSystemObserver* observer) override;

  void RemoveObserver(
      file_system_provider::ProvidedFileSystemObserver* observer) override;

  void Notify(
      const base::FilePath& entry_path,
      bool recursive,
      storage::WatcherManager::ChangeType change_type,
      std::unique_ptr<file_system_provider::ProvidedFileSystemObserver::Changes>
          changes,
      const std::string& tag,
      storage::AsyncFileUtil::StatusCallback callback) override;

  void Configure(storage::AsyncFileUtil::StatusCallback callback) override;

  base::WeakPtr<ProvidedFileSystemInterface> GetWeakPtr() override;
  std::unique_ptr<file_system_provider::ScopedUserInteraction>
  StartUserInteraction() override;

 private:
  const file_system_provider::ProvidedFileSystemInfo file_system_info_;
  // opened_files_ is marked const since is currently unsupported.
  const file_system_provider::OpenedFiles opened_files_;
  base::WeakPtrFactory<SmbFileSystem> weak_ptr_factory_{this};
};

}  // namespace smb_client
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_SMB_CLIENT_SMB_FILE_SYSTEM_H_
