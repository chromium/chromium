// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ARC_FILE_SYSTEM_WATCHER_ARC_FILE_SYSTEM_WATCHER_SERVICE_H_
#define CHROME_BROWSER_ASH_ARC_FILE_SYSTEM_WATCHER_ARC_FILE_SYSTEM_WATCHER_SERVICE_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "ash/components/arc/mojom/file_system.mojom-forward.h"
#include "ash/components/arc/session/connection_observer.h"
#include "ash/components/arc/volume_mounter/arc_volume_mounter_bridge.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "components/keyed_service/core/keyed_service.h"

namespace base {
class FilePath;
class SequencedTaskRunner;
}  // namespace base

namespace content {
class BrowserContext;
}  // namespace content

namespace arc {

class ArcBridgeService;

// Watches file system directories and registers newly created media files to
// Android MediaProvider.
class ArcFileSystemWatcherService
    : public KeyedService,
      public ConnectionObserver<mojom::FileSystemInstance>,
      public ArcVolumeMounterBridge::Delegate {
 public:
  // Returns singleton instance for the given BrowserContext,
  // or nullptr if the browser |context| is not allowed to use ARC.
  static ArcFileSystemWatcherService* GetForBrowserContext(
      content::BrowserContext* context);

  ArcFileSystemWatcherService(content::BrowserContext* context,
                              ArcBridgeService* bridge_service);

  ArcFileSystemWatcherService(const ArcFileSystemWatcherService&) = delete;
  ArcFileSystemWatcherService& operator=(const ArcFileSystemWatcherService&) =
      delete;

  ~ArcFileSystemWatcherService() override;

  // ConnectionObserver<mojom::FileSystemInstance> overrides.
  void OnConnectionReady() override;
  void OnConnectionClosed() override;

  // ArcVolumeMounterBridge::Delegate overrides.
  bool IsWatchingFileSystemChanges() override;
  void StartWatchingRemovableMedia(const std::string& fs_uuid,
                                   const std::string& mount_path,
                                   base::OnceClosure callback) override;
  void StopWatchingRemovableMedia(const std::string& mount_path) override;

  static void EnsureFactoryBuilt();

 private:
  class FileSystemWatcher;

  void StartWatchingFileSystem();
  void StopWatchingFileSystem();

  void OnMyFilesWatcherStarted();

  void TriggerSendAllMountEvents() const;

  std::unique_ptr<FileSystemWatcher> CreateAndStartFileSystemWatcher(
      const base::FilePath& cros_path,
      const base::FilePath& android_path,
      base::OnceClosure callback);
  void OnFileSystemChanged(const std::vector<std::string>& paths);

  const raw_ptr<content::BrowserContext> context_;
  const raw_ptr<ArcBridgeService>
      arc_bridge_service_;  // Owned by ArcServiceManager.

  bool watching_file_system_changes_ = false;

  std::unique_ptr<FileSystemWatcher> myfiles_watcher_;
  // A map from mount path to watcher.
  std::map<std::string, std::unique_ptr<FileSystemWatcher>>
      removable_media_watchers_;

  scoped_refptr<base::SequencedTaskRunner> file_task_runner_;

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate the weak pointers before any other members are destroyed.
  base::WeakPtrFactory<ArcFileSystemWatcherService> weak_ptr_factory_{this};
};

}  // namespace arc

#endif  // CHROME_BROWSER_ASH_ARC_FILE_SYSTEM_WATCHER_ARC_FILE_SYSTEM_WATCHER_SERVICE_H_
