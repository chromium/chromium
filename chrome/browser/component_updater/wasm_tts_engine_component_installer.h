// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COMPONENT_UPDATER_WASM_TTS_ENGINE_COMPONENT_INSTALLER_H_
#define CHROME_BROWSER_COMPONENT_UPDATER_WASM_TTS_ENGINE_COMPONENT_INSTALLER_H_

#include <string>
#include <vector>

#include "base/time/time.h"
#include "base/values.h"
#include "components/component_updater/component_installer.h"
#include "components/prefs/pref_service.h"

namespace base {
class FilePath;
}  // namespace base

namespace component_updater {

class ComponentUpdateService;

class WasmTtsEngineComponentInstallerPolicy : public ComponentInstallerPolicy {
 public:
  explicit WasmTtsEngineComponentInstallerPolicy(PrefService* prefs);
  WasmTtsEngineComponentInstallerPolicy(
      const WasmTtsEngineComponentInstallerPolicy&) = delete;
  WasmTtsEngineComponentInstallerPolicy& operator=(
      const WasmTtsEngineComponentInstallerPolicy&) = delete;

  static void RegisterPrefs(PrefRegistrySimple* registry);
  static void GetWasmTTSEngineDirectory(
      base::OnceCallback<void(const base::FilePath&)> callback);
  static bool IsWasmTTSEngineDirectorySet();

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
  void MaybeReinstallTtsEngine(const base::FilePath& install_dir);
  base::FilePath GetRelativeInstallDir() const override;
  void GetHash(std::vector<uint8_t>* hash) const override;
  base::Time GetTimeSinceLastOpened(char pref);
  std::string GetName() const override;
  update_client::InstallerAttributes GetInstallerAttributes() const override;

  // Thresholds used to keep track of how long it has been since reading mode
  // was last opened. The TTS engine should be reinstalled to remove never
  // used voices after kThresholdRecent time has passed without reading mode
  // being opened and reinstalled to removed unused voices after
  // kThresholdLonger time has passed without reading mode being opened.
  const base::TimeDelta kThresholdRecent = base::Days(14);
  const base::TimeDelta kThresholdLonger = base::Days(90);

  raw_ptr<PrefService, DanglingUntriaged> pref_service_;
};

// Call once during startup to make the component update service aware of
// the WASM TTS Engine component.
void RegisterWasmTtsEngineComponent(ComponentUpdateService* cus,
                                    PrefService* pref);

}  // namespace component_updater

#endif  // CHROME_BROWSER_COMPONENT_UPDATER_WASM_TTS_ENGINE_COMPONENT_INSTALLER_H_
