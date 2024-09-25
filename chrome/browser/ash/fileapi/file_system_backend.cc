// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/fileapi/file_system_backend.h"

#include <stddef.h>

#include <memory>
#include <utility>

#include "ash/webui/file_manager/url_constants.h"
#include "base/check_op.h"
#include "base/command_line.h"
#include "base/functional/callback_helpers.h"
#include "base/notreached.h"
#include "base/strings/escape.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/ash/arc/fileapi/arc_documents_provider_util.h"
#include "chrome/browser/ash/fileapi/file_access_permissions.h"
#include "chrome/browser/ash/fileapi/file_system_backend.h"
#include "chrome/browser/ash/fileapi/file_system_backend_delegate.h"
#include "chrome/browser/ash/fileapi/observable_file_system_operation_impl.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/media_galleries/fileapi/media_file_system_backend.h"
#include "chrome/common/url_constants.h"
#include "chromeos/ash/components/dbus/cros_disks/cros_disks_client.h"
#include "chromeos/ash/components/file_manager/app_id.h"
#include "components/file_access/scoped_file_access_delegate.h"
#include "components/user_manager/user.h"
#include "extensions/common/extension.h"
#include "storage/browser/file_system/async_file_util.h"
#include "storage/browser/file_system/external_mount_points.h"
#include "storage/browser/file_system/file_stream_reader.h"
#include "storage/browser/file_system/file_stream_writer.h"
#include "storage/browser/file_system/file_system_context.h"
#include "storage/browser/file_system/file_system_operation.h"
#include "storage/browser/file_system/file_system_operation_context.h"
#include "storage/browser/file_system/file_system_url.h"
#include "storage/common/file_system/file_system_mount_option.h"
#include "storage/common/file_system/file_system_types.h"
#include "storage/common/file_system/file_system_util.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace ash {
namespace {

// Returns the `AccountId` associated with the specified `profile`.
AccountId GetAccountId(Profile* profile) {
  user_manager::User* user =
      profile ? ProfileHelper::Get()->GetUserByProfile(profile) : nullptr;
  return user ? user->GetAccountId() : AccountId();
}

// Returns true if the BackendFunction and OperationType correspond to the
// GetMetadata() and CreateFileStreamReader() calls used when loading a
// filesystem: URL via FileSystemUrlLoaderFactory.
bool IsReadOperation(BackendFunction backend_function,
                     storage::OperationType operation_type) {
  if (backend_function == BackendFunction::kCreateFileSystemOperation &&
      operation_type == storage::OperationType::kGetMetadata) {
    return true;
  }
  if (backend_function == BackendFunction::kCreateFileStreamReader) {
    return true;
  }
  return false;
}

}  // namespace

// static
bool FileSystemBackend::CanHandleURL(const storage::FileSystemURL& url) {
  if (!url.is_valid())
    return false;
  return url.type() == storage::kFileSystemTypeLocal ||
         url.type() == storage::kFileSystemTypeProvided ||
         url.type() == storage::kFileSystemTypeDeviceMediaAsFileStorage ||
         url.type() == storage::kFileSystemTypeArcContent ||
         url.type() == storage::kFileSystemTypeArcDocumentsProvider ||
         url.type() == storage::kFileSystemTypeDriveFs ||
         url.type() == storage::kFileSystemTypeSmbFs ||
         url.type() == storage::kFileSystemTypeFuseBox;
}

FileSystemBackend::FileSystemBackend(
    Profile* profile,
    std::unique_ptr<FileSystemBackendDelegate> file_system_provider_delegate,
    std::unique_ptr<FileSystemBackendDelegate> mtp_delegate,
    std::unique_ptr<FileSystemBackendDelegate> arc_content_delegate,
    std::unique_ptr<FileSystemBackendDelegate> arc_documents_provider_delegate,
    std::unique_ptr<FileSystemBackendDelegate> drivefs_delegate,
    std::unique_ptr<FileSystemBackendDelegate> smbfs_delegate,
    scoped_refptr<storage::ExternalMountPoints> mount_points,
    storage::ExternalMountPoints* system_mount_points)
    : account_id_(GetAccountId(profile)),
      file_access_permissions_(new FileAccessPermissions()),
      local_file_util_(storage::AsyncFileUtil::CreateForLocalFileSystem()),
      file_system_provider_delegate_(std::move(file_system_provider_delegate)),
      mtp_delegate_(std::move(mtp_delegate)),
      arc_content_delegate_(std::move(arc_content_delegate)),
      arc_documents_provider_delegate_(
          std::move(arc_documents_provider_delegate)),
      drivefs_delegate_(std::move(drivefs_delegate)),
      smbfs_delegate_(std::move(smbfs_delegate)),
      mount_points_(mount_points),
      system_mount_points_(system_mount_points) {}

