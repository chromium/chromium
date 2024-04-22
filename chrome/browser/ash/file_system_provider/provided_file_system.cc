// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/file_system_provider/provided_file_system.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/files/file.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/ptr_util.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "chrome/browser/ash/file_system_provider/notification_manager.h"
#include "chrome/browser/ash/file_system_provider/odfs_metrics.h"
#include "chrome/browser/ash/file_system_provider/operation_request_manager.h"
#include "chrome/browser/ash/file_system_provider/operations/abort.h"
#include "chrome/browser/ash/file_system_provider/operations/add_watcher.h"
#include "chrome/browser/ash/file_system_provider/operations/close_file.h"
#include "chrome/browser/ash/file_system_provider/operations/configure.h"
#include "chrome/browser/ash/file_system_provider/operations/copy_entry.h"
#include "chrome/browser/ash/file_system_provider/operations/create_directory.h"
#include "chrome/browser/ash/file_system_provider/operations/create_file.h"
#include "chrome/browser/ash/file_system_provider/operations/delete_entry.h"
#include "chrome/browser/ash/file_system_provider/operations/execute_action.h"
#include "chrome/browser/ash/file_system_provider/operations/get_actions.h"
#include "chrome/browser/ash/file_system_provider/operations/get_metadata.h"
#include "chrome/browser/ash/file_system_provider/operations/move_entry.h"
#include "chrome/browser/ash/file_system_provider/operations/open_file.h"
#include "chrome/browser/ash/file_system_provider/operations/read_directory.h"
#include "chrome/browser/ash/file_system_provider/operations/read_file.h"
#include "chrome/browser/ash/file_system_provider/operations/remove_watcher.h"
#include "chrome/browser/ash/file_system_provider/operations/truncate.h"
#include "chrome/browser/ash/file_system_provider/operations/unmount.h"
#include "chrome/browser/ash/file_system_provider/operations/write_file.h"
#include "chrome/browser/ash/file_system_provider/provided_file_system_info.h"
#include "chrome/browser/ash/file_system_provider/request_dispatcher_impl.h"
#include "chrome/browser/ash/file_system_provider/request_manager.h"
#include "chrome/browser/chromeos/extensions/file_system_provider/service_worker_lifetime_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/api/file_system_provider.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chromeos/constants/chromeos_features.h"
#include "extensions/browser/event_router.h"

namespace net {
class IOBuffer;
}  // namespace net

namespace ash::file_system_provider {

namespace {

constexpr char kODFSFlushAction[] = "HIDDEN_ONEDRIVE_FLUSH_FILE";

// Timeout in seconds, before a file system operation request is considered as
// stale and hence aborted.
constexpr base::TimeDelta kDefaultOperationTimeout = base::Seconds(10);

// Operation timeout for the ODFS extension.
constexpr base::TimeDelta kODFSOperationTimeout = base::Seconds(30);

extensions::file_system_provider::ServiceWorkerLifetimeManager*
GetServiceWorkerLifetimeManager(Profile* profile) {
  if (!chromeos::features::IsUploadOfficeToCloudEnabled()) {
    return nullptr;
  }
  return extensions::file_system_provider::ServiceWorkerLifetimeManager::Get(
      profile);
}

class ScopedUserInteractionImpl : public ScopedUserInteraction {
 public:
  explicit ScopedUserInteractionImpl(ProvidedFileSystem* file_system)
      : file_system_(file_system->GetWeakPtr()) {
    file_system_->GetRequestManager()->StartUserInteraction();
  }

  ~ScopedUserInteractionImpl() override {
    if (file_system_) {
      file_system_->GetRequestManager()->EndUserInteraction();
    }
  }

 private:
  base::WeakPtr<ProvidedFileSystemInterface> file_system_;
};

}  // namespace

AutoUpdater::AutoUpdater(base::OnceClosure update_callback)
    : update_callback_(std::move(update_callback)),
      created_callbacks_(0),
      pending_callbacks_(0) {}

base::OnceClosure AutoUpdater::CreateCallback() {
  ++created_callbacks_;
  ++pending_callbacks_;
  return base::BindOnce(&AutoUpdater::OnPendingCallback, this);
}

void AutoUpdater::OnPendingCallback() {
  DCHECK_LT(0, pending_callbacks_);
  if (--pending_callbacks_ == 0)
    std::move(update_callback_).Run();
}

AutoUpdater::~AutoUpdater() {
  // If no callbacks are created, then we need to invoke updating in the
  // destructor.
  if (!created_callbacks_)
    std::move(update_callback_).Run();
  else if (pending_callbacks_)
    LOG(ERROR) << "Not all callbacks called. This may happen on shutdown.";
}

struct ProvidedFileSystem::AddWatcherInQueueArgs {
  AddWatcherInQueueArgs(
      size_t token,
      const GURL& origin,
      const base::FilePath& entry_path,
      bool recursive,
      bool persistent,
      storage::AsyncFileUtil::StatusCallback callback,
      storage::WatcherManager::NotificationCallback notification_callback)
      : token(token),
        origin(origin),
        entry_path(entry_path),
        recursive(recursive),
        persistent(persistent),
        callback(std::move(callback)),
        notification_callback(std::move(notification_callback)) {}
  ~AddWatcherInQueueArgs() = default;
  AddWatcherInQueueArgs(AddWatcherInQueueArgs&&) = default;

