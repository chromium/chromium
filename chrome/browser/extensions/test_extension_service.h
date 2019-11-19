// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_TEST_EXTENSION_SERVICE_H_
#define CHROME_BROWSER_EXTENSIONS_TEST_EXTENSION_SERVICE_H_

#include <string>
#include <vector>

#include "chrome/browser/extensions/extension_service.h"

namespace extensions {
class CrxInstaller;
class Extension;
}

// Implemention of ExtensionServiceInterface with default
// implementations for methods that add failures.  You should subclass
// this and override the methods you care about.
class TestExtensionService : public extensions::ExtensionServiceInterface {
 public:
  ~TestExtensionService() override;

  // ExtensionServiceInterface implementation.
  extensions::PendingExtensionManager* pending_extension_manager() override;

  bool UpdateExtension(const extensions::CRXFileInfo& file,
                       bool file_ownership_passed,
                       extensions::CrxInstaller** out_crx_installer) override;
  const extensions::Extension* GetPendingExtensionUpdate(
      const std::string& extension_id) const override;
  bool FinishDelayedInstallationIfReady(const std::string& extension_id,
                                        bool install_immediately) override;
  bool IsExtensionEnabled(const std::string& extension_id) const override;

  void CheckManagementPolicy() override;
  void CheckForUpdatesSoon() override;

  bool is_ready() override;

  void AddExtension(const extensions::Extension* extension) override;
  void AddComponentExtension(const extensions::Extension* extension) override;

  void UnloadExtension(const std::string& extension_id,
                       extensions::UnloadedExtensionReason reason) override;
  void RemoveComponentExtension(const std::string& extension_id) override;
};

#endif  // CHROME_BROWSER_EXTENSIONS_TEST_EXTENSION_SERVICE_H_
