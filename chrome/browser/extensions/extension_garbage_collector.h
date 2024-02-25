// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_GARBAGE_COLLECTOR_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_GARBAGE_COLLECTOR_H_

#include <map>
#include <string>

#include "base/files/file_path.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/extensions/install_observer.h"
#include "components/keyed_service/core/keyed_service.h"
#include "extensions/common/extension_id.h"

namespace content {
class BrowserContext;
}

namespace extensions {

// The class responsible for cleaning up the cruft left behind on the file
// system by uninstalled (or failed install) extensions.
// The class is owned by ExtensionService, but is mostly independent. Tasks to
// garbage collect extensions and isolated storage are posted once the
// ExtensionSystem signals ready.
class ExtensionGarbageCollector : public KeyedService, public InstallObserver {
 public:
  explicit ExtensionGarbageCollector(content::BrowserContext* context);

  ExtensionGarbageCollector(const ExtensionGarbageCollector&) = delete;
  ExtensionGarbageCollector& operator=(const ExtensionGarbageCollector&) =
      delete;

  ~ExtensionGarbageCollector() override;

  static ExtensionGarbageCollector* Get(content::BrowserContext* context);

  // Manually trigger GarbageCollectExtensions() for testing.
  void GarbageCollectExtensionsForTest();

  // KeyedService:
  void Shutdown() override;

  // InstallObserver:
  void OnBeginCrxInstall(content::BrowserContext* context,
                         const CrxInstaller& installer,
                         const ExtensionId& extension_id) override;
  void OnFinishCrxInstall(content::BrowserContext* context,
                          const CrxInstaller& installer,
                          const ExtensionId& extension_id,
                          bool success) override;

 protected:
  // Cleans up the extension install directory. It can end up with garbage in it
  // if extensions can't initially be removed when they are uninstalled (eg if a
  // file is in use).
  // Obsolete version directories are removed, as are directories that aren't
  // found in the ExtensionPrefs.
  // The "Temp" directory that is used during extension installation will get
  // removed iff there are no pending installations.
  virtual void GarbageCollectExtensions();

  static void GarbageCollectExtensionsOnFileThread(
      const base::FilePath& install_directory,
      const std::multimap<ExtensionId, base::FilePath>& extension_paths,
      bool unpacked);

  // The BrowserContext associated with the GarbageCollector.
  raw_ptr<content::BrowserContext> context_;

  // The number of currently ongoing CRX installations. This is used to prevent
  // garbage collection from running while a CRX is being installed.
  int crx_installs_in_progress_;

  // Generate weak pointers for safely posting to the file thread for garbage
  // collection.
  base::WeakPtrFactory<ExtensionGarbageCollector> weak_factory_{this};
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_GARBAGE_COLLECTOR_H_
