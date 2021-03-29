// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/fileapi/arc_file_system_bridge.h"

#include <fcntl.h>
#include <stdint.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/bind_post_task.h"
#include "base/logging.h"
#include "base/memory/singleton.h"
#include "base/posix/eintr_wrapper.h"
#include "base/system/sys_info.h"
#include "base/task/thread_pool.h"
#include "base/threading/scoped_blocking_call.h"
#include "chrome/browser/ash/arc/fileapi/arc_select_files_handler.h"
#include "chrome/browser/ash/arc/fileapi/chrome_content_provider_url_util.h"
#include "chrome/browser/ash/arc/fileapi/file_stream_forwarder.h"
#include "chrome/browser/ash/drive/drive_integration_service.h"
#include "chrome/browser/ash/drive/file_system_util.h"
#include "chrome/browser/chromeos/file_manager/fileapi_util.h"
#include "chrome/browser/chromeos/file_manager/path_util.h"
#include "chrome/browser/chromeos/fileapi/external_file_url_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/virtual_file_provider_client.h"
#include "components/arc/arc_browser_context_keyed_service_factory_base.h"
#include "components/arc/session/arc_bridge_service.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/common/url_constants.h"
#include "extensions/browser/api/file_handlers/mime_util.h"
#include "mojo/public/cpp/platform/platform_handle.h"
#include "mojo/public/cpp/system/platform_handle.h"
#include "net/base/escape.h"
#include "storage/browser/file_system/external_mount_points.h"
#include "storage/browser/file_system/file_system_context.h"
#include "url/gurl.h"

namespace {
constexpr char kChromeOSReleaseTrack[] = "CHROMEOS_RELEASE_TRACK";
constexpr char kTestImageRelease[] = "testimage-channel";
}  // namespace

namespace arc {

namespace {

// Returns true if it's OK to allow ARC apps to read the given URL.
bool IsUrlAllowed(const GURL& url) {
  // Currently, only externalfile URLs are allowed.
  return url.SchemeIs(content::kExternalFileScheme);
}

// Returns true if this is a testimage build.
bool IsTestImageBuild() {
  std::string track;
  return base::SysInfo::GetLsbReleaseValue(kChromeOSReleaseTrack, &track) &&
         track.find(kTestImageRelease) != std::string::npos;
}

// Returns FileSystemContext.
scoped_refptr<storage::FileSystemContext> GetFileSystemContext(
    content::BrowserContext* context,
    const GURL& url) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  content::StoragePartition* storage =
      content::BrowserContext::GetStoragePartitionForUrl(context, url);
  return storage->GetFileSystemContext();
}

// Converts the given URL to a FileSystemURL.
file_manager::util::FileSystemURLAndHandle GetFileSystemURL(
    const storage::FileSystemContext& context,
    const GURL& url) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  return file_manager::util::CreateIsolatedURLFromVirtualPath(
      context, /* empty origin */ GURL(),
      chromeos::ExternalFileURLToVirtualPath(url));
}

// Retrieves the file size on the IO thread, and runs the callback on the UI
// thread.
void GetFileSizeOnIOThread(scoped_refptr<storage::FileSystemContext> context,
                           const storage::FileSystemURL& url,
                           ArcFileSystemBridge::GetFileSizeCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  context->operation_runner()->GetMetadata(
      url,
      storage::FileSystemOperation::GET_METADATA_FIELD_IS_DIRECTORY |
          storage::FileSystemOperation::GET_METADATA_FIELD_SIZE,
      base::BindOnce([](base::File::Error result,
                        const base::File::Info& file_info) -> int64_t {
        if (result == base::File::FILE_OK && !file_info.is_directory &&
            file_info.size >= 0) {
          return file_info.size;
        }
        return -1;
      })
          .Then(base::BindPostTask(content::GetUIThreadTaskRunner({}),
                                   std::move(callback))));
}

