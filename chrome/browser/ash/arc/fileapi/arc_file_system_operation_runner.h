// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ARC_FILEAPI_ARC_FILE_SYSTEM_OPERATION_RUNNER_H_
#define CHROME_BROWSER_ASH_ARC_FILEAPI_ARC_FILE_SYSTEM_OPERATION_RUNNER_H_

#include <stdint.h>

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "ash/components/arc/mojom/file_system.mojom-forward.h"
#include "ash/components/arc/session/connection_observer.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "chrome/browser/ash/arc/fileapi/arc_file_system_bridge.h"
#include "chrome/browser/ash/arc/session/arc_session_manager_observer.h"
#include "components/keyed_service/core/keyed_service.h"
#include "storage/browser/file_system/watcher_manager.h"

class BrowserContextKeyedServiceFactory;

namespace ash {
class RecentArcMediaSourceTest;
}

namespace content {
class BrowserContext;
}  // namespace content

namespace arc {

class ArcBridgeService;

// Runs ARC file system operations.
//
// This is an abstraction layer on top of mojom::FileSystemInstance. ARC file
// system operations from chrome to the ARC container which can be initiated
// before the ARC container gets ready should go through this class, rather than
// invoking mojom::FileSystemInstance directly.
//
// When ARC is disabled or ARC has already booted, file system operations are
// performed immediately. While ARC boot is under progress, file operations are
// deferred until ARC boot finishes or the user disables ARC.
//
// This file system operation runner provides better UX when the user attempts
// to perform file operations while ARC is booting. For example:
//
// - Media views are mounted in Files app soon after the user logs into
//   the system. If the user attempts to open media views before ARC boots,
//   a spinner is shown until file system gets ready because ReadDirectory
//   operations are deferred.
// - When an Android content URL is opened soon after the user logs into
//   the system (because the user opened the tab before they logged out for
//   instance), the tab keeps loading until ARC boot finishes, instead of
//   failing immediately.
//
// All member functions must be called on the UI thread.
class ArcFileSystemOperationRunner
    : public KeyedService,
      public ArcFileSystemBridge::Observer,
      public ArcSessionManagerObserver,
      public ConnectionObserver<mojom::FileSystemInstance> {
 public:
  using GetFileSizeCallback = mojom::FileSystemInstance::GetFileSizeCallback;
  using GetMimeTypeCallback = mojom::FileSystemInstance::GetMimeTypeCallback;
  using OpenThumbnailCallback =
      mojom::FileSystemInstance::OpenThumbnailCallback;
  using OpenFileSessionToWriteCallback =
      mojom::FileSystemInstance::OpenFileSessionToWriteCallback;
  using OpenFileSessionToReadCallback =
      mojom::FileSystemInstance::OpenFileSessionToReadCallback;
  using GetDocumentCallback = mojom::FileSystemInstance::GetDocumentCallback;
  using GetChildDocumentsCallback =
      mojom::FileSystemInstance::GetChildDocumentsCallback;
  using GetRecentDocumentsCallback =
      mojom::FileSystemInstance::GetRecentDocumentsCallback;
  using GetRootsCallback = mojom::FileSystemInstance::GetRootsCallback;
  using GetRootSizeCallback = mojom::FileSystemInstance::GetRootSizeCallback;
  using DeleteDocumentCallback =
      mojom::FileSystemInstance::DeleteDocumentCallback;
  using RenameDocumentCallback =
      mojom::FileSystemInstance::RenameDocumentCallback;
  using CreateDocumentCallback =
      mojom::FileSystemInstance::CreateDocumentCallback;
  using CopyDocumentCallback = mojom::FileSystemInstance::CopyDocumentCallback;
  using MoveDocumentCallback = mojom::FileSystemInstance::MoveDocumentCallback;
  using AddWatcherCallback = base::OnceCallback<void(int64_t watcher_id)>;
  using RemoveWatcherCallback = base::OnceCallback<void(bool success)>;
  using ChangeType = storage::WatcherManager::ChangeType;
  using WatcherCallback = base::RepeatingCallback<void(ChangeType type)>;

  class Observer {
   public:
    // Called when the installed watchers are invalidated.
    // This can happen when Android system restarts, for example.
    // After this event is fired, watcher IDs issued before the event can be
    // reused.
    virtual void OnWatchersCleared() = 0;

   protected:
    virtual ~Observer() = default;
  };

  // Returns singleton instance for the given BrowserContext,
  // or nullptr if the browser |context| is not allowed to use ARC.
  static ArcFileSystemOperationRunner* GetForBrowserContext(
      content::BrowserContext* context);

  // Creates an instance suitable for unit tests.
  // This instance will run all operations immediately without deferring by
  // default. Also, deferring can be enabled/disabled by calling
  // SetShouldDefer() from friend classes.
  static std::unique_ptr<ArcFileSystemOperationRunner> CreateForTesting(
      content::BrowserContext* context,
      ArcBridgeService* bridge_service);

  // Returns Factory instance for ArcFileSystemOperationRunner.
  static BrowserContextKeyedServiceFactory* GetFactory();

  ArcFileSystemOperationRunner(content::BrowserContext* context,
                               ArcBridgeService* bridge_service);

  ArcFileSystemOperationRunner(const ArcFileSystemOperationRunner&) = delete;
  ArcFileSystemOperationRunner& operator=(const ArcFileSystemOperationRunner&) =
      delete;

  ~ArcFileSystemOperationRunner() override;

  // Adds or removes observers.
  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Runs file system operations. See file_system.mojom for documentation.
  void GetFileSize(const GURL& url, GetFileSizeCallback callback);
  void GetMimeType(const GURL& url, GetMimeTypeCallback callback);
  void OpenThumbnail(const GURL& url,
                     const gfx::Size& size,
                     OpenThumbnailCallback callback);
  void CloseFileSession(const std::string& session_id,
                        const std::string& error_message);
  void OpenFileSessionToWrite(const GURL& url,
                              OpenFileSessionToWriteCallback callback);
  void OpenFileSessionToRead(const GURL& url,
                             OpenFileSessionToReadCallback callback);
  void GetDocument(const std::string& authority,
                   const std::string& document_id,
                   GetDocumentCallback callback);
  void GetChildDocuments(const std::string& authority,
                         const std::string& parent_document_id,
                         GetChildDocumentsCallback callback);
  void GetRecentDocuments(const std::string& authority,
                          const std::string& root_id,
                          GetRecentDocumentsCallback callback);
  void GetRoots(GetRootsCallback callback);
  void GetRootSize(const std::string& authority,
                   const std::string& root_id,
                   GetRootSizeCallback callback);
  void DeleteDocument(const std::string& authority,
                      const std::string& document_id,
                      DeleteDocumentCallback callback);
  void RenameDocument(const std::string& authority,
                      const std::string& document_id,
                      const std::string& display_name,
                      RenameDocumentCallback callback);
  void CreateDocument(const std::string& authority,
                      const std::string& parent_document_id,
                      const std::string& mime_type,
                      const std::string& display_name,
                      CreateDocumentCallback callback);
  void CopyDocument(const std::string& authority,
                    const std::string& source_document_id,
                    const std::string& target_parent_document_id,
                    CopyDocumentCallback callback);
  void MoveDocument(const std::string& authority,
                    const std::string& source_document_id,
                    const std::string& source_parent_document_id,
                    const std::string& target_parent_document_id,
                    MoveDocumentCallback callback);
  void AddWatcher(const std::string& authority,
                  const std::string& document_id,
                  const WatcherCallback& watcher_callback,
                  AddWatcherCallback callback);
  void RemoveWatcher(int64_t watcher_id, RemoveWatcherCallback callback);

  // KeyedService overrides:
  void Shutdown() override;

  // ArcFileSystemBridge::Observer overrides:
  void OnDocumentChanged(int64_t watcher_id, ChangeType type) override;

  // ArcSessionManagerObserver overrides:
  void OnArcPlayStoreEnabledChanged(bool enabled) override;

  // ConnectionObserver<mojom::FileSystemInstance> overrides:
  void OnConnectionReady() override;
  void OnConnectionClosed() override;

  // Returns true if operations will be deferred.
  bool WillDefer() const { return should_defer_; }

  static void EnsureFactoryBuilt();

 private:
  friend class ArcFileSystemOperationRunnerTest;
  friend class ash::RecentArcMediaSourceTest;

  ArcFileSystemOperationRunner(content::BrowserContext* context,
                               ArcBridgeService* bridge_service,
                               bool set_should_defer_by_events);

  void OnWatcherAdded(const WatcherCallback& watcher_callback,
                      AddWatcherCallback callback,
                      int64_t watcher_id);

  // Called whenever ARC states related to |should_defer_| are changed.
  void OnStateChanged();

  // Enables/disables deferring.
  // Friend unit tests can call this function to simulate enabling/disabling
  // deferring.
  void SetShouldDefer(bool should_defer);

  // Maybe nullptr in unittests.
  const raw_ptr<content::BrowserContext> context_;
  const raw_ptr<ArcBridgeService>
      arc_bridge_service_;  // Owned by ArcServiceManager

  // Indicates if this instance should enable/disable deferring by events.
  // Usually true, but set to false in unit tests.
  bool set_should_defer_by_events_;

  // Set to true if operations should be deferred at this moment.
  // The default is set to false so that operations are not deferred in
  // unit tests.
  bool should_defer_ = false;

  // List of deferred operations.
  std::vector<base::OnceClosure> deferred_operations_;

  // Map from a watcher ID to a watcher callback.
  std::map<int64_t, WatcherCallback> watcher_callbacks_;

  base::ObserverList<Observer>::Unchecked observer_list_;

  base::WeakPtrFactory<ArcFileSystemOperationRunner> weak_ptr_factory_{this};
};

}  // namespace arc

#endif  // CHROME_BROWSER_ASH_ARC_FILEAPI_ARC_FILE_SYSTEM_OPERATION_RUNNER_H_
