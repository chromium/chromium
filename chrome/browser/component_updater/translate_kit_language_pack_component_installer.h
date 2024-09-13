// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COMPONENT_UPDATER_TRANSLATE_KIT_LANGUAGE_PACK_COMPONENT_INSTALLER_H_
#define CHROME_BROWSER_COMPONENT_UPDATER_TRANSLATE_KIT_LANGUAGE_PACK_COMPONENT_INSTALLER_H_

#include <string>

#include "base/memory/raw_ref.h"
#include "base/values.h"
#include "components/component_updater/component_installer.h"
#include "components/prefs/pref_service.h"
#include "components/update_client/update_client.h"

namespace on_device_translation {
enum class LanguagePackKey;
struct LanguagePackComponentConfig;
}  // namespace on_device_translation

namespace component_updater {

// The installer policy for the TranslateKit language package component.
class TranslateKitLanguagePackComponentInstallerPolicy
    : public ComponentInstallerPolicy {
 public:
  TranslateKitLanguagePackComponentInstallerPolicy(
      PrefService* pref_service,
      on_device_translation::LanguagePackKey language_pack_key);
  ~TranslateKitLanguagePackComponentInstallerPolicy() override;

  // Not Copyable.
  TranslateKitLanguagePackComponentInstallerPolicy(
      const TranslateKitLanguagePackComponentInstallerPolicy&) = delete;
  TranslateKitLanguagePackComponentInstallerPolicy& operator=(
      const TranslateKitLanguagePackComponentInstallerPolicy&) = delete;

  // `ComponentInstallerPolicy` overrides:
  bool VerifyInstallation(const base::Value::Dict& manifest,
                          const base::FilePath& install_dir) const override;
  bool SupportsGroupPolicyEnabledComponentUpdates() const override;
  bool RequiresNetworkEncryption() const override;
  update_client::CrxInstaller::Result OnCustomInstall(
      const base::Value::Dict& manifest,
      const base::FilePath& install_dir) override;
  void OnCustomUninstall() override;
  void ComponentReady(const base::Version& version,
                      const base::FilePath& install_dir,
                      base::Value::Dict manifest) override;
  base::FilePath GetRelativeInstallDir() const override;
  void GetHash(std::vector<uint8_t>* hash) const override;
  std::string GetName() const override;
  update_client::InstallerAttributes GetInstallerAttributes() const override;

 private:
  const on_device_translation::LanguagePackComponentConfig& GetConfig() const;

  const on_device_translation::LanguagePackKey language_pack_key_;
  raw_ptr<PrefService> pref_service_;
};

void RegisterTranslateKitLanguagePackComponent(
    ComponentUpdateService* cus,
    PrefService* pref_service,
    on_device_translation::LanguagePackKey language_pack_key,
    base::OnceClosure registered_callback);

}  // namespace component_updater

#endif  // CHROME_BROWSER_COMPONENT_UPDATER_TRANSLATE_KIT_LANGUAGE_PACK_COMPONENT_INSTALLER_H_
