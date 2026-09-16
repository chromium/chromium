// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COMPONENT_UPDATER_DICTATION_CONNECTOR_COMPONENT_INSTALLER_H_
#define CHROME_BROWSER_COMPONENT_UPDATER_DICTATION_CONNECTOR_COMPONENT_INSTALLER_H_

#include <stdint.h>

#include <string>
#include <vector>

#include "base/callback_list.h"
#include "base/functional/callback.h"
#include "components/component_updater/component_installer.h"

namespace base {
class FilePath;
}  // namespace base

namespace component_updater {

class ComponentUpdateService;

// Installer for the "connector" component extension used by
// chrome/browser/dictation
class DictationConnectorComponentInstallerPolicy
    : public ComponentInstallerPolicy {
 public:
  DictationConnectorComponentInstallerPolicy();
  DictationConnectorComponentInstallerPolicy(
      const DictationConnectorComponentInstallerPolicy&) = delete;
  DictationConnectorComponentInstallerPolicy& operator=(
      const DictationConnectorComponentInstallerPolicy&) = delete;
  ~DictationConnectorComponentInstallerPolicy() override = default;

  // Returns the directory in which the dictation connector extension component
  // has been downloaded to. Since the extension may not yet have been
  // downloaded, the FilePath is returned via a callback. The callback is fired
  // async even if the directory is already available. Must be called from the
  // UI thread.
  static base::CallbackListSubscription GetExtensionDirectory(
      base::OnceCallback<void(const base::FilePath&)> callback);

  // For testing only. Resets the stored installation directory.
  static void ResetForTesting();

  // ComponentInstallerPolicy interface
  bool SupportsGroupPolicyEnabledComponentUpdates() const override;
  bool RequiresNetworkEncryption() const override;
  update_client::CrxInstaller::Result OnCustomInstall(
      const base::DictValue& manifest,
      const base::FilePath& install_dir) override;
  void OnCustomUninstall() override;
  bool VerifyInstallation(const base::DictValue& manifest,
                          const base::FilePath& install_dir) const override;
  void ComponentReady(const base::Version& version,
                      const base::FilePath& install_dir,
                      base::DictValue manifest) override;
  base::FilePath GetRelativeInstallDir() const override;
  void GetHash(std::vector<uint8_t>* hash) const override;
  std::string GetName() const override;
  update_client::InstallerAttributes GetInstallerAttributes() const override;
};

void RegisterDictationConnectorComponent(ComponentUpdateService* cus);

}  // namespace component_updater

#endif  // CHROME_BROWSER_COMPONENT_UPDATER_DICTATION_CONNECTOR_COMPONENT_INSTALLER_H_