FileSystemBackend::~FileSystemBackend() {}

// static
FileSystemBackend* FileSystemBackend::Get(
    const storage::FileSystemContext& context) {
  return static_cast<FileSystemBackend*>(
      context.GetFileSystemBackend(storage::kFileSystemTypeExternal));
}

void FileSystemBackend::AddSystemMountPoints() {
  // RegisterFileSystem() is no-op if the mount point with the same name
  // already exists, hence it's safe to call without checking if a mount
  // point already exists or not.
  system_mount_points_->RegisterFileSystem(
      kSystemMountNameArchive, storage::kFileSystemTypeLocal,
      storage::FileSystemMountOption(),
      CrosDisksClient::GetArchiveMountPoint());
  system_mount_points_->RegisterFileSystem(
      kSystemMountNameRemovable, storage::kFileSystemTypeLocal,
      storage::FileSystemMountOption(storage::FlushPolicy::FLUSH_ON_COMPLETION),
      CrosDisksClient::GetRemovableDiskMountPoint());
}

bool FileSystemBackend::CanHandleType(storage::FileSystemType type) const {
  switch (type) {
    case storage::kFileSystemTypeExternal:
    case storage::kFileSystemTypeLocal:
    case storage::kFileSystemTypeLocalForPlatformApp:
    case storage::kFileSystemTypeDeviceMediaAsFileStorage:
    case storage::kFileSystemTypeProvided:
    case storage::kFileSystemTypeArcContent:
    case storage::kFileSystemTypeArcDocumentsProvider:
    case storage::kFileSystemTypeDriveFs:
    case storage::kFileSystemTypeSmbFs:
    case storage::kFileSystemTypeFuseBox:
      return true;
    default:
      return false;
  }
}

void FileSystemBackend::Initialize(storage::FileSystemContext* context) {}

void FileSystemBackend::ResolveURL(const storage::FileSystemURL& url,
                                   storage::OpenFileSystemMode mode,
                                   ResolveURLCallback callback) {
  std::string id;
  storage::FileSystemType type;
  std::string cracked_id;
  base::FilePath path;
  storage::FileSystemMountOption option;
  if (!mount_points_->CrackVirtualPath(url.virtual_path(), &id, &type,
                                       &cracked_id, &path, &option) &&
      !system_mount_points_->CrackVirtualPath(url.virtual_path(), &id, &type,
                                              &cracked_id, &path, &option)) {
    // Not under a mount point, so return an error, since the root is not
    // accessible.
    GURL root_url = GURL(storage::GetExternalFileSystemRootURIString(
        url.origin().GetURL(), std::string()));
    std::move(callback).Run(root_url, std::string(),
                            base::File::FILE_ERROR_SECURITY);
    return;
  }

  std::string name;
  // Construct a URL restricted to the found mount point.
  std::string root_url =
      storage::GetExternalFileSystemRootURIString(url.origin().GetURL(), id);

  // For removable and archives, the file system root is the external mount
  // point plus the inner mount point.
  if (id == "archive" || id == "removable") {
    std::vector<std::string> components = url.virtual_path().GetComponents();
    DCHECK_EQ(id, components.at(0));
    if (components.size() < 2) {
      // Unable to access /archive and /removable directories directly. The
      // inner mount name must be specified.
      std::move(callback).Run(GURL(root_url), std::string(),
                              base::File::FILE_ERROR_SECURITY);
      return;
    }
    std::string inner_mount_name = base::EscapePath(components[1]);
    root_url += inner_mount_name + "/";
    name = inner_mount_name;
  } else if (id == arc::kDocumentsProviderMountPointName) {
    // For ARC documents provider file system, volumes are mounted per document
    // provider root, so we need to fix up |root_url| to point to an individual
    // root.
    std::string authority;
    std::string root_id;
    base::FilePath unused_path;
    if (!arc::ParseDocumentsProviderUrl(url, &authority, &root_id,
                                        &unused_path)) {
      std::move(callback).Run(GURL(root_url), std::string(),
                              base::File::FILE_ERROR_SECURITY);
      return;
    }
    base::FilePath mount_path =
        arc::GetDocumentsProviderMountPath(authority, root_id);
    base::FilePath relative_mount_path;
    base::FilePath(arc::kDocumentsProviderMountPointPath)
        .AppendRelativePath(mount_path, &relative_mount_path);
    root_url +=
        base::EscapePath(storage::FilePathToString(relative_mount_path)) + "/";
    name = authority + ":" + root_id;
  } else {
    name = id;
  }

  std::move(callback).Run(GURL(root_url), name, base::File::FILE_OK);
}

