// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_ARC_FILEAPI_ARC_FILE_SYSTEM_BRIDGE_H_
#define CHROME_BROWSER_CHROMEOS_ARC_FILEAPI_ARC_FILE_SYSTEM_BRIDGE_H_

#include <stdint.h>

#include <list>
#include <map>
#include <memory>
#include <string>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "chrome/browser/chromeos/arc/fileapi/arc_select_files_handler.h"
#include "chrome/browser/chromeos/arc/fileapi/file_stream_forwarder.h"
#include "components/arc/mojom/file_system.mojom.h"
#include "components/arc/session/connection_observer.h"
#include "components/keyed_service/core/keyed_service.h"
#include "storage/browser/file_system/watcher_manager.h"

class BrowserContextKeyedServiceFactory;
class GURL;
class Profile;

namespace content {
class BrowserContext;
}  // namespace content

namespace arc {

class ArcBridgeService;

// This class handles file system related IPC from the ARC container.
class ArcFileSystemBridge
    : public KeyedService,
      public ConnectionObserver<mojom::FileSystemInstance>,
      public mojom::FileSystemHost {
 public:
  class Observer {
   public:
    virtual void OnDocumentChanged(int64_t watcher_id,
                                   storage::WatcherManager::ChangeType type) {}
    virtual void OnRootsChanged() {}

   protected:
    virtual ~Observer() {}
  };

  ArcFileSystemBridge(content::BrowserContext* context,
                      ArcBridgeService* bridge_service);
  ~ArcFileSystemBridge() override;

  // Returns the factory instance for this class.
  static BrowserContextKeyedServiceFactory* GetFactory();

  // Returns the instance for the given BrowserContext, or nullptr if the
  // browser |context| is not allowed to use ARC.
  static ArcFileSystemBridge* GetForBrowserContext(
      content::BrowserContext* context);
  static ArcFileSystemBridge* GetForBrowserContextForTesting(
      content::BrowserContext* context);

  // Handles a read request.
  bool HandleReadRequest(const std::string& id,
                         int64_t offset,
                         int64_t size,
                         base::ScopedFD pipe_write_end);

  // Releases resources associated with the ID.
  bool HandleIdReleased(const std::string& id);

  // Adds an observer.
  void AddObserver(Observer* observer);

  // Removes an observer.
  void RemoveObserver(Observer* observer);

  // FileSystemHost overrides:
  void GetFileName(const std::string& url,
                   GetFileNameCallback callback) override;
  void GetFileSize(const std::string& url,
                   GetFileSizeCallback callback) override;
  void GetFileType(const std::string& url,
                   GetFileTypeCallback callback) override;
  void OnDocumentChanged(int64_t watcher_id,
                         storage::WatcherManager::ChangeType type) override;
  void OnRootsChanged() override;
  void OpenFileToRead(const std::string& url,
                      OpenFileToReadCallback callback) override;
  void SelectFiles(mojom::SelectFilesRequestPtr request,
                   SelectFilesCallback callback) override;
  void OnFileSelectorEvent(mojom::FileSelectorEventPtr event,
                           OnFileSelectorEventCallback callback) override;
  void GetFileSelectorElements(
      mojom::GetFileSelectorElementsRequestPtr request,
      GetFileSelectorElementsCallback callback) override;

  // ConnectionObserver<mojom::FileSystemInstance> overrides:
  void OnConnectionClosed() override;

 private:
  // Used to implement OpenFileToRead().
  void OpenFileToReadAfterGetFileSize(const GURL& url_decoded,
                                      OpenFileToReadCallback callback,
                                      int64_t size);

  // Used to implement OpenFileToRead().
  void OnOpenFile(const GURL& url_decoded,
                  OpenFileToReadCallback callback,
                  const std::string& id,
                  base::ScopedFD fd);

  // Called when FileStreamForwarder completes read request.
  void OnReadRequestCompleted(const std::string& id,
                              std::list<FileStreamForwarderPtr>::iterator it,
                              bool result);

  Profile* const profile_;
  ArcBridgeService* const bridge_service_;  // Owned by ArcServiceManager
  base::ObserverList<Observer>::Unchecked observer_list_;

  // Map from file descriptor IDs to requested URLs.
  std::map<std::string, GURL> id_to_url_;

  std::list<FileStreamForwarderPtr> file_stream_forwarders_;

  std::unique_ptr<ArcSelectFilesHandlersManager> select_files_handlers_manager_;

  base::WeakPtrFactory<ArcFileSystemBridge> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(ArcFileSystemBridge);
};

}  // namespace arc

#endif  // CHROME_BROWSER_CHROMEOS_ARC_FILEAPI_ARC_FILE_SYSTEM_BRIDGE_H_
