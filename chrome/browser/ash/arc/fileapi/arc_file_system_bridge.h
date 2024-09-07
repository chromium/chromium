// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ARC_FILEAPI_ARC_FILE_SYSTEM_BRIDGE_H_
#define CHROME_BROWSER_ASH_ARC_FILEAPI_ARC_FILE_SYSTEM_BRIDGE_H_

#include <stdint.h>

#include <list>
#include <map>
#include <memory>
#include <string>

#include "ash/components/arc/mojom/file_system.mojom-forward.h"
#include "ash/components/arc/session/connection_observer.h"
#include "base/files/file_path.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "chrome/browser/ash/arc/fileapi/arc_select_files_handler.h"
#include "chrome/browser/ash/arc/fileapi/file_stream_forwarder.h"
#include "chrome/browser/ash/fusebox/fusebox_moniker.h"
#include "components/keyed_service/core/keyed_service.h"
#include "storage/browser/file_system/file_system_operation.h"
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
  using OpenFileToReadCallback = mojom::FileSystemHost::OpenFileToReadCallback;

  class Observer {
   public:
    virtual void OnDocumentChanged(int64_t watcher_id,
                                   storage::WatcherManager::ChangeType type) {}

    // Propagates `mojom::FileSystemHost::OnMediaStoreUriAdded()` events from
    // ARC to observers. See payload details in mojo interface documentation:
    // /ash/components/arc/mojom/file_system.mojom.
    virtual void OnMediaStoreUriAdded(
        const GURL& uri,
        const mojom::MediaStoreMetadata& metadata) {}

    virtual void OnRootsChanged() {}

   protected:
    virtual ~Observer() {}
  };

  ArcFileSystemBridge(content::BrowserContext* context,
                      ArcBridgeService* bridge_service);

  ArcFileSystemBridge(const ArcFileSystemBridge&) = delete;
  ArcFileSystemBridge& operator=(const ArcFileSystemBridge&) = delete;

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
  void GetLastModified(const GURL& url,
                       GetLastModifiedCallback callback) override;
  void GetFileType(const std::string& url,
                   GetFileTypeCallback callback) override;
  void OnDocumentChanged(int64_t watcher_id,
                         storage::WatcherManager::ChangeType type) override;
  void OnRootsChanged() override;
  void GetVirtualFileId(const std::string& url,
                        GetVirtualFileIdCallback callback) override;
  void HandleIdReleased(const std::string& id,
                        HandleIdReleasedCallback callback) override;
  void OpenFileToRead(const std::string& url,
                      OpenFileToReadCallback callback) override;
  void SelectFiles(mojom::SelectFilesRequestPtr request,
                   SelectFilesCallback callback) override;
  void OnFileSelectorEvent(mojom::FileSelectorEventPtr event,
                           OnFileSelectorEventCallback callback) override;
  void GetFileSelectorElements(
      mojom::GetFileSelectorElementsRequestPtr request,
      GetFileSelectorElementsCallback callback) override;
  void OnMediaStoreUriAdded(const GURL& uri,
                            mojom::MediaStoreMetadataPtr metadata) override;
  void CreateMoniker(const GURL& content_uri,
                     bool read_only,
                     CreateMonikerCallback callback) override;
  void DestroyMoniker(const fusebox::Moniker& moniker,
                      DestroyMonikerCallback callback) override;

  // ConnectionObserver<mojom::FileSystemInstance> overrides:
  void OnConnectionClosed() override;

 private:
  FRIEND_TEST_ALL_PREFIXES(ArcFileSystemBridgeTest,
                           GetLinuxVFSPathFromExternalFileURL);
  FRIEND_TEST_ALL_PREFIXES(ArcFileSystemBridgeTest,
                           GetLinuxVFSPathForPathOnFileSystemType);
  FRIEND_TEST_ALL_PREFIXES(ArcFileSystemBridgeTest, MaxNumberOfSharedMonikers);

  using GenerateVirtualFileIdCallback =
      base::OnceCallback<void(const std::optional<std::string>& id)>;

  // Used to implement GetFileSize().
  void GetFileSizeInternal(const GURL& url_decoded,
                           GetFileSizeCallback callback);

  // Used to implement GetFileSize() and GetLastModified().
  void GetMetadata(const GURL& url_decoded,
                   storage::FileSystemOperation::GetMetadataFieldSet flags,
                   storage::FileSystemOperation::GetMetadataCallback callback);

  // Used to implement GetVirtualFileId().
  void GetVirtualFileIdInternal(const GURL& url_decoded,
                                GetVirtualFileIdCallback callback);

  // Used to implement GetVirtualFileId().
  void GenerateVirtualFileId(const GURL& url_decoded,
                             GenerateVirtualFileIdCallback callback,
                             int64_t size);

  // Used to implement GetVirtualFileId().
  void OnGenerateVirtualFileId(const GURL& url_decoded,
                               GenerateVirtualFileIdCallback callback,
                               const std::optional<std::string>& id);

  // Used to implement OpenFileToRead().
  void OpenFileById(const GURL& url_decoded,
                    OpenFileToReadCallback callback,
                    const std::optional<std::string>& id);

  // Used to implement OpenFileToRead().
  void OnOpenFileById(const GURL& url_decoded,
                      OpenFileToReadCallback callback,
                      const std::string& id,
                      base::ScopedFD fd);

  // Called from CreateMoniker() after sharing the new Moniker's path with
  // ARCVM.
  void OnShareMonikerPath(CreateMonikerCallback callback,
                          const fusebox::Moniker& moniker,
                          const base::FilePath& path,
                          bool success,
                          const std::string& failure_reason);

  // Used to implement OpenFileToRead(), needs to be testable.
  //
  // Decode a percent-encoded externalfile: URL to an absolute path on
  // the Linux VFS (virtual file system). This returns a non-empty path
  // for FUSE filesystems (ie. DriveFS, SmbFs, archives) that utilise FD
  // passing and externalfile: in file_manager::util::ConvertPathToArcUrl().
  // Returns an empty path for Chrome's virtual filesystems that are not exposed
  // on the Linux VFS (ie. MTP, FSP).
  base::FilePath GetLinuxVFSPathFromExternalFileURL(Profile* const profile,
                                                    const GURL& url);

  // Used to implement OpenFileToRead(), needs to be testable.
  //
  // Takes a path within the mount namespace of a specific FileSystemType and
  // returns the path on the Linux VFS, if it exists, or an empty path
  // otherwise.
  base::FilePath GetLinuxVFSPathForPathOnFileSystemType(
      Profile* const profile,
      const base::FilePath& path,
      storage::FileSystemType file_system_type);

  // Called when FileStreamForwarder completes read request.
  void OnReadRequestCompleted(const std::string& id,
                              std::list<FileStreamForwarderPtr>::iterator it,
                              const std::string& file_system_id,
                              bool result);

  void SetMaxNumberOfSharedMonikersForTesting(size_t value);

  const raw_ptr<Profile> profile_;
  const raw_ptr<ArcBridgeService>
      bridge_service_;  // Owned by ArcServiceManager
  base::ObserverList<Observer>::Unchecked observer_list_;

  // Map from file descriptor IDs to requested URLs.
  std::map<std::string, GURL> id_to_url_;

  // Set of Fusebox Monikers currently shared with ARC, associated with distinct
  // indices that indicate the order of creation (smaller is older).
  std::map<fusebox::Moniker, int> shared_monikers_;
  std::map<int, fusebox::Moniker> moniker_indices_;
  size_t max_number_of_shared_monikers_;

  std::list<FileStreamForwarderPtr> file_stream_forwarders_;

  std::unique_ptr<ArcSelectFilesHandlersManager> select_files_handlers_manager_;

  base::WeakPtrFactory<ArcFileSystemBridge> weak_ptr_factory_{this};
};

}  // namespace arc

#endif  // CHROME_BROWSER_ASH_ARC_FILEAPI_ARC_FILE_SYSTEM_BRIDGE_H_