storage::FileSystemQuotaUtil* FileSystemBackend::GetQuotaUtil() {
  // No quota support.
  return nullptr;
}

const storage::UpdateObserverList* FileSystemBackend::GetUpdateObservers(
    storage::FileSystemType type) const {
  return nullptr;
}

const storage::ChangeObserverList* FileSystemBackend::GetChangeObservers(
    storage::FileSystemType type) const {
  return nullptr;
}

const storage::AccessObserverList* FileSystemBackend::GetAccessObservers(
    storage::FileSystemType type) const {
  return nullptr;
}

bool FileSystemBackend::IsAccessAllowed(
    BackendFunction backend_function,
    storage::OperationType operation_type,
    const storage::FileSystemURL& url) const {
  if (!url.is_valid())
    return false;

  // No extra check is needed for isolated file systems.
  if (url.mount_type() == storage::kFileSystemTypeIsolated)
    return true;

  if (!CanHandleURL(url))
    return false;

  const url::Origin origin = url.origin();
  // If there is no origin set, then it's an internal access.
  if (origin.opaque())
    return true;

  // The chrome://file-manager can access its filesystem origin.
  if (origin.GetURL() == file_manager::kChromeUIFileManagerURL) {
    return true;
  }

  // ImageLoader extension has read-only access via FileSystemUrlLoaderFactory.
  if (origin.GetURL() == extensions::Extension::GetBaseURLFromExtensionId(
                             ::file_manager::kImageLoaderExtensionId) &&
      IsReadOperation(backend_function, operation_type)) {
    return true;
  }

  return file_access_permissions_->HasAccessPermission(origin,
                                                       url.virtual_path());
}

void FileSystemBackend::GrantFileAccessToOrigin(
    const url::Origin& origin,
    const base::FilePath& virtual_path) {
  std::string id;
  storage::FileSystemType type;
  std::string cracked_id;
  base::FilePath path;
  storage::FileSystemMountOption option;
  if (!mount_points_->CrackVirtualPath(virtual_path, &id, &type, &cracked_id,
                                       &path, &option) &&
      !system_mount_points_->CrackVirtualPath(virtual_path, &id, &type,
                                              &cracked_id, &path, &option)) {
    return;
  }

  file_access_permissions_->GrantAccessPermission(origin, virtual_path);
}

void FileSystemBackend::RevokeAccessForOrigin(const url::Origin& origin) {
  file_access_permissions_->RevokePermissions(origin);
}

std::vector<base::FilePath> FileSystemBackend::GetRootDirectories() const {
  std::vector<storage::MountPoints::MountPointInfo> mount_points;
  mount_points_->AddMountPointInfosTo(&mount_points);
  system_mount_points_->AddMountPointInfosTo(&mount_points);

  std::vector<base::FilePath> root_dirs;
  for (size_t i = 0; i < mount_points.size(); ++i)
    root_dirs.push_back(mount_points[i].path);
  return root_dirs;
}

storage::AsyncFileUtil* FileSystemBackend::GetAsyncFileUtil(
    storage::FileSystemType type) {
  switch (type) {
    case storage::kFileSystemTypeProvided:
      return file_system_provider_delegate_->GetAsyncFileUtil(type);
    case storage::kFileSystemTypeLocal:
    case storage::kFileSystemTypeFuseBox:
      return local_file_util_.get();
    case storage::kFileSystemTypeDeviceMediaAsFileStorage:
      return mtp_delegate_->GetAsyncFileUtil(type);
    case storage::kFileSystemTypeArcContent:
      return arc_content_delegate_->GetAsyncFileUtil(type);
    case storage::kFileSystemTypeArcDocumentsProvider:
      return arc_documents_provider_delegate_->GetAsyncFileUtil(type);
    case storage::kFileSystemTypeDriveFs:
      return drivefs_delegate_->GetAsyncFileUtil(type);
    case storage::kFileSystemTypeSmbFs:
      return smbfs_delegate_->GetAsyncFileUtil(type);
    default:
      NOTREACHED_IN_MIGRATION();
  }
  return nullptr;
}

