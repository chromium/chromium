// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COMPONENT_UPDATER_TRANSLATE_KIT_COMPONENT_INSTALLER_H_
#define CHROME_BROWSER_COMPONENT_UPDATER_TRANSLATE_KIT_COMPONENT_INSTALLER_H_

#include <string>

#include "base/gtest_prod_util.h"
#include "base/values.h"
#include "components/component_updater/component_installer.h"
#include "components/prefs/pref_service.h"
#include "components/update_client/update_client.h"

namespace component_updater {

// The installer policy for the TranslateKit Component.
class TranslateKitComponentInstallerPolicy : public ComponentInstallerPolicy {
 public:
  explicit TranslateKitComponentInstallerPolicy(PrefService* pref_service);
  ~TranslateKitComponentInstallerPolicy() override;

  // Not Copyable.
  TranslateKitComponentInstallerPolicy(
      const TranslateKitComponentInstallerPolicy&) = delete;
  TranslateKitComponentInstallerPolicy& operator=(
      const TranslateKitComponentInstallerPolicy&) = delete;

 private:
  FRIEND_TEST_ALL_PREFIXES(RegisterTranslateKitComponentTest,
                           VerifyInstallationDefaultEmpty);

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

  raw_ptr<PrefService> pref_service_;
};

// Call once during startup to make the component update service aware of
// the TranslateKit component.
void RegisterTranslateKitComponent(ComponentUpdateService* cus,
                                   PrefService* pref_service);

}  // namespace component_updater

#endif  // CHROME_BROWSER_COMPONENT_UPDATER_TRANSLATE_KIT_COMPONENT_INSTALLER_H_
