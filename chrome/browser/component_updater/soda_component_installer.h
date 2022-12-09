// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COMPONENT_UPDATER_SODA_COMPONENT_INSTALLER_H_
#define CHROME_BROWSER_COMPONENT_UPDATER_SODA_COMPONENT_INSTALLER_H_

#include <string>

#include "base/gtest_prod_util.h"
#include "base/values.h"
#include "components/component_updater/component_installer.h"
#include "components/prefs/pref_service.h"
#include "components/soda/constants.h"
#include "components/update_client/update_client.h"

namespace component_updater {

// Success callback to be run after the component is downloaded.
using OnSodaComponentInstalledCallback =
    base::RepeatingCallback<void(const base::FilePath&)>;

using OnSodaComponentReadyCallback = base::OnceClosure;
using OnSodaLanguagePackComponentReadyCallback =
    base::OnceCallback<void(speech::LanguageCode)>;

class SodaComponentInstallerPolicy : public ComponentInstallerPolicy {
 public:
  explicit SodaComponentInstallerPolicy(
      OnSodaComponentInstalledCallback on_installed_callback,
      OnSodaComponentReadyCallback on_ready_callback);
  ~SodaComponentInstallerPolicy() override;

  SodaComponentInstallerPolicy(const SodaComponentInstallerPolicy&) = delete;
  SodaComponentInstallerPolicy& operator=(const SodaComponentInstallerPolicy&) =
      delete;

  static const std::string GetExtensionId();
  static void UpdateSodaComponentOnDemand();

  static update_client::CrxInstaller::Result SetComponentDirectoryPermission(
      const base::FilePath& install_dir);

 private:
  FRIEND_TEST_ALL_PREFIXES(SodaComponentInstallerTest,
                           ComponentReady_CallsLambda);

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

  OnSodaComponentInstalledCallback on_installed_callback_;
  OnSodaComponentReadyCallback on_ready_callback_;
};

// Call once during startup to make the component update service aware of
// the File Type Policies component. Should only be called by SodaInstaller.
void RegisterSodaComponent(ComponentUpdateService* cus,
                           PrefService* global_prefs,
                           base::OnceClosure on_ready_callback,
                           base::OnceClosure on_registered_callback);

// Should only be called by SodaInstaller.
void RegisterSodaLanguageComponent(
    ComponentUpdateService* cus,
    const std::string& language,
    PrefService* global_prefs,
    OnSodaLanguagePackComponentReadyCallback on_ready_callback);

}  // namespace component_updater

#endif  // CHROME_BROWSER_COMPONENT_UPDATER_SODA_COMPONENT_INSTALLER_H_
