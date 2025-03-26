// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_UPDATER_EXTENSION_UPDATER_DELEGATE_H_
#define CHROME_BROWSER_EXTENSIONS_UPDATER_EXTENSION_UPDATER_DELEGATE_H_

#include "base/memory/scoped_refptr.h"

namespace extensions {

struct CRXFileInfo;
class CrxInstaller;

// Delegate for ExtensionUpdater. Exists to break circular dependencies with
// ExtensionService. Has its own header file to avoid increasing the number of
// transitive includes from extension_service.h.
class ExtensionUpdaterDelegate {
 public:
  // Creates an CrxInstaller to update an extension. Returns null if an update
  // is not possible. Eg: system shutdown or extension doesn't exist.
  virtual scoped_refptr<CrxInstaller> CreateUpdateInstaller(
      const CRXFileInfo& file,
      bool file_ownership_passed) = 0;

  virtual ~ExtensionUpdaterDelegate() = default;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_UPDATER_EXTENSION_UPDATER_DELEGATE_H_