// TODO(risan): Write test.
// Open a file from a VFS (vs Chrome-only) filesystem.
mojo::ScopedHandle OpenVFSFileToRead(const base::FilePath& fs_path) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::WILL_BLOCK);
  // Open the file and wrap the fd to be returned through mojo.
  base::ScopedFD fd(HANDLE_EINTR(
      open(fs_path.value().c_str(), O_RDONLY | O_CLOEXEC | O_NOFOLLOW)));
  if (!fd.is_valid()) {
    PLOG(WARNING) << "Invalid FD for fs_path: " << fs_path;
    return mojo::ScopedHandle();
  }
  return mojo::WrapPlatformHandle(mojo::PlatformHandle(std::move(fd)));
}

// Factory of ArcFileSystemBridge.
class ArcFileSystemBridgeFactory
    : public internal::ArcBrowserContextKeyedServiceFactoryBase<
          ArcFileSystemBridge,
          ArcFileSystemBridgeFactory> {
 public:
  static constexpr const char* kName = "ArcFileSystemBridgeFactory";

  static ArcFileSystemBridgeFactory* GetInstance() {
    return base::Singleton<ArcFileSystemBridgeFactory>::get();
  }

 private:
  friend base::DefaultSingletonTraits<ArcFileSystemBridgeFactory>;
  ArcFileSystemBridgeFactory() = default;
  ~ArcFileSystemBridgeFactory() override = default;
};

}  // namespace

ArcFileSystemBridge::ArcFileSystemBridge(content::BrowserContext* context,
                                         ArcBridgeService* bridge_service)
    : profile_(Profile::FromBrowserContext(context)),
      bridge_service_(bridge_service),
      select_files_handlers_manager_(
          std::make_unique<ArcSelectFilesHandlersManager>(context)) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  bridge_service_->file_system()->SetHost(this);
  bridge_service_->file_system()->AddObserver(this);
}

ArcFileSystemBridge::~ArcFileSystemBridge() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  bridge_service_->file_system()->RemoveObserver(this);
  bridge_service_->file_system()->SetHost(nullptr);
}

// static
BrowserContextKeyedServiceFactory* ArcFileSystemBridge::GetFactory() {
  return ArcFileSystemBridgeFactory::GetInstance();
}

// static
ArcFileSystemBridge* ArcFileSystemBridge::GetForBrowserContext(
    content::BrowserContext* context) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  return ArcFileSystemBridgeFactory::GetForBrowserContext(context);
}

// static
ArcFileSystemBridge* ArcFileSystemBridge::GetForBrowserContextForTesting(
    content::BrowserContext* context) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  return ArcFileSystemBridgeFactory::GetForBrowserContextForTesting(context);
}

void ArcFileSystemBridge::AddObserver(Observer* observer) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  observer_list_.AddObserver(observer);
}

void ArcFileSystemBridge::RemoveObserver(Observer* observer) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  observer_list_.RemoveObserver(observer);
}

void ArcFileSystemBridge::GetFileName(const std::string& url,
                                      GetFileNameCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  GURL url_decoded = DecodeFromChromeContentProviderUrl(GURL(url));
  std::string unescaped_file_name;
  // It's generally not safe to unescape path separators in strings to be used
  // in file paths.
  if (url_decoded.is_empty() || !IsUrlAllowed(url_decoded) ||
      !net::UnescapeBinaryURLComponentSafe(url_decoded.ExtractFileName(),
                                           true /* fail_on_path_separators */,
                                           &unescaped_file_name)) {
    LOG(ERROR) << "Invalid URL: " << url << " " << url_decoded;
    std::move(callback).Run(base::nullopt);
    return;
  }
  std::move(callback).Run(unescaped_file_name);
}

void ArcFileSystemBridge::GetFileSize(const std::string& url,
                                      GetFileSizeCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  GURL url_decoded = DecodeFromChromeContentProviderUrl(GURL(url));
  if (url_decoded.is_empty() || !IsUrlAllowed(url_decoded)) {
    LOG(ERROR) << "Invalid URL: " << url << " " << url_decoded;
    std::move(callback).Run(-1);
    return;
  }

  GetFileSizeInternal(url_decoded, std::move(callback));
}

