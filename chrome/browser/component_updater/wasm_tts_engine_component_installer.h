// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COMPONENT_UPDATER_WASM_TTS_ENGINE_COMPONENT_INSTALLER_H_
#define CHROME_BROWSER_COMPONENT_UPDATER_WASM_TTS_ENGINE_COMPONENT_INSTALLER_H_

#include <string>
#include <vector>

#include "base/values.h"
#include "components/component_updater/component_installer.h"

namespace base {
class FilePath;
}  // namespace base

namespace component_updater {

class ComponentUpdateService;

class WasmTtsEngineComponentInstallerPolicy : public ComponentInstallerPolicy {
 public:
  WasmTtsEngineComponentInstallerPolicy() = default;
  WasmTtsEngineComponentInstallerPolicy(
      const WasmTtsEngineComponentInstallerPolicy&) = delete;
  WasmTtsEngineComponentInstallerPolicy& operator=(
      const WasmTtsEngineComponentInstallerPolicy&) = delete;

  static void GetWasmTTSEngineDirectory(
      base::OnceCallback<void(const base::FilePath&)> callback);

 private:
  // The following methods override ComponentInstallerPolicy.
  bool SupportsGroupPolicyEnabledComponentUpdates() const override;
  bool RequiresNetworkEncryption() const override;
  update_client::CrxInstaller::Result OnCustomInstall(
      const base::Value::Dict& manifest,
      const base::FilePath& install_dir) override;
  void OnCustomUninstall() override;
  bool VerifyInstallation(const base::Value::Dict& manifest,
                          const base::FilePath& install_dir) const override;
  void ComponentReady(const base::Version& version,
                      const base::FilePath& install_dir,
                      base::Value::Dict manifest) override;
  base::FilePath GetRelativeInstallDir() const override;
  void GetHash(std::vector<uint8_t>* hash) const override;
  std::string GetName() const override;
  update_client::InstallerAttributes GetInstallerAttributes() const override;
};

// Call once during startup to make the component update service aware of
// the WASM TTS Engine component.
void RegisterWasmTtsEngineComponent(ComponentUpdateService* cus);

}  // namespace component_updater

#endif  // CHROME_BROWSER_COMPONENT_UPDATER_WASM_TTS_ENGINE_COMPONENT_INSTALLER_H_
