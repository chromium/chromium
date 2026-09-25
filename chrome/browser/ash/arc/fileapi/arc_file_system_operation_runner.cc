// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/fileapi/arc_file_system_operation_runner.h"

#include <optional>
#include <utility>

#include "base/functional/bind.h"
#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/no_destructor.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/ash/arc/arc_util.h"
#include "chrome/browser/ash/arc/fileapi/arc_file_system_bridge.h"
#include "chrome/browser/ash/arc/session/arc_session_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chromeos/ash/experiences/arc/arc_browser_context_keyed_service_factory_base.h"
#include "chromeos/ash/experiences/arc/session/arc_bridge_service.h"
#include "content/public/browser/browser_thread.h"
#include "mojo/public/cpp/platform/platform_handle.h"
#include "url/gurl.h"

using content::BrowserThread;

namespace arc {
namespace {

// Singleton factory for ArcFileSystemOperationRunner.
class ArcFileSystemOperationRunnerFactory
    : public internal::ArcBrowserContextKeyedServiceFactoryBase<
          ArcFileSystemOperationRunner,
          ArcFileSystemOperationRunnerFactory> {
 public:
  // Factory name used by ArcBrowserContextKeyedServiceFactoryBase.
  static constexpr const char* kName = "ArcFileSystemOperationRunnerFactory";

  static ArcFileSystemOperationRunnerFactory* GetInstance() {
    static base::NoDestructor<ArcFileSystemOperationRunnerFactory> instance;
    return instance.get();
  }

 private:
  friend base::NoDestructor<ArcFileSystemOperationRunnerFactory>;
  ArcFileSystemOperationRunnerFactory() {
    DependsOn(ArcFileSystemBridge::GetFactory());
  }
  ~ArcFileSystemOperationRunnerFactory() override = default;
};

}  // namespace

// static
ArcFileSystemOperationRunner*
ArcFileSystemOperationRunner::GetForBrowserContext(
    content::BrowserContext* context) {
  return ArcFileSystemOperationRunnerFactory::GetForBrowserContext(context);
}

// static
BrowserContextKeyedServiceFactory* ArcFileSystemOperationRunner::GetFactory() {
  return ArcFileSystemOperationRunnerFactory::GetInstance();
}

// static
std::unique_ptr<ArcFileSystemOperationRunner>
ArcFileSystemOperationRunner::CreateForTesting(
    content::BrowserContext* context,
    ArcBridgeService* bridge_service,
    size_t content_url_allowlist_cache_size) {
  // We can't use std::make_unique() here because we are calling a private
  // constructor.
  return base::WrapUnique<ArcFileSystemOperationRunner>(
      new ArcFileSystemOperationRunner(context, bridge_service, false,
                                       content_url_allowlist_cache_size));
}

ArcFileSystemOperationRunner::ArcFileSystemOperationRunner(
    content::BrowserContext* context,
    ArcBridgeService* bridge_service)
    : ArcFileSystemOperationRunner(
          Profile::FromBrowserContext(context),
          bridge_service,
          /*set_should_defer_by_events=*/true,
          ArcContentUrlAllowlist::kLruCacheDefaultMaxSize) {
  CHECK(context, base::NotFatalUntil::M160);
}

ArcFileSystemOperationRunner::ArcFileSystemOperationRunner(
    content::BrowserContext* context,
    ArcBridgeService* bridge_service,
    bool set_should_defer_by_events,
    size_t content_url_allowlist_cache_size)
    : context_(context),
      arc_bridge_service_(bridge_service),
      set_should_defer_by_events_(set_should_defer_by_events),
      content_url_allowlist_(
          context ? Profile::FromBrowserContext(context)->GetPath().AppendASCII(
                        "arc_content_urls.db")
                  : base::FilePath(),
          content_url_allowlist_cache_size) {
  CHECK_CURRENTLY_ON(BrowserThread::UI, base::NotFatalUntil::M160);

  arc_bridge_service_->file_system()->AddObserver(this);

  // ArcSessionManager may not exist in unit tests.
  auto* arc_session_manager = ArcSessionManager::Get();
  if (arc_session_manager)
    arc_session_manager->AddObserver(this);

  ArcFileSystemBridge::GetForBrowserContext(context_)->AddObserver(this);

  OnStateChanged();
}

ArcFileSystemOperationRunner::~ArcFileSystemOperationRunner() {
  CHECK_CURRENTLY_ON(BrowserThread::UI, base::NotFatalUntil::M160);
  // On destruction, deferred operations are discarded.
}

void ArcFileSystemOperationRunner::AddObserver(Observer* observer) {
  CHECK_CURRENTLY_ON(BrowserThread::UI, base::NotFatalUntil::M160);
  observer_list_.AddObserver(observer);
}

void ArcFileSystemOperationRunner::RemoveObserver(Observer* observer) {
  CHECK_CURRENTLY_ON(BrowserThread::UI, base::NotFatalUntil::M160);
  observer_list_.RemoveObserver(observer);
}

void ArcFileSystemOperationRunner::GrantAccessToContentUrl(const GURL& url) {
  CHECK_CURRENTLY_ON(BrowserThread::UI, base::NotFatalUntil::M160);
  content_url_allowlist_.GrantAccess(url);
}

void ArcFileSystemOperationRunner::GetFileSize(const GURL& url,
                                               GetFileSizeCallback callback) {
  CHECK_CURRENTLY_ON(BrowserThread::UI, base::NotFatalUntil::M160);
  if (should_defer_) {
    deferred_operations_.emplace_back(base::BindOnce(
        &ArcFileSystemOperationRunner::GetFileSize,
        weak_ptr_factory_.GetWeakPtr(), url, std::move(callback)));
    return;
  }
  IsContentUrlAccessible(
      url,
      base::BindOnce(&ArcFileSystemOperationRunner::GetFileSizeAfterAccessCheck,
                     weak_ptr_factory_.GetWeakPtr(), url, std::move(callback)));
}

void ArcFileSystemOperationRunner::GetFileSizeAfterAccessCheck(
    const GURL& url,
    GetFileSizeCallback callback,
    bool accessible) {
  CHECK_CURRENTLY_ON(BrowserThread::UI, base::NotFatalUntil::M160);
  if (!accessible) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), -1));
    return;
  }
  auto* file_system_instance = ARC_GET_INSTANCE_FOR_METHOD(
      arc_bridge_service_->file_system(), GetFileSize);
  if (!file_system_instance) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), -1));
    return;
  }
  file_system_instance->GetFileSize(url.spec(), std::move(callback));
}

