// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_SERVICE_TEST_WITH_INSTALL_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_SERVICE_TEST_WITH_INSTALL_H_

#include <stddef.h>

#include <string>

#include "base/macros.h"
#include "base/scoped_observer.h"
#include "chrome/browser/extensions/extension_service_test_base.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_registry_observer.h"
#include "extensions/common/extension.h"
#include "extensions/common/feature_switch.h"

namespace base {
class FilePath;
}

namespace extensions {

// An enhancement of ExtensionServiceTestBase that provides helpers to install,
// update, and uninstall extensions.
class ExtensionServiceTestWithInstall : public ExtensionServiceTestBase,
                                        public ExtensionRegistryObserver {
 public:
  ExtensionServiceTestWithInstall();
  ~ExtensionServiceTestWithInstall() override;

 protected:
  void InitializeExtensionService(
      const ExtensionServiceInitParams& params) override;

  static std::vector<base::string16> GetErrors();

  void PackCRX(const base::FilePath& dir_path,
               const base::FilePath& pem_path,
               const base::FilePath& crx_path);

  enum InstallState {
    INSTALL_FAILED,
    INSTALL_UPDATED,
    INSTALL_NEW,
    INSTALL_WITHOUT_LOAD,
  };

  const Extension* PackAndInstallCRX(const base::FilePath& dir_path,
                                     const base::FilePath& pem_path,
                                     InstallState install_state,
                                     int creation_flags,
                                     Manifest::Location install_location);
  const Extension* PackAndInstallCRX(const base::FilePath& dir_path,
                                     const base::FilePath& pem_path,
                                     InstallState install_state);
  const Extension* PackAndInstallCRX(const base::FilePath& dir_path,
                                     InstallState install_state);
  const Extension* PackAndInstallCRX(const base::FilePath& dir_path,
                                     Manifest::Location install_location,
                                     InstallState install_state);
  const Extension* InstallCRX(const base::FilePath& path,
                              InstallState install_state,
                              int creation_flags,
                              const std::string& expected_old_name);
  const Extension* InstallCRX(const base::FilePath& path,
                              Manifest::Location install_location,
                              InstallState install_state,
                              int creation_flags);
  const Extension* InstallCRX(const base::FilePath& path,
                              InstallState install_state,
                              int creation_flags);
  const Extension* InstallCRX(const base::FilePath& path,
                              InstallState install_state);
  const Extension* InstallCRXFromWebStore(const base::FilePath& path,
                                          InstallState install_state);

  // Verifies the result of a CRX installation. Used by InstallCRX. Set the
  // |install_state| to INSTALL_FAILED if the installation is expected to fail.
  // Returns an Extension pointer if the install succeeded, null otherwise.
  const Extension* VerifyCrxInstall(const base::FilePath& path,
                                    InstallState install_state);

  // Verifies the result of a CRX installation. Used by InstallCRX. Set the
  // |install_state| to INSTALL_FAILED if the installation is expected to fail.
  // If |install_state| is INSTALL_UPDATED, and |expected_old_name| is
  // non-empty, expects that the existing extension's title was
  // |expected_old_name|.
  // Returns an Extension pointer if the install succeeded, null otherwise.
  const Extension* VerifyCrxInstall(const base::FilePath& path,
                                    InstallState install_state,
                                    const std::string& expected_old_name);

  enum UpdateState {
    FAILED_SILENTLY,
    FAILED,
    UPDATED,
    INSTALLED,
    DISABLED,
    ENABLED
  };

  void PackCRXAndUpdateExtension(const std::string& id,
                                 const base::FilePath& dir_path,
                                 const base::FilePath& pem_path,
                                 UpdateState expected_state);

  void UpdateExtension(const std::string& id,
                       const base::FilePath& in_path,
                       UpdateState expected_state);

  void UninstallExtension(const std::string& id);

  void TerminateExtension(const std::string& id);

  // ExtensionRegistryObserver:
  void OnExtensionLoaded(content::BrowserContext* browser_context,
                         const Extension* extension) override;
  void OnExtensionUnloaded(content::BrowserContext* browser_context,
                           const Extension* extension,
                           UnloadedExtensionReason reason) override;
  void OnExtensionWillBeInstalled(content::BrowserContext* browser_context,
                                  const Extension* extension,
                                  bool is_update,
                                  const std::string& old_name) override;

  // TODO(treib,devlin): Make these private and add accessors as needed.
  extensions::ExtensionList loaded_;
  const Extension* installed_;
  bool was_update_;
  std::string old_name_;
  std::string unloaded_id_;
  UnloadedExtensionReason unloaded_reason_;

 private:
  void InstallCRXInternal(const base::FilePath& crx_path,
                          Manifest::Location install_location,
                          InstallState install_state,
                          int creation_flags);

  size_t expected_extensions_count_;

  FeatureSwitch::ScopedOverride override_external_install_prompt_;

  ScopedObserver<ExtensionRegistry, ExtensionRegistryObserver>
      registry_observer_{this};

  DISALLOW_COPY_AND_ASSIGN(ExtensionServiceTestWithInstall);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_SERVICE_TEST_WITH_INSTALL_H_