storage::WatcherManager* FileSystemBackend::GetWatcherManager(
    storage::FileSystemType type) {
  if (type == storage::kFileSystemTypeProvided)
    return file_system_provider_delegate_->GetWatcherManager(type);

  if (type == storage::kFileSystemTypeDeviceMediaAsFileStorage) {
    return mtp_delegate_->GetWatcherManager(type);
  }

  if (type == storage::kFileSystemTypeArcDocumentsProvider)
    return arc_documents_provider_delegate_->GetWatcherManager(type);

  // TODO(mtomasz): Add support for other backends.
  return nullptr;
}

storage::CopyOrMoveFileValidatorFactory*
FileSystemBackend::GetCopyOrMoveFileValidatorFactory(
    storage::FileSystemType type,
    base::File::Error* error_code) {
  DCHECK(error_code);
  *error_code = base::File::FILE_OK;
  return nullptr;
}

std::unique_ptr<storage::FileSystemOperation>
FileSystemBackend::CreateFileSystemOperation(
    storage::OperationType type,
    const storage::FileSystemURL& url,
    storage::FileSystemContext* context,
    base::File::Error* error_code) const {
  DCHECK(url.is_valid());

  if (!IsAccessAllowed(BackendFunction::kCreateFileSystemOperation, type,
                       url)) {
    *error_code = base::File::FILE_ERROR_SECURITY;
    return nullptr;
  }

  if (url.type() == storage::kFileSystemTypeDeviceMediaAsFileStorage) {
    // MTP file operations run on MediaTaskRunner.
    return std::make_unique<ObservableFileSystemOperationImpl>(
        account_id_, type, url, context,
        std::make_unique<storage::FileSystemOperationContext>(
            context, MediaFileSystemBackend::MediaTaskRunner().get()));
  }
  if (url.type() == storage::kFileSystemTypeLocal ||
      url.type() == storage::kFileSystemTypeDriveFs ||
      url.type() == storage::kFileSystemTypeSmbFs ||
      url.type() == storage::kFileSystemTypeFuseBox) {
    return std::make_unique<ObservableFileSystemOperationImpl>(
        account_id_, type, url, context,
        std::make_unique<storage::FileSystemOperationContext>(
            context, base::ThreadPool::CreateSequencedTaskRunner(
                         {base::MayBlock(), base::TaskPriority::USER_VISIBLE})
                         .get()));
  }

  DCHECK(url.type() == storage::kFileSystemTypeProvided ||
         url.type() == storage::kFileSystemTypeArcContent ||
         url.type() == storage::kFileSystemTypeArcDocumentsProvider);
  return std::make_unique<ObservableFileSystemOperationImpl>(
      account_id_, type, url, context,
      std::make_unique<storage::FileSystemOperationContext>(context));
}

bool FileSystemBackend::SupportsStreaming(
    const storage::FileSystemURL& url) const {
  return url.type() == storage::kFileSystemTypeProvided ||
         url.type() == storage::kFileSystemTypeDeviceMediaAsFileStorage ||
         url.type() == storage::kFileSystemTypeArcContent ||
         url.type() == storage::kFileSystemTypeArcDocumentsProvider;
}

bool FileSystemBackend::HasInplaceCopyImplementation(
    storage::FileSystemType type) const {
  switch (type) {
    case storage::kFileSystemTypeProvided:
    case storage::kFileSystemTypeDeviceMediaAsFileStorage:
    case storage::kFileSystemTypeDriveFs:
      return true;
    // TODO(fukino): Support in-place copy for DocumentsProvider.
    // crbug.com/953603.
    case storage::kFileSystemTypeArcDocumentsProvider:
    case storage::kFileSystemTypeLocal:
    case storage::kFileSystemTypeArcContent:
    // TODO(crbug.com/41445433): Implement in-place copy in SmbFs.
    case storage::kFileSystemTypeSmbFs:
    case storage::kFileSystemTypeFuseBox:
      return false;
    default:
      NOTREACHED_IN_MIGRATION();
  }
  return true;
}