void ArcFileSystemOperationRunner::GetMimeType(const GURL& url,
                                               GetMimeTypeCallback callback) {
  CHECK_CURRENTLY_ON(BrowserThread::UI, base::NotFatalUntil::M160);
  if (should_defer_) {
    deferred_operations_.emplace_back(base::BindOnce(
        &ArcFileSystemOperationRunner::GetMimeType,
        weak_ptr_factory_.GetWeakPtr(), url, std::move(callback)));
    return;
  }
  IsContentUrlAccessible(
      url,
      base::BindOnce(&ArcFileSystemOperationRunner::GetMimeTypeAfterAccessCheck,
                     weak_ptr_factory_.GetWeakPtr(), url, std::move(callback)));
}

void ArcFileSystemOperationRunner::GetMimeTypeAfterAccessCheck(
    const GURL& url,
    GetMimeTypeCallback callback,
    bool accessible) {
  CHECK_CURRENTLY_ON(BrowserThread::UI, base::NotFatalUntil::M160);
  if (!accessible) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), std::nullopt));
    return;
  }
  auto* file_system_instance = ARC_GET_INSTANCE_FOR_METHOD(
      arc_bridge_service_->file_system(), GetMimeType);
  if (!file_system_instance) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), std::nullopt));
    return;
  }
  file_system_instance->GetMimeType(url.spec(), std::move(callback));
}

