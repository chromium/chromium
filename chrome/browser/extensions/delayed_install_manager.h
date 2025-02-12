// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_DELAYED_INSTALL_MANAGER_H_
#define CHROME_BROWSER_EXTENSIONS_DELAYED_INSTALL_MANAGER_H_

#include "base/memory/raw_ptr.h"
#include "extensions/common/extension_id.h"
#include "extensions/common/extension_set.h"

namespace extensions {
class ExtensionPrefs;
class ExtensionRegistrar;
class ExtensionService;

// Manages a set of extension installs delayed for various reasons.  The reason
// for delayed install is stored in ExtensionPrefs. These are not part of
// ExtensionRegistry because they are not yet installed.
class DelayedInstallManager {
 public:
  DelayedInstallManager(ExtensionService* extension_service,
                        ExtensionPrefs* extension_prefs,
                        ExtensionRegistrar* extension_registrar);
  DelayedInstallManager(const DelayedInstallManager&) = delete;
  DelayedInstallManager& operator=(const DelayedInstallManager&) = delete;
  ~DelayedInstallManager();

  // Avoids dangling pointers during keyed service two-phase shutdown.
  void Shutdown();

  // Returns true if an extension is in the delayed install set.
  bool Contains(const ExtensionId& id) const;

  // Adds an extension to the delayed install set.
  void Insert(scoped_refptr<const Extension> extension);

  // Removes an extension from the delayed install set.
  void Remove(const ExtensionId& id);

  // Returns an update for an extension with the specified id, if installation
  // of that update was previously delayed because the extension was in use. If
  // no updates are pending for the extension returns null.
  const Extension* GetPendingExtensionUpdate(const ExtensionId& id) const;

  // Checks for delayed installation for all pending installs.
  void MaybeFinishDelayedInstallations();

  // Attempts finishing installation of an update for an extension with the
  // specified id, when installation of that extension was previously delayed.
  // `install_immediately` - Whether the extension should be installed if it's
  // currently in use.
  // Returns whether the extension installation was finished.
  bool FinishDelayedInstallationIfReady(const ExtensionId& extension_id,
                                        bool install_immediately);

  const ExtensionSet& delayed_installs() const { return delayed_installs_; }

 private:
  raw_ptr<ExtensionService> extension_service_;
  raw_ptr<ExtensionPrefs> extension_prefs_;
  raw_ptr<ExtensionRegistrar> extension_registrar_;

  ExtensionSet delayed_installs_;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_DELAYED_INSTALL_MANAGER_H_