void ArcFileSystemBridge::GetFileSizeInternal(const GURL& url_decoded,
                                              GetFileSizeCallback callback) {
  scoped_refptr<storage::FileSystemContext> context =
      GetFileSystemContext(profile_, url_decoded);
  file_manager::util::FileSystemURLAndHandle file_system_url_and_handle =
      GetFileSystemURL(*context, url_decoded);
  content::GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&GetFileSizeOnIOThread, std::move(context),
                     file_system_url_and_handle.url, std::move(callback)));
  // TODO(https://crbug.com/963027): This is currently leaking the isolated
  // file system, the file system should somehow be revoked when the url
  // returned by GetFileSystemURL is no longer needed.
  storage::IsolatedContext::GetInstance()->AddReference(
      file_system_url_and_handle.handle.id());
}

void ArcFileSystemBridge::GetFileType(const std::string& url,
                                      GetFileTypeCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  GURL url_decoded = DecodeFromChromeContentProviderUrl(GURL(url));
  if (url_decoded.is_empty() || !IsUrlAllowed(url_decoded)) {
    LOG(ERROR) << "Invalid URL: " << url << " " << url_decoded;
    std::move(callback).Run(base::nullopt);
    return;
  }
  scoped_refptr<storage::FileSystemContext> context =
      GetFileSystemContext(profile_, url_decoded);
  file_manager::util::FileSystemURLAndHandle file_system_url_and_handle =
      GetFileSystemURL(*context, url_decoded);
  extensions::app_file_handler_util::GetMimeTypeForLocalPath(
      profile_, file_system_url_and_handle.url.path(),
      base::BindOnce([](const std::string& mime_type) {
        return mime_type.empty() ? base::nullopt
                                 : base::make_optional(mime_type);
      }).Then(std::move(callback)));
}

void ArcFileSystemBridge::OnDocumentChanged(
    int64_t watcher_id,
    storage::WatcherManager::ChangeType type) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  for (auto& observer : observer_list_)
    observer.OnDocumentChanged(watcher_id, type);
}

void ArcFileSystemBridge::OnRootsChanged() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  for (auto& observer : observer_list_)
    observer.OnRootsChanged();
}

void ArcFileSystemBridge::GetVirtualFileId(const std::string& url,
                                           GetVirtualFileIdCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  GURL url_decoded = DecodeFromChromeContentProviderUrl(GURL(url));
  if (url_decoded.is_empty() || !IsUrlAllowed(url_decoded)) {
    LOG(ERROR) << "Invalid URL: " << url << " " << url_decoded;
    std::move(callback).Run(base::nullopt);
    return;
  }

  GetVirtualFileIdInternal(url_decoded, std::move(callback));
}

void ArcFileSystemBridge::HandleIdReleased(const std::string& id,
                                           HandleIdReleasedCallback callback) {
  std::move(callback).Run(HandleIdReleased(id));
}

void ArcFileSystemBridge::OpenFileToRead(const std::string& url,
                                         OpenFileToReadCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  GURL url_decoded = DecodeFromChromeContentProviderUrl(GURL(url));
  if (url_decoded.is_empty() || !IsUrlAllowed(url_decoded)) {
    LOG(ERROR) << "Invalid URL: " << url << " " << url_decoded;
    std::move(callback).Run(mojo::ScopedHandle());
    return;
  }

  base::FilePath fs_path =
      GetLinuxVFSPathFromExternalFileURL(profile_, url_decoded);
  // If the URL represents a file on a virtual (Chrome-only, e.g., FSP,
  // MTP) filesystem, use VirtualFileProvider instead.
  if (fs_path.empty()) {
    GetVirtualFileIdInternal(
        url_decoded, base::BindOnce(&ArcFileSystemBridge::OpenFileById,
                                    weak_ptr_factory_.GetWeakPtr(), url_decoded,
                                    std::move(callback)));
    return;
  }

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock()},
      base::BindOnce(&OpenVFSFileToRead, fs_path), std::move(callback));
}