void ArcFileSystemOperationRunner::OpenThumbnail(
    const GURL& url,
    const gfx::Size& size,
    OpenThumbnailCallback callback) {
  CHECK_CURRENTLY_ON(BrowserThread::UI, base::NotFatalUntil::M160);
  if (should_defer_) {
    deferred_operations_.emplace_back(base::BindOnce(
        &ArcFileSystemOperationRunner::OpenThumbnail,
        weak_ptr_factory_.GetWeakPtr(), url, size, std::move(callback)));
    return;
  }
  IsContentUrlAccessible(
      url, base::BindOnce(
               &ArcFileSystemOperationRunner::OpenThumbnailAfterAccessCheck,
               weak_ptr_factory_.GetWeakPtr(), url, size, std::move(callback)));
}

void ArcFileSystemOperationRunner::OpenThumbnailAfterAccessCheck(
    const GURL& url,
    const gfx::Size& size,
    OpenThumbnailCallback callback,
    bool accessible) {
  CHECK_CURRENTLY_ON(BrowserThread::UI, base::NotFatalUntil::M160);
  if (!accessible) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), mojo::PlatformHandle()));
    return;
  }
  auto* file_system_instance = ARC_GET_INSTANCE_FOR_METHOD(
      arc_bridge_service_->file_system(), OpenThumbnail);
  if (!file_system_instance) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), mojo::PlatformHandle()));
    return;
  }
  file_system_instance->OpenThumbnail(url.spec(), size, std::move(callback));
}

void ArcFileSystemOperationRunner::CloseFileSession(
    const std::string& url_id,
    const std::string& error_message) {
  CHECK_CURRENTLY_ON(BrowserThread::UI, base::NotFatalUntil::M160);
  if (should_defer_) {
    deferred_operations_.emplace_back(
        base::BindOnce(&ArcFileSystemOperationRunner::CloseFileSession,
                       weak_ptr_factory_.GetWeakPtr(), url_id, error_message));
    return;
  }
  auto* file_system_instance = ARC_GET_INSTANCE_FOR_METHOD(
      arc_bridge_service_->file_system(), CloseFileSession);
  if (!file_system_instance) {
    LOG(WARNING) << "Failed to call CloseFileSession.";
    return;
  }
  file_system_instance->CloseFileSession(url_id, error_message);
}

void ArcFileSystemOperationRunner::OpenFileSessionToWrite(
    const GURL& url,
    OpenFileSessionToWriteCallback callback) {
  CHECK_CURRENTLY_ON(BrowserThread::UI, base::NotFatalUntil::M160);
  if (should_defer_) {
    deferred_operations_.emplace_back(base::BindOnce(
        &ArcFileSystemOperationRunner::OpenFileSessionToWrite,
        weak_ptr_factory_.GetWeakPtr(), url, std::move(callback)));
    return;
  }
  IsContentUrlAccessible(
      url,
      base::BindOnce(
          &ArcFileSystemOperationRunner::OpenFileSessionToWriteAfterAccessCheck,
          weak_ptr_factory_.GetWeakPtr(), url, std::move(callback)));
}

void ArcFileSystemOperationRunner::OpenFileSessionToWriteAfterAccessCheck(
    const GURL& url,
    OpenFileSessionToWriteCallback callback,
    bool accessible) {
  CHECK_CURRENTLY_ON(BrowserThread::UI, base::NotFatalUntil::M160);
  if (!accessible) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback), mojom::FileSessionPtr()));
    return;
  }
  auto* file_system_instance = ARC_GET_INSTANCE_FOR_METHOD(
      arc_bridge_service_->file_system(), OpenFileSessionToWrite);
  if (!file_system_instance) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback), mojom::FileSessionPtr()));
    return;
  }
  file_system_instance->OpenFileSessionToWrite(url, std::move(callback));
}

