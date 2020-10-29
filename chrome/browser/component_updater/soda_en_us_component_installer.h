// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COMPONENT_UPDATER_SODA_EN_US_COMPONENT_INSTALLER_H_
#define CHROME_BROWSER_COMPONENT_UPDATER_SODA_EN_US_COMPONENT_INSTALLER_H_

#include <string>

#include "components/component_updater/component_installer.h"
#include "components/prefs/pref_service.h"
#include "components/update_client/update_client.h"

namespace component_updater {

// Success callback to be run after the component is downloaded.
using OnSodaEnUsComponentReadyCallback =
    base::RepeatingCallback<void(const base::FilePath&)>;

class SodaEnUsComponentInstallerPolicy : public ComponentInstallerPolicy {
 public:
  explicit SodaEnUsComponentInstallerPolicy(
      OnSodaEnUsComponentReadyCallback callback);
  ~SodaEnUsComponentInstallerPolicy() override;

  SodaEnUsComponentInstallerPolicy(const SodaEnUsComponentInstallerPolicy&) =
      delete;
  SodaEnUsComponentInstallerPolicy& operator=(
      const SodaEnUsComponentInstallerPolicy&) = delete;

  static const std::string GetExtensionId();
  static void UpdateSodaEnUsComponentOnDemand();

 private:
  FRIEND_TEST_ALL_PREFIXES(SodaEnUsComponentInstallerTest,
                           ComponentReady_CallsLambda);

  // The following methods override ComponentInstallerPolicy.
  bool SupportsGroupPolicyEnabledComponentUpdates() const override;
  bool RequiresNetworkEncryption() const override;
  update_client::CrxInstaller::Result OnCustomInstall(
      const base::DictionaryValue& manifest,
      const base::FilePath& install_dir) override;
  void OnCustomUninstall() override;
  bool VerifyInstallation(const base::DictionaryValue& manifest,
                          const base::FilePath& install_dir) const override;
  void ComponentReady(const base::Version& version,
                      const base::FilePath& install_dir,
                      std::unique_ptr<base::DictionaryValue> manifest) override;
  base::FilePath GetRelativeInstallDir() const override;
  void GetHash(std::vector<uint8_t>* hash) const override;
  std::string GetName() const override;
  update_client::InstallerAttributes GetInstallerAttributes() const override;
  std::vector<std::string> GetMimeTypes() const override;

  OnSodaEnUsComponentReadyCallback on_component_ready_callback_;
};

// Call once during startup to make the component update service aware of
// the File Type Policies component.
void RegisterSodaEnUsComponent(ComponentUpdateService* cus,
                               PrefService* prefs,
                               base::OnceClosure callback);

}  // namespace component_updater

#endif  // CHROME_BROWSER_COMPONENT_UPDATER_SODA_EN_US_COMPONENT_INSTALLER_H_