void ArcFileSystemBridge::GetVirtualFileIdInternal(
    const GURL& url_decoded,
    GetVirtualFileIdCallback callback) {
  GetFileSizeInternal(
      url_decoded, base::BindOnce(&ArcFileSystemBridge::GenerateVirtualFileId,
                                  weak_ptr_factory_.GetWeakPtr(), url_decoded,
                                  std::move(callback)));
}

void ArcFileSystemBridge::SelectFiles(mojom::SelectFilesRequestPtr request,
                                      SelectFilesCallback callback) {
  select_files_handlers_manager_->SelectFiles(std::move(request),
                                              std::move(callback));
}

void ArcFileSystemBridge::OnFileSelectorEvent(
    mojom::FileSelectorEventPtr event,
    ArcFileSystemBridge::OnFileSelectorEventCallback callback) {
  std::string track;
  select_files_handlers_manager_->OnFileSelectorEvent(std::move(event),
                                                      std::move(callback));
}

void ArcFileSystemBridge::GetFileSelectorElements(
    mojom::GetFileSelectorElementsRequestPtr request,
    GetFileSelectorElementsCallback callback) {
  if (!IsTestImageBuild()) {
    LOG(ERROR)
        << "GetFileSelectorElements is only allowed under test conditions";
    std::move(callback).Run(mojom::FileSelectorElements::New());
    return;
  }
  select_files_handlers_manager_->GetFileSelectorElements(std::move(request),
                                                          std::move(callback));
}

void ArcFileSystemBridge::GenerateVirtualFileId(
    const GURL& url_decoded,
    GenerateVirtualFileIdCallback callback,
    int64_t size) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (size < 0) {
    LOG(ERROR) << "Failed to get file size " << url_decoded;
    std::move(callback).Run(base::nullopt);
    return;
  }
  chromeos::DBusThreadManager::Get()
      ->GetVirtualFileProviderClient()
      ->GenerateVirtualFileId(
          size, base::BindOnce(&ArcFileSystemBridge::OnGenerateVirtualFileId,
                               weak_ptr_factory_.GetWeakPtr(), url_decoded,
                               std::move(callback)));
}

void ArcFileSystemBridge::OnGenerateVirtualFileId(
    const GURL& url_decoded,
    GenerateVirtualFileIdCallback callback,
    const base::Optional<std::string>& id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(id.has_value());
  DCHECK_EQ(id_to_url_.count(id.value()), 0u);
  id_to_url_[id.value()] = url_decoded;

  std::move(callback).Run(std::move(id));
}

void ArcFileSystemBridge::OpenFileById(const GURL& url_decoded,
                                       OpenFileToReadCallback callback,
                                       const base::Optional<std::string>& id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!id.has_value()) {
    LOG(ERROR) << "Missing ID";
    std::move(callback).Run(mojo::ScopedHandle());
    return;
  }

  chromeos::DBusThreadManager::Get()
      ->GetVirtualFileProviderClient()
      ->OpenFileById(id.value(),
                     base::BindOnce(&ArcFileSystemBridge::OnOpenFileById,
                                    weak_ptr_factory_.GetWeakPtr(), url_decoded,
                                    std::move(callback), id.value()));
}

void ArcFileSystemBridge::OnOpenFileById(const GURL& url_decoded,
                                         OpenFileToReadCallback callback,
                                         const std::string& id,
                                         base::ScopedFD fd) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!fd.is_valid()) {
    LOG(ERROR) << "Invalid FD";
    if (!HandleIdReleased(id))
      LOG(ERROR) << "Cannot release ID: " << id;
    std::move(callback).Run(mojo::ScopedHandle());
    return;
  }

  mojo::ScopedHandle wrapped_handle =
      mojo::WrapPlatformHandle(mojo::PlatformHandle(std::move(fd)));
  if (!wrapped_handle.is_valid()) {
    LOG(ERROR) << "Failed to wrap handle";
    if (!HandleIdReleased(id))
      LOG(ERROR) << "Cannot release ID: " << id;
    std::move(callback).Run(mojo::ScopedHandle());
    return;
  }
  std::move(callback).Run(std::move(wrapped_handle));
}