void ArcFileSystemOperationRunner::OpenFileSessionToRead(
    const GURL& url,
    OpenFileSessionToReadCallback callback) {
  CHECK_CURRENTLY_ON(BrowserThread::UI, base::NotFatalUntil::M160);
  if (should_defer_) {
    deferred_operations_.emplace_back(base::BindOnce(
        &ArcFileSystemOperationRunner::OpenFileSessionToRead,
        weak_ptr_factory_.GetWeakPtr(), url, std::move(callback)));
    return;
  }
  IsContentUrlAccessible(
      url,
      base::BindOnce(
          &ArcFileSystemOperationRunner::OpenFileSessionToReadAfterAccessCheck,
          weak_ptr_factory_.GetWeakPtr(), url, std::move(callback)));
}

void ArcFileSystemOperationRunner::OpenFileSessionToReadAfterAccessCheck(
    const GURL& url,
    OpenFileSessionToReadCallback callback,
    bool accessible) {
  CHECK_CURRENTLY_ON(BrowserThread::UI, base::NotFatalUntil::M160);
  if (!accessible) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback), mojom::FileSessionPtr()));
    return;
  }
  auto* file_system_instance = ARC_GET_INSTANCE_FOR_METHOD(
      arc_bridge_service_->file_system(), OpenFileSessionToRead);
  if (!file_system_instance) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback), mojom::FileSessionPtr()));
    return;
  }
  file_system_instance->OpenFileSessionToRead(url, std::move(callback));
}

void ArcFileSystemOperationRunner::GetDocument(const std::string& authority,
                                               const std::string& document_id,
                                               GetDocumentCallback callback) {
  CHECK_CURRENTLY_ON(BrowserThread::UI, base::NotFatalUntil::M160);
  if (should_defer_) {
    deferred_operations_.emplace_back(
        base::BindOnce(&ArcFileSystemOperationRunner::GetDocument,
                       weak_ptr_factory_.GetWeakPtr(), authority, document_id,
                       std::move(callback)));
    return;
  }
  auto* file_system_instance = ARC_GET_INSTANCE_FOR_METHOD(
      arc_bridge_service_->file_system(), GetDocument);
  if (!file_system_instance) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), mojom::DocumentPtr()));
    return;
  }
  file_system_instance->GetDocument(authority, document_id,
                                    std::move(callback));
}

void ArcFileSystemOperationRunner::GetChildDocuments(
    const std::string& authority,
    const std::string& parent_document_id,
    GetChildDocumentsCallback callback) {
  CHECK_CURRENTLY_ON(BrowserThread::UI, base::NotFatalUntil::M160);
  if (should_defer_) {
    deferred_operations_.emplace_back(
        base::BindOnce(&ArcFileSystemOperationRunner::GetChildDocuments,
                       weak_ptr_factory_.GetWeakPtr(), authority,
                       parent_document_id, std::move(callback)));
    return;
  }
  auto* file_system_instance = ARC_GET_INSTANCE_FOR_METHOD(
      arc_bridge_service_->file_system(), GetChildDocuments);
  if (!file_system_instance) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), std::nullopt));
    return;
  }
  file_system_instance->GetChildDocuments(authority, parent_document_id,
                                          std::move(callback));
}

void ArcFileSystemOperationRunner::GetRecentDocuments(
    const std::string& authority,
    const std::string& root_id,
    GetRecentDocumentsCallback callback) {
  CHECK_CURRENTLY_ON(BrowserThread::UI, base::NotFatalUntil::M160);
  if (should_defer_) {
    deferred_operations_.emplace_back(
        base::BindOnce(&ArcFileSystemOperationRunner::GetRecentDocuments,
                       weak_ptr_factory_.GetWeakPtr(), authority, root_id,
                       std::move(callback)));
    return;
  }
  auto* file_system_instance = ARC_GET_INSTANCE_FOR_METHOD(
      arc_bridge_service_->file_system(), GetRecentDocuments);
  if (!file_system_instance) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), std::nullopt));
    return;
  }
  file_system_instance->GetRecentDocuments(authority, root_id,
                                           std::move(callback));
}

