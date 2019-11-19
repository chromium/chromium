// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_ARC_FILE_SYSTEM_WATCHER_ARC_FILE_SYSTEM_WATCHER_SERVICE_H_
#define CHROME_BROWSER_CHROMEOS_ARC_FILE_SYSTEM_WATCHER_ARC_FILE_SYSTEM_WATCHER_SERVICE_H_

#include <memory>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "components/arc/mojom/file_system.mojom.h"
#include "components/arc/session/connection_observer.h"
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

// Returns true if the file path has a media extension supported by Android.
bool HasAndroidSupportedMediaExtension(const base::FilePath& path);

// Appends |cros_path|'s relative path from "/media/removable" to |android_path|
// with the altered device label which is used in Android removable media paths.
// Exposed only for testing.
bool AppendRelativePathForRemovableMedia(const base::FilePath& cros_path,
                                         base::FilePath* android_path);

// Exposed only for testing.
extern const char* kAndroidSupportedMediaExtensions[];
extern const int kAndroidSupportedMediaExtensionsSize;

// Watches file system directories and registers newly created media files to
// Android MediaProvider.
class ArcFileSystemWatcherService
    : public KeyedService,
      public ConnectionObserver<mojom::FileSystemInstance> {
 public:
  // Returns singleton instance for the given BrowserContext,
  // or nullptr if the browser |context| is not allowed to use ARC.
  static ArcFileSystemWatcherService* GetForBrowserContext(
      content::BrowserContext* context);

  ArcFileSystemWatcherService(content::BrowserContext* context,
                              ArcBridgeService* bridge_service);

  ~ArcFileSystemWatcherService() override;

  // ConnectionObserver<mojom::FileSystemInstance> overrides.
  void OnConnectionReady() override;
  void OnConnectionClosed() override;

 private:
  class FileSystemWatcher;

  void StartWatchingFileSystem();
  void StopWatchingFileSystem();

  std::unique_ptr<FileSystemWatcher> CreateAndStartFileSystemWatcher(
      const base::FilePath& cros_path,
      const base::FilePath& android_path);
  void OnFileSystemChanged(const std::vector<std::string>& paths);

  content::BrowserContext* const context_;
  ArcBridgeService* const arc_bridge_service_;  // Owned by ArcServiceManager.

  std::unique_ptr<FileSystemWatcher> downloads_watcher_;
  std::unique_ptr<FileSystemWatcher> myfiles_watcher_;
  std::unique_ptr<FileSystemWatcher> removable_media_watcher_;

  scoped_refptr<base::SequencedTaskRunner> file_task_runner_;

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate the weak pointers before any other members are destroyed.
  base::WeakPtrFactory<ArcFileSystemWatcherService> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(ArcFileSystemWatcherService);
};

}  // namespace arc

#endif  // CHROME_BROWSER_CHROMEOS_ARC_FILE_SYSTEM_WATCHER_ARC_FILE_SYSTEM_WATCHER_SERVICE_H_