bool ArcFileSystemBridge::HandleReadRequest(const std::string& id,
                                            int64_t offset,
                                            int64_t size,
                                            base::ScopedFD pipe_write_end) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  auto it_url = id_to_url_.find(id);
  if (it_url == id_to_url_.end()) {
    LOG(ERROR) << "Invalid ID: " << id;
    return false;
  }

  // Add a new element to the list, and get an iterator.
  // NOTE: std::list iterators never get invalidated as long as the pointed
  // element is alive.
  file_stream_forwarders_.emplace_front();
  auto it_forwarder = file_stream_forwarders_.begin();

  const GURL& url = it_url->second;
  scoped_refptr<storage::FileSystemContext> context =
      GetFileSystemContext(profile_, url);
  file_manager::util::FileSystemURLAndHandle file_system_url_and_handle =
      GetFileSystemURL(*context, url);
  *it_forwarder = FileStreamForwarderPtr(new FileStreamForwarder(
      std::move(context), file_system_url_and_handle.url, offset, size,
      std::move(pipe_write_end),
      base::BindOnce(&ArcFileSystemBridge::OnReadRequestCompleted,
                     weak_ptr_factory_.GetWeakPtr(), id, it_forwarder)));
  // TODO(https://crbug.com/963027): This is currently leaking the isolated
  // file system, the file system should somehow be revoked when the url
  // returned by GetFileSystemURL is no longer needed.
  storage::IsolatedContext::GetInstance()->AddReference(
      file_system_url_and_handle.handle.id());
  return true;
}

bool ArcFileSystemBridge::HandleIdReleased(const std::string& id) {
  return id_to_url_.erase(id) != 0;
}

void ArcFileSystemBridge::OnReadRequestCompleted(
    const std::string& id,
    std::list<FileStreamForwarderPtr>::iterator it,
    bool result) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  LOG_IF(ERROR, !result) << "Failed to read " << id;
  file_stream_forwarders_.erase(it);
}

void ArcFileSystemBridge::OnConnectionClosed() {
  LOG(WARNING) << "FileSystem connection has been closed. "
               << "Closing SelectFileDialogs owned by ARC apps, if any.";
  select_files_handlers_manager_->DeleteAllHandlers();
}

base::FilePath ArcFileSystemBridge::GetLinuxVFSPathFromExternalFileURL(
    Profile* const profile,
    const GURL& url) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  base::FilePath virtual_path = chromeos::ExternalFileURLToVirtualPath(url);

  std::string mount_name, cracked_id;
  storage::FileSystemType file_system_type;
  base::FilePath absolute_path;
  storage::FileSystemMountOption mount_option;

  if (!storage::ExternalMountPoints::GetSystemInstance()->CrackVirtualPath(
          virtual_path, &mount_name, &file_system_type, &cracked_id,
          &absolute_path, &mount_option)) {
    LOG(WARNING) << "Couldn't find mount point for: " << url;
    return base::FilePath();
  }

  return GetLinuxVFSPathForPathOnFileSystemType(profile, absolute_path,
                                                file_system_type);
}

base::FilePath ArcFileSystemBridge::GetLinuxVFSPathForPathOnFileSystemType(
    Profile* const profile,
    const base::FilePath& path,
    storage::FileSystemType file_system_type) {
  switch (file_system_type) {
    case storage::FileSystemType::kFileSystemTypeDriveFs:
    case storage::FileSystemType::kFileSystemTypeSmbFs:
      return path;
    case storage::FileSystemType::kFileSystemTypeLocal: {
      base::FilePath crostini_mount_path =
          file_manager::util::GetCrostiniMountDirectory(profile);
      if (crostini_mount_path == path || crostini_mount_path.IsParent(path))
        return path;

      // fuse-zip, rar2fs.
      base::FilePath archive_mount_path =
          base::FilePath(file_manager::util::kArchiveMountPath);
      if (archive_mount_path.IsParent(path))
        return path;

      break;
    }
    default:
      break;
  }

  // The path is not representable on the Linux VFS.
  return base::FilePath();
}

}  // namespace arc
