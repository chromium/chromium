// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/arc/fileapi/arc_file_system_bridge.h"

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/logging.h"
#include "base/memory/singleton.h"
#include "base/posix/eintr_wrapper.h"
#include "base/system/sys_info.h"
#include "base/task/post_task.h"
#include "base/threading/scoped_blocking_call.h"
#include "chrome/browser/chromeos/arc/fileapi/arc_select_files_handler.h"
#include "chrome/browser/chromeos/arc/fileapi/chrome_content_provider_url_util.h"
#include "chrome/browser/chromeos/arc/fileapi/file_stream_forwarder.h"
#include "chrome/browser/chromeos/drive/drive_integration_service.h"
#include "chrome/browser/chromeos/drive/file_system_util.h"
#include "chrome/browser/chromeos/file_manager/fileapi_util.h"
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
#include "storage/browser/file_system/file_system_context.h"
#include "url/gurl.h"

namespace {
constexpr char kChromeOSReleaseTrack[] = "CHROMEOS_RELEASE_TRACK";
constexpr char kTestImageRelease[] = "testimage-channel";
constexpr char kDriveFSPrefix[] = "drivefs-";
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
      content::BrowserContext::GetStoragePartitionForSite(context, url);
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
      base::Bind(
          [](ArcFileSystemBridge::GetFileSizeCallback callback,
             base::File::Error result, const base::File::Info& file_info) {
            int64_t size = -1;
            if (result == base::File::FILE_OK && !file_info.is_directory &&
                file_info.size >= 0) {
              size = file_info.size;
            }
            base::PostTask(FROM_HERE, {content::BrowserThread::UI},
                           base::BindOnce(std::move(callback), size));
          },
          base::Passed(&callback)));
}

// Decodes a percent-encoded URL to the drivefs path in the filesystem.
base::FilePath GetDriveFSPathFromURL(Profile* const profile, const GURL& url) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  // If DriveFS is not mounted, return an empty base::FilePath().
  if (!drive::util::GetIntegrationServiceByProfile(profile) ||
      !drive::util::GetIntegrationServiceByProfile(profile)->GetDriveFsHost()) {
    return base::FilePath();
  }
  base::FilePath virtual_path = chromeos::ExternalFileURLToVirtualPath(url);
  std::vector<base::FilePath::StringType> virtual_path_components;
  virtual_path.GetComponents(&virtual_path_components);

  // If the path is not DriveFS prefixed, then it might be FSP/MTP.
  if (virtual_path_components.empty() ||
      !base::StartsWith(virtual_path_components[0], kDriveFSPrefix,
                        base::CompareCase::SENSITIVE)) {
    return base::FilePath();
  }

  // Construct the path.
  base::FilePath drivefs_path =
      drive::util::GetIntegrationServiceByProfile(profile)->GetMountPointPath();
  DCHECK(!drivefs_path.empty());
  for (size_t i = 1; i < virtual_path_components.size(); i++) {
    drivefs_path = drivefs_path.Append(virtual_path_components[i]);
  }
  return drivefs_path;
}

// TODO(risan): Write test.
// Open DriveFS file from the fuse filesystem.
mojo::ScopedHandle OpenDriveFSFileToRead(const base::FilePath& fs_path) {
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
  scoped_refptr<storage::FileSystemContext> context =
      GetFileSystemContext(profile_, url_decoded);
  file_manager::util::FileSystemURLAndHandle file_system_url_and_handle =
      GetFileSystemURL(*context, url_decoded);
  base::PostTask(
      FROM_HERE, {content::BrowserThread::IO},
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
      base::Bind(
          [](GetFileTypeCallback callback, const std::string& mime_type) {
            std::move(callback).Run(mime_type.empty()
                                        ? base::nullopt
                                        : base::make_optional(mime_type));
          },
          base::Passed(&callback)));
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

void ArcFileSystemBridge::OpenFileToRead(const std::string& url,
                                         OpenFileToReadCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  GURL url_decoded = DecodeFromChromeContentProviderUrl(GURL(url));
  if (url_decoded.is_empty() || !IsUrlAllowed(url_decoded)) {
    LOG(ERROR) << "Invalid URL: " << url << " " << url_decoded;
    std::move(callback).Run(mojo::ScopedHandle());
    return;
  }

  // TODO(risan): Remove the fallback path in M75+ after DriveFS is always
  // enabled.
  base::FilePath fs_path = GetDriveFSPathFromURL(profile_, url_decoded);
  // If either DriveFS is not enabled/not mounted, or the URL doesn't represent
  // drivefs file (e.g., FSP, MTP), use VirtualFileProvider instead.
  if (fs_path.empty()) {
    GetFileSize(url, base::BindOnce(
                         &ArcFileSystemBridge::OpenFileToReadAfterGetFileSize,
                         weak_ptr_factory_.GetWeakPtr(), url_decoded,
                         std::move(callback)));
    return;
  }

  base::PostTaskAndReplyWithResult(
      FROM_HERE, {base::ThreadPool(), base::MayBlock()},
      base::BindOnce(&OpenDriveFSFileToRead, fs_path), std::move(callback));
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

void ArcFileSystemBridge::OpenFileToReadAfterGetFileSize(
    const GURL& url_decoded,
    OpenFileToReadCallback callback,
    int64_t size) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (size < 0) {
    LOG(ERROR) << "Failed to get file size " << url_decoded;
    std::move(callback).Run(mojo::ScopedHandle());
    return;
  }
  chromeos::DBusThreadManager::Get()->GetVirtualFileProviderClient()->OpenFile(
      size, base::BindOnce(&ArcFileSystemBridge::OnOpenFile,
                           weak_ptr_factory_.GetWeakPtr(), url_decoded,
                           std::move(callback)));
}

void ArcFileSystemBridge::OnOpenFile(const GURL& url_decoded,
                                     OpenFileToReadCallback callback,
                                     const std::string& id,
                                     base::ScopedFD fd) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!fd.is_valid()) {
    LOG(ERROR) << "Invalid FD";
    std::move(callback).Run(mojo::ScopedHandle());
    return;
  }
  DCHECK_EQ(id_to_url_.count(id), 0u);
  id_to_url_[id] = url_decoded;

  mojo::ScopedHandle wrapped_handle =
      mojo::WrapPlatformHandle(mojo::PlatformHandle(std::move(fd)));
  if (!wrapped_handle.is_valid()) {
    LOG(ERROR) << "Failed to wrap handle";
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

}  // namespace arc