void ArcFileSystemOperationRunner::GetRoots(GetRootsCallback callback) {
  CHECK_CURRENTLY_ON(BrowserThread::UI, base::NotFatalUntil::M160);
  if (should_defer_) {
    deferred_operations_.emplace_back(
        base::BindOnce(&ArcFileSystemOperationRunner::GetRoots,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
    return;
  }
  auto* file_system_instance =
      ARC_GET_INSTANCE_FOR_METHOD(arc_bridge_service_->file_system(), GetRoots);
  if (!file_system_instance) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), std::nullopt));
    return;
  }
  file_system_instance->GetRoots(std::move(callback));
}

void ArcFileSystemOperationRunner::GetRootSize(const std::string& authority,
                                               const std::string& root_id,
                                               GetRootSizeCallback callback) {
  CHECK_CURRENTLY_ON(BrowserThread::UI, base::NotFatalUntil::M160);
  if (should_defer_) {
    deferred_operations_.emplace_back(
        base::BindOnce(&ArcFileSystemOperationRunner::GetRootSize,
                       weak_ptr_factory_.GetWeakPtr(), authority, root_id,
                       std::move(callback)));
    return;
  }
  auto* file_system_instance = ARC_GET_INSTANCE_FOR_METHOD(
      arc_bridge_service_->file_system(), GetRootSize);
  if (!file_system_instance) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), mojom::RootSizePtr()));
    return;
  }
  file_system_instance->GetRootSize(authority, root_id, std::move(callback));
}

void ArcFileSystemOperationRunner::DeleteDocument(
    const std::string& authority,
    const std::string& document_id,
    DeleteDocumentCallback callback) {
  if (should_defer_) {
    deferred_operations_.emplace_back(
        base::BindOnce(&ArcFileSystemOperationRunner::DeleteDocument,
                       weak_ptr_factory_.GetWeakPtr(), authority, document_id,
                       std::move(callback)));
    return;
  }
  auto* file_system_instance = ARC_GET_INSTANCE_FOR_METHOD(
      arc_bridge_service_->file_system(), DeleteDocument);
  if (!file_system_instance) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), false));
    return;
  }
  file_system_instance->DeleteDocument(authority, document_id,
                                       std::move(callback));
}

void ArcFileSystemOperationRunner::RenameDocument(
    const std::string& authority,
    const std::string& document_id,
    const std::string& display_name,
    RenameDocumentCallback callback) {
  if (should_defer_) {
    deferred_operations_.emplace_back(
        base::BindOnce(&ArcFileSystemOperationRunner::RenameDocument,
                       weak_ptr_factory_.GetWeakPtr(), authority, document_id,
                       display_name, std::move(callback)));
    return;
  }
  auto* file_system_instance = ARC_GET_INSTANCE_FOR_METHOD(
      arc_bridge_service_->file_system(), RenameDocument);
  if (!file_system_instance) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), mojom::DocumentPtr()));
    return;
  }
  file_system_instance->RenameDocument(authority, document_id, display_name,
                                       std::move(callback));
}

void ArcFileSystemOperationRunner::CreateDocument(
    const std::string& authority,
    const std::string& parent_document_id,
    const std::string& mime_type,
    const std::string& display_name,
    CreateDocumentCallback callback) {
  if (should_defer_) {
    deferred_operations_.emplace_back(base::BindOnce(
        &ArcFileSystemOperationRunner::CreateDocument,
        weak_ptr_factory_.GetWeakPtr(), authority, parent_document_id,
        mime_type, display_name, std::move(callback)));
    return;
  }
  auto* file_system_instance = ARC_GET_INSTANCE_FOR_METHOD(
      arc_bridge_service_->file_system(), CreateDocument);
  if (!file_system_instance) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), mojom::DocumentPtr()));
    return;
  }
  file_system_instance->CreateDocument(authority, parent_document_id, mime_type,
                                       display_name, std::move(callback));
}

