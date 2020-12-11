// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_GARBAGE_COLLECTOR_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_GARBAGE_COLLECTOR_H_

#include <map>
#include <string>

#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/extensions/install_gate.h"
#include "chrome/browser/extensions/install_observer.h"
#include "components/keyed_service/core/keyed_service.h"

namespace content {
class BrowserContext;
}

namespace extensions {

// The class responsible for cleaning up the cruft left behind on the file
// system by uninstalled (or failed install) extensions.
// The class is owned by ExtensionService, but is mostly independent. Tasks to
// garbage collect extensions and isolated storage are posted once the
// ExtensionSystem signals ready.
class ExtensionGarbageCollector : public KeyedService,
                                  public InstallObserver,
                                  public InstallGate {
 public:
  explicit ExtensionGarbageCollector(content::BrowserContext* context);
  ~ExtensionGarbageCollector() override;

  static ExtensionGarbageCollector* Get(content::BrowserContext* context);

  // Manually trigger GarbageCollectExtensions() for testing.
  void GarbageCollectExtensionsForTest();

  // KeyedService:
  void Shutdown() override;

  // InstallObserver:
  void OnBeginCrxInstall(const std::string& extension_id) override;
  void OnFinishCrxInstall(const std::string& extension_id,
                          bool success) override;

  // InstallGate:
  Action ShouldDelay(const Extension* extension,
                     bool install_immediately) override;

 protected:
  // Cleans up the extension install directory. It can end up with garbage in it
  // if extensions can't initially be removed when they are uninstalled (eg if a
  // file is in use).
  // Obsolete version directories are removed, as are directories that aren't
  // found in the ExtensionPrefs.
  // The "Temp" directory that is used during extension installation will get
  // removed iff there are no pending installations.
  virtual void GarbageCollectExtensions();

  // Garbage collects apps/extensions isolated storage if it is uninstalled.
  // There is an exception for ephemeral apps because they can outlive their
  // cache lifetimes.
  void GarbageCollectIsolatedStorageIfNeeded();

  // Restart any extension installs which were delayed for isolated storage
  // garbage collection.
  void OnGarbageCollectIsolatedStorageFinished();

  static void GarbageCollectExtensionsOnFileThread(
      const base::FilePath& install_directory,
      const std::multimap<std::string, base::FilePath>& extension_paths);

  // The BrowserContext associated with the GarbageCollector.
  content::BrowserContext* context_;

  // The number of currently ongoing CRX installations. This is used to prevent
  // garbage collection from running while a CRX is being installed.
  int crx_installs_in_progress_;

  // Set to true to delay all new extension installations. Acts as a lock to
  // allow background processing of garbage collection of on-disk state without
  // needing to worry about race conditions caused by extension installation and
  // reinstallation.
  bool installs_delayed_for_gc_ = false;

  // Generate weak pointers for safely posting to the file thread for garbage
  // collection.
  base::WeakPtrFactory<ExtensionGarbageCollector> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(ExtensionGarbageCollector);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_GARBAGE_COLLECTOR_H_
