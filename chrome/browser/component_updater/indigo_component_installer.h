// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COMPONENT_UPDATER_INDIGO_COMPONENT_INSTALLER_H_
#define CHROME_BROWSER_COMPONENT_UPDATER_INDIGO_COMPONENT_INSTALLER_H_

#include <stdint.h>

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/callback_list.h"
#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/values.h"
#include "base/version.h"
#include "components/component_updater/component_installer.h"

namespace component_updater {

class ComponentUpdateService;

class IndigoComponentInstallerPolicy : public ComponentInstallerPolicy {
 public:
  IndigoComponentInstallerPolicy();
  ~IndigoComponentInstallerPolicy() override;

  IndigoComponentInstallerPolicy(const IndigoComponentInstallerPolicy&) =
      delete;
  IndigoComponentInstallerPolicy& operator=(
      const IndigoComponentInstallerPolicy&) = delete;

  // ComponentInstallerPolicy implementation.
  bool VerifyInstallation(const base::DictValue& manifest,
                          const base::FilePath& install_dir) const override;
  void ComponentReady(const base::Version& version,
                      const base::FilePath& install_dir,
                      base::DictValue manifest) override;
  update_client::InstallerAttributes GetInstallerAttributes() const override;

 private:
  // ComponentInstallerPolicy implementation.
  bool SupportsGroupPolicyEnabledComponentUpdates() const override;
  bool RequiresNetworkEncryption() const override;
  update_client::CrxInstaller::Result OnCustomInstall(
      const base::DictValue& manifest,
      const base::FilePath& install_dir) override;
  void OnCustomUninstall() override;
  base::FilePath GetRelativeInstallDir() const override;
  void GetHash(std::vector<uint8_t>* hash) const override;
  std::string GetName() const override;
};

// Registers a callback to be notified when the Indigo component becomes ready
// (i.e. installed or updated).
// Must be called on the Browser UI thread, and the callback will in turn
// be invoked on the Browser UI thread.
base::CallbackListSubscription RegisterIndigoComponentReadyCallback(
    base::RepeatingClosure callback);

// Call once during startup to make the component update service aware of
// the Indigo component.
void RegisterIndigoComponent(ComponentUpdateService* cus);

// Returns the installation directory for the Indigo component. Must be called
// on UI thread.
std::optional<base::FilePath> GetIndigoComponentInstallDir();

// Returns the path to the content_script.js file. Must be called on UI thread.
std::optional<base::FilePath> GetIndigoContentScriptPath();

// For testing only. Resets the stored installation directory.
void ResetIndigoInstallDirForTesting();

}  // namespace component_updater

#endif  // CHROME_BROWSER_COMPONENT_UPDATER_INDIGO_COMPONENT_INSTALLER_H_