void ArcFileSystemOperationRunner::CopyDocument(
    const std::string& authority,
    const std::string& source_document_id,
    const std::string& target_parent_document_id,
    CopyDocumentCallback callback) {
  if (should_defer_) {
    deferred_operations_.emplace_back(base::BindOnce(
        &ArcFileSystemOperationRunner::CopyDocument,
        weak_ptr_factory_.GetWeakPtr(), authority, source_document_id,
        target_parent_document_id, std::move(callback)));
    return;
  }
  auto* file_system_instance = ARC_GET_INSTANCE_FOR_METHOD(
      arc_bridge_service_->file_system(), CopyDocument);
  if (!file_system_instance) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), mojom::DocumentPtr()));
    return;
  }
  file_system_instance->CopyDocument(authority, source_document_id,
                                     target_parent_document_id,
                                     std::move(callback));
}

void ArcFileSystemOperationRunner::MoveDocument(
    const std::string& authority,
    const std::string& source_document_id,
    const std::string& source_parent_document_id,
    const std::string& target_parent_document_id,
    MoveDocumentCallback callback) {
  if (should_defer_) {
    deferred_operations_.emplace_back(
        base::BindOnce(&ArcFileSystemOperationRunner::MoveDocument,
                       weak_ptr_factory_.GetWeakPtr(), authority,
                       source_document_id, source_parent_document_id,
                       target_parent_document_id, std::move(callback)));
    return;
  }
  auto* file_system_instance = ARC_GET_INSTANCE_FOR_METHOD(
      arc_bridge_service_->file_system(), MoveDocument);
  if (!file_system_instance) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), mojom::DocumentPtr()));
    return;
  }
  file_system_instance->MoveDocument(
      authority, source_document_id, source_parent_document_id,
      target_parent_document_id, std::move(callback));
}

void ArcFileSystemOperationRunner::AddWatcher(
    const std::string& authority,
    const std::string& document_id,
    const WatcherCallback& watcher_callback,
    AddWatcherCallback callback) {
  CHECK_CURRENTLY_ON(BrowserThread::UI, base::NotFatalUntil::M160);
  if (should_defer_) {
    deferred_operations_.emplace_back(
        base::BindOnce(&ArcFileSystemOperationRunner::AddWatcher,
                       weak_ptr_factory_.GetWeakPtr(), authority, document_id,
                       watcher_callback, std::move(callback)));
    return;
  }
  auto* file_system_instance = ARC_GET_INSTANCE_FOR_METHOD(
      arc_bridge_service_->file_system(), AddWatcher);
  if (!file_system_instance) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), -1));
    return;
  }
  file_system_instance->AddWatcher(
      authority, document_id,
      base::BindOnce(&ArcFileSystemOperationRunner::OnWatcherAdded,
                     weak_ptr_factory_.GetWeakPtr(), watcher_callback,
                     std::move(callback)));
}

void ArcFileSystemOperationRunner::RemoveWatcher(
    int64_t watcher_id,
    RemoveWatcherCallback callback) {
  CHECK_CURRENTLY_ON(BrowserThread::UI, base::NotFatalUntil::M160);
  // RemoveWatcher() is never deferred since watchers do not persist across
  // container reboots.
  if (should_defer_) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), false));
    return;
  }

  // Unregister from |watcher_callbacks_| now because we will do it even if
  // the remote method fails anyway. This is an implementation detail, so
  // users must not assume registered callbacks are immediately invalidated.
  auto iter = watcher_callbacks_.find(watcher_id);
  if (iter == watcher_callbacks_.end()) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), false));
    return;
  }
  watcher_callbacks_.erase(iter);

  auto* file_system_instance = ARC_GET_INSTANCE_FOR_METHOD(
      arc_bridge_service_->file_system(), RemoveWatcher);
  if (!file_system_instance) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), false));
    return;
  }
  file_system_instance->RemoveWatcher(watcher_id, std::move(callback));
}

