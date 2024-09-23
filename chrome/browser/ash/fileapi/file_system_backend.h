// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_FILEAPI_FILE_SYSTEM_BACKEND_H_
#define CHROME_BROWSER_ASH_FILEAPI_FILE_SYSTEM_BACKEND_H_

#include <stdint.h>

#include <memory>
#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "components/account_id/account_id.h"
#include "components/file_access/scoped_file_access_delegate.h"
#include "storage/browser/file_system/file_system_backend.h"
#include "storage/browser/file_system/task_runner_bound_observer_list.h"
#include "storage/common/file_system/file_system_types.h"
#include "url/origin.h"

class Profile;

namespace storage {
class CopyOrMoveFileValidatorFactory;
class ExternalMountPoints;
class FileSystemURL;
class WatcherManager;
}  // namespace storage

namespace ash {

class FileSystemBackendDelegate;
class FileAccessPermissions;

constexpr char kSystemMountNameArchive[] = "archive";
constexpr char kSystemMountNameRemovable[] = "removable";

// Backend Function called.  Used to control access.
enum class BackendFunction {
  kCreateFileSystemOperation,
  kCreateFileStreamReader,
  kCreateFileStreamWriter,
};

// FileSystemBackend is a Chrome OS specific implementation of
// ExternalFileSystemBackend. This class is responsible for a
// number of things, including:
//
// - Add system mount points
// - Grant/revoke/check file access permissions
// - Create FileSystemOperation per file system type
// - Create FileStreamReader/Writer per file system type
//
// Chrome OS specific static mount points:
//
// "archive" is a mount point for an archive file, such as a zip file. This
// mount point exposes contents of an archive file via cros_disks and AVFS
// <http://avf.sourceforge.net/>.
//
// "removable" is a mount point for removable media such as an SD card.
// Insertion and removal of removable media are handled by cros_disks.
//
// These mount points are placed under the "external" namespace, and file
// system URLs for these mount points look like:
//
//   filesystem:<origin>/external/<mount_name>/...
//
// Other mounts are also registered by VolumeManager for MyFiles, Drive, VMs
// (crostini, arc, etc), Android Document Providers, fileSystemProviders, etc.
class FileSystemBackend : public storage::FileSystemBackend {
 public:
  using storage::FileSystemBackend::ResolveURLCallback;

  // |system_mount_points| should outlive FileSystemBackend instance.
  FileSystemBackend(
      Profile* profile,
      std::unique_ptr<FileSystemBackendDelegate> file_system_provider_delegate,
      std::unique_ptr<FileSystemBackendDelegate> mtp_delegate,
      std::unique_ptr<FileSystemBackendDelegate> arc_content_delegate,
      std::unique_ptr<FileSystemBackendDelegate>
          arc_documents_provider_delegate,
      std::unique_ptr<FileSystemBackendDelegate> drivefs_delegate,
      std::unique_ptr<FileSystemBackendDelegate> smbfs_delegate,
      scoped_refptr<storage::ExternalMountPoints> mount_points,
      storage::ExternalMountPoints* system_mount_points);

  FileSystemBackend(const FileSystemBackend&) = delete;
  FileSystemBackend& operator=(const FileSystemBackend&) = delete;

  ~FileSystemBackend() override;

  // Gets the ChromeOS FileSystemBackend.
  static FileSystemBackend* Get(const storage::FileSystemContext& context);

  // Adds system mount points, such as "archive", and "removable". This
  // function is no-op if these mount points are already present.
  void AddSystemMountPoints();

  // Returns true if CrosMountpointProvider can handle |url|, i.e. its
  // file system type matches with what this provider supports.
  // This could be called on any threads.
  static bool CanHandleURL(const storage::FileSystemURL& url);

  // Returns true if |url| is allowed to be accessed.
  // This is supposed to perform ExternalFileSystem-specific security
  // checks.
  bool IsAccessAllowed(BackendFunction backend_function,
                       storage::OperationType operation_type,
                       const storage::FileSystemURL& url) const;

  // Returns the list of top level directories that are exposed by this
  // provider. This list is used to set appropriate child process file access
  // permissions.
  std::vector<base::FilePath> GetRootDirectories() const;

  // Grants access to |virtual_path| from |origin| URL.
  void GrantFileAccessToOrigin(const url::Origin& origin,
                               const base::FilePath& virtual_path);

  // Revokes file access from origin identified with |origin|.
  void RevokeAccessForOrigin(const url::Origin& origin);

  // Gets virtual path by known filesystem path. Returns false when filesystem
  // path is not exposed by this provider.
  bool GetVirtualPath(const base::FilePath& file_system_path,
                      base::FilePath* virtual_path) const;

  // Creates an internal File System URL for performing internal operations such
  // as confirming if a file or a directory exist before granting the final
  // permission to the entry. The path must be an absolute path.
  storage::FileSystemURL CreateInternalURL(
      storage::FileSystemContext* context,
      const base::FilePath& entry_path) const;

  // storage::FileSystemBackend overrides.
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
      const storage::FileSystemURL& path,
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
  const AccountId account_id_;

  std::unique_ptr<FileAccessPermissions> file_access_permissions_;
  std::unique_ptr<storage::AsyncFileUtil> local_file_util_;

  // The delegate instance for the provided file system related operations.
  std::unique_ptr<FileSystemBackendDelegate> file_system_provider_delegate_;

  // The delegate instance for the MTP file system related operations.
  std::unique_ptr<FileSystemBackendDelegate> mtp_delegate_;

  // The delegate instance for the ARC content file system related operations.
  std::unique_ptr<FileSystemBackendDelegate> arc_content_delegate_;

  // The delegate instance for the ARC documents provider related operations.
  std::unique_ptr<FileSystemBackendDelegate> arc_documents_provider_delegate_;

  // The delegate instance for the DriveFS file system related operations.
  std::unique_ptr<FileSystemBackendDelegate> drivefs_delegate_;

  // The delegate instance for the SmbFs file system related operations.
  std::unique_ptr<FileSystemBackendDelegate> smbfs_delegate_;

  // Mount points specific to the owning context (i.e. per-profile mount
  // points).
  //
  // It is legal to have mount points with the same name as in
  // system_mount_points_. Also, mount point paths may overlap with mount point
  // paths in system_mount_points_. In both cases mount points in
  // |mount_points_| will have a priority.
  // E.g. if |mount_points_| map 'foo1' to '/foo/foo1' and
  // |file_system_mount_points_| map 'xxx' to '/foo/foo1/xxx', |GetVirtualPaths|
  // will resolve '/foo/foo1/xxx/yyy' as 'foo1/xxx/yyy' (i.e. the mapping from
  // |mount_points_| will be used).
  scoped_refptr<storage::ExternalMountPoints> mount_points_;

  // Globally visible mount points. System MountPonts instance should outlive
  // all FileSystemBackend instances, so raw pointer is safe.
  raw_ptr<storage::ExternalMountPoints> system_mount_points_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_FILEAPI_FILE_SYSTEM_BACKEND_H_
