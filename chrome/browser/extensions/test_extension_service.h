// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_TEST_EXTENSION_SERVICE_H_
#define CHROME_BROWSER_EXTENSIONS_TEST_EXTENSION_SERVICE_H_

#include <string>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "chrome/browser/extensions/extension_service.h"

namespace extensions {
class CWSInfoServiceInterface;
class CrxInstaller;
class Extension;
}  // namespace extensions

// Implementation of ExtensionServiceInterface with default
// implementations for methods that add failures.  You should subclass
// this and override the methods you care about.
class TestExtensionService : public extensions::ExtensionServiceInterface {
 public:
  TestExtensionService();
  ~TestExtensionService() override;

  // ExtensionServiceInterface implementation.
  extensions::PendingExtensionManager* pending_extension_manager() override;
  extensions::CorruptedExtensionReinstaller* corrupted_extension_reinstaller()
      override;

  scoped_refptr<extensions::CrxInstaller> CreateUpdateInstaller(
      const extensions::CRXFileInfo& file,
      bool file_ownership_passed) override;
  const extensions::Extension* GetPendingExtensionUpdate(
      const std::string& extension_id) const override;
  bool FinishDelayedInstallationIfReady(const std::string& extension_id,
                                        bool install_immediately) override;
  bool IsExtensionEnabled(const std::string& extension_id) const override;

  void CheckManagementPolicy() override;
  void CheckForUpdatesSoon() override;

  void AddExtension(const extensions::Extension* extension) override;
  void AddComponentExtension(const extensions::Extension* extension) override;

  void UnloadExtension(const std::string& extension_id,
                       extensions::UnloadedExtensionReason reason) override;
  void RemoveComponentExtension(const std::string& extension_id) override;

  bool UserCanDisableInstalledExtension(
      const std::string& extension_id) override;

  void ReinstallProviderExtensions() override;

  base::WeakPtr<ExtensionServiceInterface> AsWeakPtr() override;

 private:
  std::unique_ptr<extensions::CWSInfoServiceInterface> cws_info_service_;
  base::WeakPtrFactory<TestExtensionService> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_EXTENSIONS_TEST_EXTENSION_SERVICE_H_