void ArcFileSystemOperationRunner::Shutdown() {
  CHECK_CURRENTLY_ON(BrowserThread::UI, base::NotFatalUntil::M160);
  ArcFileSystemBridge::GetForBrowserContext(context_)->RemoveObserver(this);

  // ArcSessionManager may not exist in unit tests.
  auto* arc_session_manager = ArcSessionManager::Get();
  if (arc_session_manager)
    arc_session_manager->RemoveObserver(this);

  arc_bridge_service_->file_system()->RemoveObserver(this);
}

void ArcFileSystemOperationRunner::OnDocumentChanged(int64_t watcher_id,
                                                     ChangeType type) {
  CHECK_CURRENTLY_ON(BrowserThread::UI, base::NotFatalUntil::M160);
  auto iter = watcher_callbacks_.find(watcher_id);
  if (iter == watcher_callbacks_.end()) {
    // This may happen in a race condition with documents changes and
    // RemoveWatcher().
    return;
  }
  WatcherCallback watcher_callback = iter->second;
  watcher_callback.Run(type);
}

void ArcFileSystemOperationRunner::OnArcPlayStoreEnabledChanged(bool enabled) {
  CHECK_CURRENTLY_ON(BrowserThread::UI, base::NotFatalUntil::M160);
  OnStateChanged();
}

void ArcFileSystemOperationRunner::OnConnectionReady() {
  CHECK_CURRENTLY_ON(BrowserThread::UI, base::NotFatalUntil::M160);
  OnStateChanged();
}

void ArcFileSystemOperationRunner::OnConnectionClosed() {
  CHECK_CURRENTLY_ON(BrowserThread::UI, base::NotFatalUntil::M160);
  // ArcFileSystemService and watchers are gone.
  watcher_callbacks_.clear();
  for (auto& observer : observer_list_)
    observer.OnWatchersCleared();
  OnStateChanged();
}

void ArcFileSystemOperationRunner::OnWatcherAdded(
    const WatcherCallback& watcher_callback,
    AddWatcherCallback callback,
    int64_t watcher_id) {
  CHECK_CURRENTLY_ON(BrowserThread::UI, base::NotFatalUntil::M160);
  if (watcher_id < 0) {
    std::move(callback).Run(-1);
    return;
  }
  if (watcher_callbacks_.count(watcher_id)) {
    NOTREACHED();
  }
  watcher_callbacks_.insert(std::make_pair(watcher_id, watcher_callback));
  std::move(callback).Run(watcher_id);
}

void ArcFileSystemOperationRunner::OnStateChanged() {
  CHECK_CURRENTLY_ON(BrowserThread::UI, base::NotFatalUntil::M160);
  if (set_should_defer_by_events_) {
    SetShouldDefer(IsArcPlayStoreEnabledForProfile(
                       Profile::FromBrowserContext(context_)) &&
                   !arc_bridge_service_->file_system()->IsConnected());
  }
}

void ArcFileSystemOperationRunner::SetShouldDefer(bool should_defer) {
  CHECK_CURRENTLY_ON(BrowserThread::UI, base::NotFatalUntil::M160);

  should_defer_ = should_defer;

  if (should_defer_)
    return;

  // Run deferred operations.
  std::vector<base::OnceClosure> deferred_operations;
  deferred_operations.swap(deferred_operations_);
  for (base::OnceClosure& operation : deferred_operations)
    std::move(operation).Run();

  // No deferred operations should be left at this point.
  CHECK(deferred_operations_.empty(), base::NotFatalUntil::M160);
}

void ArcFileSystemOperationRunner::IsContentUrlAccessible(
    const GURL& url,
    base::OnceCallback<void(bool)> callback) {
  CHECK_CURRENTLY_ON(BrowserThread::UI, base::NotFatalUntil::M160);
  content_url_allowlist_.IsAccessGranted(url, std::move(callback));
}

// static
void ArcFileSystemOperationRunner::EnsureFactoryBuilt() {
  ArcFileSystemOperationRunnerFactory::GetInstance();
}

}  // namespace arc