std::unique_ptr<storage::FileStreamReader>
FileSystemBackend::CreateFileStreamReader(
    const storage::FileSystemURL& url,
    int64_t offset,
    int64_t max_bytes_to_read,
    const base::Time& expected_modification_time,
    storage::FileSystemContext* context,
    file_access::ScopedFileAccessDelegate::RequestFilesAccessIOCallback
        file_access) const {
  DCHECK(url.is_valid());

  if (!IsAccessAllowed(BackendFunction::kCreateFileStreamReader,
                       storage::OperationType::kNone, url)) {
    return nullptr;
  }

  switch (url.type()) {
    case storage::kFileSystemTypeProvided:
      return file_system_provider_delegate_->CreateFileStreamReader(
          url, offset, max_bytes_to_read, expected_modification_time, context);
    // The dlp file_access callback is needed for the local filesystem only.
    case storage::kFileSystemTypeLocal:
      return storage::FileStreamReader::CreateForLocalFile(
          base::ThreadPool::CreateTaskRunner(
              {base::MayBlock(), base::TaskPriority::USER_VISIBLE})
              .get(),
          url.path(), offset, expected_modification_time,
          std::move(file_access));
    case storage::kFileSystemTypeDriveFs:
    case storage::kFileSystemTypeSmbFs:
    case storage::kFileSystemTypeFuseBox:
      return storage::FileStreamReader::CreateForLocalFile(
          base::ThreadPool::CreateTaskRunner(
              {base::MayBlock(), base::TaskPriority::USER_VISIBLE})
              .get(),
          url.path(), offset, expected_modification_time);
    case storage::kFileSystemTypeDeviceMediaAsFileStorage:
      return mtp_delegate_->CreateFileStreamReader(
          url, offset, max_bytes_to_read, expected_modification_time, context);
    case storage::kFileSystemTypeArcContent:
      return arc_content_delegate_->CreateFileStreamReader(
          url, offset, max_bytes_to_read, expected_modification_time, context);
    case storage::kFileSystemTypeArcDocumentsProvider:
      return arc_documents_provider_delegate_->CreateFileStreamReader(
          url, offset, max_bytes_to_read, expected_modification_time, context);
    default:
      NOTREACHED_IN_MIGRATION();
  }
  return nullptr;
}

std::unique_ptr<storage::FileStreamWriter>
FileSystemBackend::CreateFileStreamWriter(
    const storage::FileSystemURL& url,
    int64_t offset,
    storage::FileSystemContext* context) const {
  DCHECK(url.is_valid());

  if (!IsAccessAllowed(BackendFunction::kCreateFileStreamWriter,
                       storage::OperationType::kNone, url)) {
    return nullptr;
  }

  switch (url.type()) {
    case storage::kFileSystemTypeProvided:
      return file_system_provider_delegate_->CreateFileStreamWriter(url, offset,
                                                                    context);
    case storage::kFileSystemTypeLocal:
    case storage::kFileSystemTypeDriveFs:
    case storage::kFileSystemTypeSmbFs:
    case storage::kFileSystemTypeFuseBox:
      return storage::FileStreamWriter::CreateForLocalFile(
          base::ThreadPool::CreateTaskRunner(
              {base::MayBlock(), base::TaskPriority::USER_VISIBLE})
              .get(),
          url.path(), offset, storage::FileStreamWriter::OPEN_EXISTING_FILE);
    case storage::kFileSystemTypeDeviceMediaAsFileStorage:
      return mtp_delegate_->CreateFileStreamWriter(url, offset, context);
    case storage::kFileSystemTypeArcDocumentsProvider:
      return arc_documents_provider_delegate_->CreateFileStreamWriter(
          url, offset, context);
    // Read only file systems.
    case storage::kFileSystemTypeArcContent:
      return nullptr;
    default:
      NOTREACHED_IN_MIGRATION();
  }
  return nullptr;
}

bool FileSystemBackend::GetVirtualPath(const base::FilePath& filesystem_path,
                                       base::FilePath* virtual_path) const {
  return mount_points_->GetVirtualPath(filesystem_path, virtual_path) ||
         system_mount_points_->GetVirtualPath(filesystem_path, virtual_path);
}

storage::FileSystemURL FileSystemBackend::CreateInternalURL(
    storage::FileSystemContext* context,
    const base::FilePath& entry_path) const {
  base::FilePath virtual_path;
  if (!GetVirtualPath(entry_path, &virtual_path))
    return storage::FileSystemURL();

  return context->CreateCrackedFileSystemURL(
      blink::StorageKey(), storage::kFileSystemTypeExternal, virtual_path);
}

}  // namespace ash