  const size_t token;
  const GURL origin;
  const base::FilePath entry_path;
  const bool recursive;
  const bool persistent;
  storage::AsyncFileUtil::StatusCallback callback;
  const storage::WatcherManager::NotificationCallback notification_callback;
};

struct ProvidedFileSystem::NotifyInQueueArgs {
  NotifyInQueueArgs(
      size_t token,
      const base::FilePath& entry_path,
      bool recursive,
      storage::WatcherManager::ChangeType change_type,
      std::unique_ptr<ProvidedFileSystemObserver::Changes> changes,
      const std::string& tag,
      storage::AsyncFileUtil::StatusCallback callback)
      : token(token),
        entry_path(entry_path),
        recursive(recursive),
        change_type(change_type),
        changes(std::move(changes)),
        tag(tag),
        callback(std::move(callback)) {}

  NotifyInQueueArgs(const NotifyInQueueArgs&) = delete;
  NotifyInQueueArgs& operator=(const NotifyInQueueArgs&) = delete;

  ~NotifyInQueueArgs() = default;

  const size_t token;
  const base::FilePath entry_path;
  const bool recursive;
  const storage::WatcherManager::ChangeType change_type;
  const std::unique_ptr<ProvidedFileSystemObserver::Changes> changes;
  const std::string tag;
  storage::AsyncFileUtil::StatusCallback callback;
};

ProvidedFileSystem::ProvidedFileSystem(
    Profile* profile,
    const ProvidedFileSystemInfo& file_system_info)
    : profile_(profile),
      event_router_(extensions::EventRouter::Get(profile)),  // May be NULL.
      file_system_info_(file_system_info),
      notification_manager_(
          new NotificationManager(profile_, file_system_info_)),
      watcher_queue_(1) {
  DCHECK_EQ(ProviderId::EXTENSION, file_system_info.provider_id().GetType());
  request_dispatcher_ = std::make_unique<RequestDispatcherImpl>(
      file_system_info_.provider_id().GetExtensionId(), event_router_,
      base::BindRepeating(&ProvidedFileSystem::OnLacrosOperationForwarded,
                          weak_ptr_factory_.GetWeakPtr()),
      GetServiceWorkerLifetimeManager(profile_));
  const ProviderId& provider_id = file_system_info_.provider_id();
  if (chromeos::features::IsUploadOfficeToCloudEnabled() &&
      provider_id.GetExtensionId() == extension_misc::kODFSExtensionId) {
    odfs_metrics_ = std::make_unique<ODFSMetrics>();
  }
  ConstructRequestManager();
}

ProvidedFileSystem::~ProvidedFileSystem() {
  const std::vector<int> request_ids = request_manager_->GetActiveRequestIds();
  for (int request_id : request_ids) {
    Abort(request_id);
  }
}

void ProvidedFileSystem::SetEventRouterForTesting(
    extensions::EventRouter* event_router) {
  event_router_ = event_router;
  request_dispatcher_ = std::make_unique<RequestDispatcherImpl>(
      file_system_info_.provider_id().GetExtensionId(), event_router_,
      base::BindRepeating(&ProvidedFileSystem::OnLacrosOperationForwarded,
                          weak_ptr_factory_.GetWeakPtr()),
      GetServiceWorkerLifetimeManager(profile_));
}

void ProvidedFileSystem::SetNotificationManagerForTesting(
    std::unique_ptr<NotificationManagerInterface> notification_manager) {
  notification_manager_ = std::move(notification_manager);
  ConstructRequestManager();
}

AbortCallback ProvidedFileSystem::RequestUnmount(
    storage::AsyncFileUtil::StatusCallback callback) {
  auto split_callback = base::SplitOnceCallback(std::move(callback));
  const int request_id = request_manager_->CreateRequest(
      RequestType::kUnmount, std::make_unique<operations::Unmount>(
                                 request_dispatcher_.get(), file_system_info_,
                                 std::move(split_callback.first)));
  if (!request_id) {
    std::move(split_callback.second).Run(base::File::FILE_ERROR_SECURITY);
    return AbortCallback();
  }

  return base::BindOnce(&ProvidedFileSystem::Abort,
                        weak_ptr_factory_.GetWeakPtr(), request_id);
}

AbortCallback ProvidedFileSystem::GetMetadata(const base::FilePath& entry_path,
                                              MetadataFieldMask fields,
                                              GetMetadataCallback callback) {
  // Create two callbacks of which only one will be called because
  // RequestManager::CreateRequest() is guaranteed not to call |callback| if it
  // signals an error (by returning request_id == 0).
  auto split_callback = base::SplitOnceCallback(std::move(callback));
  const int request_id = request_manager_->CreateRequest(
      RequestType::kGetMetadata,
      std::make_unique<operations::GetMetadata>(
          request_dispatcher_.get(), file_system_info_, entry_path, fields,
          std::move(split_callback.first)));
  if (!request_id) {
    std::move(split_callback.second)
        .Run(base::WrapUnique<EntryMetadata>(nullptr),
             base::File::FILE_ERROR_SECURITY);
    return AbortCallback();
  }

  return base::BindOnce(&ProvidedFileSystem::Abort,
                        weak_ptr_factory_.GetWeakPtr(), request_id);
}

AbortCallback ProvidedFileSystem::GetActions(
    const std::vector<base::FilePath>& entry_paths,
    GetActionsCallback callback) {
  // Create two callbacks of which only one will be called because
  // RequestManager::CreateRequest() is guaranteed not to call |callback| if it
  // signals an error (by returning request_id == 0).
  auto split_callback = base::SplitOnceCallback(std::move(callback));
  const int request_id = request_manager_->CreateRequest(
      RequestType::kGetActions,
      std::make_unique<operations::GetActions>(
          request_dispatcher_.get(), file_system_info_, entry_paths,
          std::move(split_callback.first)));
  if (!request_id) {
    // If the provider doesn't listen for GetActions requests, treat it as
    // having no actions.
    std::move(split_callback.second).Run(Actions(), base::File::FILE_OK);
    return AbortCallback();
  }

  return base::BindOnce(&ProvidedFileSystem::Abort,
                        weak_ptr_factory_.GetWeakPtr(), request_id);
}

AbortCallback ProvidedFileSystem::ExecuteAction(
    const std::vector<base::FilePath>& entry_paths,
    const std::string& action_id,
    storage::AsyncFileUtil::StatusCallback callback) {
  auto split_callback = base::SplitOnceCallback(std::move(callback));
  const int request_id = request_manager_->CreateRequest(
      RequestType::kExecuteAction,
      std::make_unique<operations::ExecuteAction>(
          request_dispatcher_.get(), file_system_info_, entry_paths, action_id,
          std::move(split_callback.first)));
  if (!request_id) {
    std::move(split_callback.second).Run(base::File::FILE_ERROR_SECURITY);
    return AbortCallback();
  }

  return base::BindOnce(&ProvidedFileSystem::Abort,
                        weak_ptr_factory_.GetWeakPtr(), request_id);
}

AbortCallback ProvidedFileSystem::ReadDirectory(
    const base::FilePath& directory_path,
    storage::AsyncFileUtil::ReadDirectoryCallback callback) {
  const int request_id = request_manager_->CreateRequest(
      RequestType::kReadDirectory,
      std::make_unique<operations::ReadDirectory>(request_dispatcher_.get(),
                                                  file_system_info_,
                                                  directory_path, callback));
  if (!request_id) {
    callback.Run(base::File::FILE_ERROR_SECURITY,
                 storage::AsyncFileUtil::EntryList(),
                 /*has_more=*/false);
    return AbortCallback();
  }

  return base::BindOnce(&ProvidedFileSystem::Abort,
                        weak_ptr_factory_.GetWeakPtr(), request_id);
}

AbortCallback ProvidedFileSystem::ReadFile(int file_handle,
                                           net::IOBuffer* buffer,
                                           int64_t offset,
                                           int length,
                                           ReadChunkReceivedCallback callback) {
  TRACE_EVENT1(
      "file_system_provider", "ProvidedFileSystem::ReadFile", "length", length);
  const int request_id = request_manager_->CreateRequest(
      RequestType::kReadFile,
      std::make_unique<operations::ReadFile>(request_dispatcher_.get(),
                                             file_system_info_, file_handle,
                                             buffer, offset, length, callback));
  if (!request_id) {
    callback.Run(/*chunk_length=*/0,
                 /*has_more=*/false, base::File::FILE_ERROR_SECURITY);
    return AbortCallback();
  }

  return base::BindOnce(&ProvidedFileSystem::Abort,
                        weak_ptr_factory_.GetWeakPtr(), request_id);
}

AbortCallback ProvidedFileSystem::OpenFile(const base::FilePath& file_path,
                                           OpenFileMode mode,
                                           OpenFileCallback callback) {
  // Create two callbacks of which only one will be called because
  // RequestManager::CreateRequest() is guaranteed not to call |callback| if it
  // signals an error (by returning request_id == 0).
  auto split_callback = base::SplitOnceCallback(std::move(callback));
  const int request_id = request_manager_->CreateRequest(
      RequestType::kOpenFile,
      std::make_unique<operations::OpenFile>(
          request_dispatcher_.get(), file_system_info_, file_path, mode,
          base::BindOnce(&ProvidedFileSystem::OnOpenFileCompleted,
                         weak_ptr_factory_.GetWeakPtr(), file_path, mode,
                         std::move(split_callback.first))));
  if (!request_id) {
    std::move(split_callback.second)
        .Run(/*file_handle=*/0, base::File::FILE_ERROR_SECURITY,
             /*cloud_file_info=*/nullptr);
    return AbortCallback();
  }

  return base::BindOnce(&ProvidedFileSystem::Abort,
                        weak_ptr_factory_.GetWeakPtr(), request_id);
}

AbortCallback ProvidedFileSystem::CloseFile(
    int file_handle,
    storage::AsyncFileUtil::StatusCallback callback) {
  auto split_callback = base::SplitOnceCallback(std::move(callback));
  const int request_id = request_manager_->CreateRequest(
      RequestType::kCloseFile,
      std::make_unique<operations::CloseFile>(
          request_dispatcher_.get(), file_system_info_, file_handle,
          base::BindOnce(&ProvidedFileSystem::OnCloseFileCompleted,
                         weak_ptr_factory_.GetWeakPtr(), file_handle,
                         std::move(split_callback.first))));
  if (!request_id) {
    std::move(split_callback.second).Run(base::File::FILE_ERROR_SECURITY);
    return AbortCallback();
  }

  return base::BindOnce(&ProvidedFileSystem::Abort,
                        weak_ptr_factory_.GetWeakPtr(), request_id);
}

AbortCallback ProvidedFileSystem::CreateDirectory(
    const base::FilePath& directory_path,
    bool recursive,
    storage::AsyncFileUtil::StatusCallback callback) {
  auto split_callback = base::SplitOnceCallback(std::move(callback));
  const int request_id = request_manager_->CreateRequest(
      RequestType::kCreateDirectory,
      std::make_unique<operations::CreateDirectory>(
          request_dispatcher_.get(), file_system_info_, directory_path,
          recursive, std::move(split_callback.first)));
  if (!request_id) {
    std::move(split_callback.second).Run(base::File::FILE_ERROR_SECURITY);
    return AbortCallback();
  }

  return base::BindOnce(&ProvidedFileSystem::Abort,
                        weak_ptr_factory_.GetWeakPtr(), request_id);
}

AbortCallback ProvidedFileSystem::DeleteEntry(
    const base::FilePath& entry_path,
    bool recursive,
    storage::AsyncFileUtil::StatusCallback callback) {
  auto split_callback = base::SplitOnceCallback(std::move(callback));
  const int request_id = request_manager_->CreateRequest(
      RequestType::kDeleteEntry,
      std::make_unique<operations::DeleteEntry>(
          request_dispatcher_.get(), file_system_info_, entry_path, recursive,
          std::move(split_callback.first)));
  if (!request_id) {
    std::move(split_callback.second).Run(base::File::FILE_ERROR_SECURITY);
    return AbortCallback();
  }

  return base::BindOnce(&ProvidedFileSystem::Abort,
                        weak_ptr_factory_.GetWeakPtr(), request_id);
}

AbortCallback ProvidedFileSystem::CreateFile(
    const base::FilePath& file_path,
    storage::AsyncFileUtil::StatusCallback callback) {
  auto split_callback = base::SplitOnceCallback(std::move(callback));
  const int request_id = request_manager_->CreateRequest(
      RequestType::kCreateFile,
      std::make_unique<operations::CreateFile>(
          request_dispatcher_.get(), file_system_info_, file_path,
          std::move(split_callback.first)));
  if (!request_id) {
    std::move(split_callback.second).Run(base::File::FILE_ERROR_SECURITY);
    return AbortCallback();
  }

  return base::BindOnce(&ProvidedFileSystem::Abort,
                        weak_ptr_factory_.GetWeakPtr(), request_id);
}

AbortCallback ProvidedFileSystem::CopyEntry(
    const base::FilePath& source_path,
    const base::FilePath& target_path,
    storage::AsyncFileUtil::StatusCallback callback) {
  auto split_callback = base::SplitOnceCallback(std::move(callback));
  const int request_id = request_manager_->CreateRequest(
      RequestType::kCopyEntry,
      std::make_unique<operations::CopyEntry>(
          request_dispatcher_.get(), file_system_info_, source_path,
          target_path, std::move(split_callback.first)));
  if (!request_id) {
    std::move(split_callback.second).Run(base::File::FILE_ERROR_SECURITY);
    return AbortCallback();
  }

  return base::BindOnce(&ProvidedFileSystem::Abort,
                        weak_ptr_factory_.GetWeakPtr(), request_id);
}

AbortCallback ProvidedFileSystem::WriteFile(
    int file_handle,
    net::IOBuffer* buffer,
    int64_t offset,
    int length,
    storage::AsyncFileUtil::StatusCallback callback) {
  TRACE_EVENT1("file_system_provider",
               "ProvidedFileSystem::WriteFile",
               "length",
               length);
  if (length < 0) {
    std::move(callback).Run(base::File::FILE_ERROR_INVALID_OPERATION);
    return AbortCallback();
  }
  auto split_callback = base::SplitOnceCallback(std::move(callback));
  const int request_id = request_manager_->CreateRequest(
      RequestType::kWriteFile,
      std::make_unique<operations::WriteFile>(
          request_dispatcher_.get(), file_system_info_, file_handle,
          base::WrapRefCounted(buffer), offset, static_cast<size_t>(length),
          std::move(split_callback.first)));
  if (!request_id) {
    std::move(split_callback.second).Run(base::File::FILE_ERROR_SECURITY);
    return AbortCallback();
  }

  return base::BindOnce(&ProvidedFileSystem::Abort,
                        weak_ptr_factory_.GetWeakPtr(), request_id);
}

AbortCallback ProvidedFileSystem::FlushFile(
    int file_handle,
    storage::AsyncFileUtil::StatusCallback callback) {
  if (!chromeos::features::IsUploadOfficeToCloudEnabled()) {
    std::move(callback).Run(base::File::FILE_OK);
    return AbortCallback();
  }
  const auto& provider_id = file_system_info_.provider_id();
  bool is_odfs =
      provider_id.GetType() == ProviderId::EXTENSION &&
      provider_id.GetExtensionId() == extension_misc::kODFSExtensionId;
  if (!is_odfs) {
    // No-op for non-ODFS providers for backwards compatibility.
    std::move(callback).Run(base::File::FILE_OK);
    return AbortCallback();
  }

  // Flush for ODFS: run a custom action by path.
  auto it = opened_files_.find(file_handle);
  if (it == opened_files_.end()) {
    std::move(callback).Run(base::File::FILE_ERROR_INVALID_OPERATION);
    return AbortCallback();
  }
  const OpenedFile& opened_file = it->second;
  return ExecuteAction({opened_file.file_path}, kODFSFlushAction,
                       std::move(callback));
}

AbortCallback ProvidedFileSystem::MoveEntry(
    const base::FilePath& source_path,
    const base::FilePath& target_path,
    storage::AsyncFileUtil::StatusCallback callback) {
  auto split_callback = base::SplitOnceCallback(std::move(callback));
  const int request_id = request_manager_->CreateRequest(
      RequestType::kMoveEntry,
      std::make_unique<operations::MoveEntry>(
          request_dispatcher_.get(), file_system_info_, source_path,
          target_path, std::move(split_callback.first)));
  if (!request_id) {
    std::move(split_callback.second).Run(base::File::FILE_ERROR_SECURITY);
    return AbortCallback();
  }

  return base::BindOnce(&ProvidedFileSystem::Abort,
                        weak_ptr_factory_.GetWeakPtr(), request_id);
}

AbortCallback ProvidedFileSystem::Truncate(
    const base::FilePath& file_path,
    int64_t length,
    storage::AsyncFileUtil::StatusCallback callback) {
  auto split_callback = base::SplitOnceCallback(std::move(callback));
  const int request_id = request_manager_->CreateRequest(
      RequestType::kTruncate,
      std::make_unique<operations::Truncate>(
          request_dispatcher_.get(), file_system_info_, file_path, length,
          std::move(split_callback.first)));
  if (!request_id) {
    std::move(split_callback.second).Run(base::File::FILE_ERROR_SECURITY);
    return AbortCallback();
  }

  return base::BindOnce(&ProvidedFileSystem::Abort,
                        weak_ptr_factory_.GetWeakPtr(), request_id);
}

AbortCallback ProvidedFileSystem::AddWatcher(
    const GURL& origin,
    const base::FilePath& entry_path,
    bool recursive,
    bool persistent,
    storage::AsyncFileUtil::StatusCallback callback,
    storage::WatcherManager::NotificationCallback notification_callback) {
  const size_t token = watcher_queue_.NewToken();
  watcher_queue_.Enqueue(
      token,
      base::BindOnce(&ProvidedFileSystem::AddWatcherInQueue,
                     base::Unretained(this),  // Outlived by the queue.
                     AddWatcherInQueueArgs(token, origin, entry_path, recursive,
                                           persistent, std::move(callback),
                                           std::move(notification_callback))));
  return AbortCallback();
}

void ProvidedFileSystem::RemoveWatcher(
    const GURL& origin,
    const base::FilePath& entry_path,
    bool recursive,
    storage::AsyncFileUtil::StatusCallback callback) {
  const size_t token = watcher_queue_.NewToken();
  watcher_queue_.Enqueue(
      token, base::BindOnce(&ProvidedFileSystem::RemoveWatcherInQueue,
                            base::Unretained(this),  // Outlived by the queue.
                            token, origin, entry_path, recursive,
                            std::move(callback)));
}

const ProvidedFileSystemInfo& ProvidedFileSystem::GetFileSystemInfo() const {
  return file_system_info_;
}

OperationRequestManager* ProvidedFileSystem::GetRequestManager() {
  return request_manager_.get();
}

Watchers* ProvidedFileSystem::GetWatchers() {
  return &watchers_;
}

const OpenedFiles& ProvidedFileSystem::GetOpenedFiles() const {
  return opened_files_;
}

void ProvidedFileSystem::AddObserver(ProvidedFileSystemObserver* observer) {
  DCHECK(observer);
  observers_.AddObserver(observer);
}

void ProvidedFileSystem::RemoveObserver(ProvidedFileSystemObserver* observer) {
  DCHECK(observer);
  observers_.RemoveObserver(observer);
}

void ProvidedFileSystem::Notify(
    const base::FilePath& entry_path,
    bool recursive,
    storage::WatcherManager::ChangeType change_type,
    std::unique_ptr<ProvidedFileSystemObserver::Changes> changes,
    const std::string& tag,
    storage::AsyncFileUtil::StatusCallback callback) {
  const size_t token = watcher_queue_.NewToken();
  watcher_queue_.Enqueue(
      token, base::BindOnce(&ProvidedFileSystem::NotifyInQueue,
                            base::Unretained(this),  // Outlived by the queue.
                            std::make_unique<NotifyInQueueArgs>(
                                token, entry_path, recursive, change_type,
                                std::move(changes), tag, std::move(callback))));
}

void ProvidedFileSystem::Configure(
    storage::AsyncFileUtil::StatusCallback callback) {
  auto split_callback = base::SplitOnceCallback(std::move(callback));
  const int request_id = request_manager_->CreateRequest(
      RequestType::kConfigure, std::make_unique<operations::Configure>(
                                   request_dispatcher_.get(), file_system_info_,
                                   std::move(split_callback.first)));
  if (!request_id)
    std::move(split_callback.second).Run(base::File::FILE_ERROR_SECURITY);
}

void ProvidedFileSystem::Abort(int operation_request_id) {
  if (!request_manager_->CreateRequest(
          RequestType::kAbort,
          std::make_unique<operations::Abort>(
              request_dispatcher_.get(), file_system_info_,
              operation_request_id,
              base::BindOnce(&ProvidedFileSystem::OnAbortCompleted,
                             weak_ptr_factory_.GetWeakPtr(),
                             operation_request_id)))) {
    // If the aborting event is not handled, then the operation should simply
    // be not aborted. Instead we'll wait until it completes.
    LOG(ERROR) << "Failed to create an abort request.";
  }
}

void ProvidedFileSystem::OnAbortCompleted(int operation_request_id,
                                          base::File::Error result) {
  if (result != base::File::FILE_OK) {
    // If an error in aborting happens, then do not abort the request in the
    // request manager, as the operation is supposed to complete. The only case
    // it wouldn't complete is if there is a bug in the extension code, and
    // the extension never calls the callback. We consiously *do not* handle
    // bugs in extensions here.
    return;
  }
  request_manager_->RejectRequest(operation_request_id, RequestValue(),
                                  base::File::FILE_ERROR_ABORT);
}

AbortCallback ProvidedFileSystem::AddWatcherInQueue(
    AddWatcherInQueueArgs args) {
  if (args.persistent && (!file_system_info_.supports_notify_tag() ||
                          !args.notification_callback.is_null())) {
    OnAddWatcherInQueueCompleted(args.token, args.entry_path, args.recursive,
                                 Subscriber(), std::move(args.callback),
                                 base::File::FILE_ERROR_INVALID_OPERATION);
    return AbortCallback();
  }

  // Create a candidate subscriber. This could be done in OnAddWatcherCompleted,
  // but base::Bind supports only up to 7 arguments.
  Subscriber subscriber;
  subscriber.origin = args.origin;
  subscriber.persistent = args.persistent;
  subscriber.notification_callback = args.notification_callback;

  const WatcherKey key(args.entry_path, args.recursive);
  const Watchers::const_iterator it = watchers_.find(key);
  if (it != watchers_.end()) {
    const bool exists = it->second.subscribers.find(args.origin) !=
                        it->second.subscribers.end();
    OnAddWatcherInQueueCompleted(
        args.token, args.entry_path, args.recursive, subscriber,
        std::move(args.callback),
        exists ? base::File::FILE_ERROR_EXISTS : base::File::FILE_OK);
    return AbortCallback();
  }

  auto split_callback = base::SplitOnceCallback(std::move(args.callback));
  const int request_id = request_manager_->CreateRequest(
      RequestType::kAddWatcher,
      std::make_unique<operations::AddWatcher>(
          request_dispatcher_.get(), file_system_info_, args.entry_path,
          args.recursive,
          base::BindOnce(&ProvidedFileSystem::OnAddWatcherInQueueCompleted,
                         weak_ptr_factory_.GetWeakPtr(), args.token,
                         args.entry_path, args.recursive, subscriber,
                         std::move(split_callback.first))));

  if (!request_id) {
    OnAddWatcherInQueueCompleted(args.token, args.entry_path, args.recursive,
                                 subscriber, std::move(split_callback.second),
                                 base::File::FILE_ERROR_SECURITY);
  }

  return AbortCallback();
}

AbortCallback ProvidedFileSystem::RemoveWatcherInQueue(
    size_t token,
    const GURL& origin,
    const base::FilePath& entry_path,
    bool recursive,
    storage::AsyncFileUtil::StatusCallback callback) {
  const WatcherKey key(entry_path, recursive);
  const Watchers::iterator it = watchers_.find(key);
  if (it == watchers_.end() ||
      it->second.subscribers.find(origin) == it->second.subscribers.end()) {
    OnRemoveWatcherInQueueCompleted(token, origin, key, std::move(callback),
                                    /*extension_response=*/false,
                                    base::File::FILE_ERROR_NOT_FOUND);
    return AbortCallback();
  }

  // If there are other subscribers, then do not remove the observer, but simply
  // return a success.
  if (it->second.subscribers.size() > 1) {
    OnRemoveWatcherInQueueCompleted(token, origin, key, std::move(callback),
                                    /*extension_response=*/false,
                                    base::File::FILE_OK);
    return AbortCallback();
  }

  // Otherwise, emit an event, and remove the watcher.
  request_manager_->CreateRequest(
      RequestType::kRemoveWatcher,
      std::make_unique<operations::RemoveWatcher>(
          request_dispatcher_.get(), file_system_info_, entry_path, recursive,
          base::BindOnce(&ProvidedFileSystem::OnRemoveWatcherInQueueCompleted,
                         weak_ptr_factory_.GetWeakPtr(), token, origin, key,
                         std::move(callback), /*extension_response=*/true)));

  return AbortCallback();
}

AbortCallback ProvidedFileSystem::NotifyInQueue(
    std::unique_ptr<NotifyInQueueArgs> args) {
  const WatcherKey key(args->entry_path, args->recursive);
  const auto& watcher_it = watchers_.find(key);
  if (watcher_it == watchers_.end()) {
    OnNotifyInQueueCompleted(std::move(args), base::File::FILE_ERROR_NOT_FOUND);
    return AbortCallback();
  }

  // The tag must be provided if and only if it's explicitly supported.
  if (file_system_info_.supports_notify_tag() == args->tag.empty()) {
    OnNotifyInQueueCompleted(std::move(args),
                             base::File::FILE_ERROR_INVALID_OPERATION);
    return AbortCallback();
  }

  // It's illegal to provide a tag which is not unique.
  if (!args->tag.empty() && args->tag == watcher_it->second.last_tag) {
    OnNotifyInQueueCompleted(std::move(args),
                             base::File::FILE_ERROR_INVALID_OPERATION);
    return AbortCallback();
  }

  // The object is owned by AutoUpdated, so the reference is valid as long as
  // callbacks created with AutoUpdater::CreateCallback().
  const ProvidedFileSystemObserver::Changes& changes_ref = *args->changes.get();
  const storage::WatcherManager::ChangeType change_type = args->change_type;

  scoped_refptr<AutoUpdater> auto_updater(new AutoUpdater(base::BindOnce(
      &ProvidedFileSystem::OnNotifyInQueueCompleted,
      weak_ptr_factory_.GetWeakPtr(), std::move(args), base::File::FILE_OK)));

  // Call all notification callbacks (if any).
  for (const auto& subscriber_it : watcher_it->second.subscribers) {
    const storage::WatcherManager::NotificationCallback& notification_callback =
        subscriber_it.second.notification_callback;
    if (!notification_callback.is_null())
      notification_callback.Run(change_type);
  }

  // Notify all observers.
  for (auto& observer : observers_) {
    observer.OnWatcherChanged(file_system_info_, watcher_it->second,
                              change_type, changes_ref,
                              auto_updater->CreateCallback());
  }

  return AbortCallback();
}

base::WeakPtr<ProvidedFileSystemInterface> ProvidedFileSystem::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

std::unique_ptr<ScopedUserInteraction>
ProvidedFileSystem::StartUserInteraction() {
  return std::make_unique<ScopedUserInteractionImpl>(this);
}

void ProvidedFileSystem::OnAddWatcherInQueueCompleted(
    size_t token,
    const base::FilePath& entry_path,
    bool recursive,
    const Subscriber& subscriber,
    storage::AsyncFileUtil::StatusCallback callback,
    base::File::Error result) {
  if (result != base::File::FILE_OK) {
    std::move(callback).Run(result);
    watcher_queue_.Complete(token);
    return;
  }

  const WatcherKey key(entry_path, recursive);
  const Watchers::iterator it = watchers_.find(key);
  if (it != watchers_.end()) {
    std::move(callback).Run(base::File::FILE_OK);
    watcher_queue_.Complete(token);
    return;
  }

  Watcher* const watcher = &watchers_[key];
  watcher->entry_path = entry_path;
  watcher->recursive = recursive;
  watcher->subscribers[subscriber.origin] = subscriber;

  for (auto& observer : observers_)
    observer.OnWatcherListChanged(file_system_info_, watchers_);

  std::move(callback).Run(base::File::FILE_OK);
  watcher_queue_.Complete(token);
}

void ProvidedFileSystem::OnRemoveWatcherInQueueCompleted(
    size_t token,
    const GURL& origin,
    const WatcherKey& key,
    storage::AsyncFileUtil::StatusCallback callback,
    bool extension_response,
    base::File::Error result) {
  if (!extension_response && result != base::File::FILE_OK) {
    watcher_queue_.Complete(token);
    std::move(callback).Run(result);
    return;
  }

  // Even if the extension returns an error, the callback is called with base::
  // File::FILE_OK.
  const auto it = watchers_.find(key);
  DCHECK(it != watchers_.end());
  DCHECK(it->second.subscribers.find(origin) != it->second.subscribers.end());

  it->second.subscribers.erase(origin);

  // If there are no more subscribers, then remove the watcher.
  if (it->second.subscribers.empty())
    watchers_.erase(it);

  for (auto& observer : observers_) {
    observer.OnWatcherListChanged(file_system_info_, watchers_);
  }

  std::move(callback).Run(base::File::FILE_OK);
  watcher_queue_.Complete(token);
}

void ProvidedFileSystem::OnNotifyInQueueCompleted(
    std::unique_ptr<NotifyInQueueArgs> args,
    base::File::Error result) {
  if (result != base::File::FILE_OK) {
    std::move(args->callback).Run(result);
    watcher_queue_.Complete(args->token);
    return;
  }

  // Check if the entry is still watched.
  const WatcherKey key(args->entry_path, args->recursive);
  const Watchers::iterator it = watchers_.find(key);
  if (it == watchers_.end()) {
    std::move(args->callback).Run(base::File::FILE_ERROR_NOT_FOUND);
    watcher_queue_.Complete(args->token);
    return;
  }

  it->second.last_tag = args->tag;

  for (auto& observer : observers_)
    observer.OnWatcherTagUpdated(file_system_info_, it->second);

  // If the watched entry is deleted, then remove the watcher.
  if (args->change_type == storage::WatcherManager::DELETED) {
    // Make a copy, since the |it| iterator will get invalidated on the last
    // subscriber.
    Subscribers subscribers = it->second.subscribers;
    for (const auto& subscriber_it : subscribers) {
      RemoveWatcher(subscriber_it.second.origin, args->entry_path,
                    args->recursive, base::DoNothing());
    }
  }

  std::move(args->callback).Run(base::File::FILE_OK);
  watcher_queue_.Complete(args->token);
}

void ProvidedFileSystem::OnOpenFileCompleted(
    const base::FilePath& file_path,
    OpenFileMode mode,
    OpenFileCallback callback,
    int file_handle,
    base::File::Error result,
    std::unique_ptr<EntryMetadata> metadata) {
  if (result != base::File::FILE_OK) {
    std::move(callback).Run(file_handle, result, std::move(metadata));
    return;
  }

  opened_files_[file_handle] = OpenedFile(file_path, mode);
  std::move(callback).Run(file_handle, base::File::FILE_OK,
                          std::move(metadata));
}

void ProvidedFileSystem::OnCloseFileCompleted(
    int file_handle,
    storage::AsyncFileUtil::StatusCallback callback,
    base::File::Error result) {
  // Closing files is final. Even if an error happened, we remove it from the
  // list of opened files.
  opened_files_.erase(file_handle);
  std::move(callback).Run(result);
}

void ProvidedFileSystem::OnLacrosOperationForwarded(int request_id,
                                                    base::File::Error error) {
  request_manager_->RejectRequest(request_id, RequestValue(), error);
}

void ProvidedFileSystem::ConstructRequestManager() {
  const extensions::ExtensionId& extension_id =
      file_system_info_.provider_id().GetExtensionId();
  base::TimeDelta operation_timeout = kDefaultOperationTimeout;
  if (chromeos::features::IsUploadOfficeToCloudEnabled() &&
      extension_id == extension_misc::kODFSExtensionId) {
    // Longer timeout for ODFS.
    operation_timeout = kODFSOperationTimeout;
  }

  request_manager_ = std::make_unique<OperationRequestManager>(
      profile_, extension_id, notification_manager_.get(), operation_timeout);

  if (chromeos::features::IsUploadOfficeToCloudEnabled() &&
      extension_id == extension_misc::kODFSExtensionId) {
    request_manager_->AddObserver(odfs_metrics_.get());
  }
}

}  // namespace ash::file_system_provider
