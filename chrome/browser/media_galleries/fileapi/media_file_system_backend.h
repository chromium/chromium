// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_GALLERIES_FILEAPI_MEDIA_FILE_SYSTEM_BACKEND_H_
#define CHROME_BROWSER_MEDIA_GALLERIES_FILEAPI_MEDIA_FILE_SYSTEM_BACKEND_H_

#include <stdint.h>

#include <memory>
#include <string>

#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/memory/ref_counted.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/media_galleries/media_galleries_preferences.h"
#include "components/download/public/common/quarantine_connection.h"
#include "components/file_access/scoped_file_access_delegate.h"
#include "storage/browser/file_system/file_system_backend.h"
#include "storage/browser/file_system/file_system_request_info.h"
#include "storage/browser/file_system/task_runner_bound_observer_list.h"

namespace base {
class SequencedTaskRunner;
}  // namespace base

namespace storage {
class FileSystemURL;
}  // namespace storage

class DeviceMediaAsyncFileUtil;
class MediaPathFilter;

class MediaFileSystemBackend : public storage::FileSystemBackend {
 public:
  MediaFileSystemBackend(
      const base::FilePath& profile_path,
      download::QuarantineConnectionCallback quarantine_connection_callback);

  MediaFileSystemBackend(const MediaFileSystemBackend&) = delete;
  MediaFileSystemBackend& operator=(const MediaFileSystemBackend&) = delete;

  ~MediaFileSystemBackend() override;

  // Asserts that the current task is sequenced with any other task that calls
  // this.
  static void AssertCurrentlyOnMediaSequence();

  static scoped_refptr<base::SequencedTaskRunner> MediaTaskRunner();

  // Construct the mount point for the gallery specified by `pref_id` in
  // the profile located in `profile_path`.
  static std::string ConstructMountName(const base::FilePath& profile_path,
                                        const std::string& extension_id,
                                        MediaGalleryPrefId pref_id);

  static bool AttemptAutoMountForURLRequest(
      const storage::FileSystemRequestInfo& request_info,
      const storage::FileSystemURL& filesystem_url,
      base::OnceCallback<void(base::File::Error result)> callback);

  // FileSystemBackend implementation.
  bool CanHandleType(storage::FileSystemType type) const override;
  void Initialize(storage::FileSystemContext* context) override;
  void ResolveURL(const storage::FileSystemURL& url,
                  storage::OpenFileSystemMode mode,
                  ResolveURLCallback callback) override;
  storage::AsyncFileUtil* GetAsyncFileUtil(
      storage::FileSystemType type) override;
  storage::WatcherManager* GetWatcherManager(
      storage::FileSystemType type) override;
  storage::CopyOrMoveFileValidatorFactory* GetCopyOrMoveFileValidatorFactory(
      storage::FileSystemType type,
      base::File::Error* error_code) override;
  std::unique_ptr<storage::FileSystemOperation> CreateFileSystemOperation(
      storage::OperationType type,
      const storage::FileSystemURL& url,
      storage::FileSystemContext* context,
      base::File::Error* error_code) const override;
  bool SupportsStreaming(const storage::FileSystemURL& url) const override;
  bool HasInplaceCopyImplementation(
      storage::FileSystemType type) const override;
  std::unique_ptr<storage::FileStreamReader> CreateFileStreamReader(
      const storage::FileSystemURL& url,
      int64_t offset,
      int64_t max_bytes_to_read,
      const base::Time& expected_modification_time,
      storage::FileSystemContext* context,
      file_access::ScopedFileAccessDelegate::RequestFilesAccessIOCallback
          file_access) const override;
  std::unique_ptr<storage::FileStreamWriter> CreateFileStreamWriter(
      const storage::FileSystemURL& url,
      int64_t offset,
      storage::FileSystemContext* context) const override;
  storage::FileSystemQuotaUtil* GetQuotaUtil() override;
  const storage::UpdateObserverList* GetUpdateObservers(
      storage::FileSystemType type) const override;
  const storage::ChangeObserverList* GetChangeObservers(
      storage::FileSystemType type) const override;
  const storage::AccessObserverList* GetAccessObservers(
      storage::FileSystemType type) const override;

 private:
  // Store the profile path. We need this to create temporary snapshot files.
  const base::FilePath profile_path_;

  std::unique_ptr<MediaPathFilter> media_path_filter_;
  std::unique_ptr<storage::CopyOrMoveFileValidatorFactory>
      media_copy_or_move_file_validator_factory_;

  std::unique_ptr<storage::AsyncFileUtil> native_media_file_util_;

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_CHROMEOS_ASH)
  std::unique_ptr<DeviceMediaAsyncFileUtil> device_media_async_file_util_;
#endif
};

#endif  // CHROME_BROWSER_MEDIA_GALLERIES_FILEAPI_MEDIA_FILE_SYSTEM_BACKEND_H_
