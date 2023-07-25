// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COMPONENT_UPDATER_SODA_LANGUAGE_PACK_COMPONENT_INSTALLER_H_
#define CHROME_BROWSER_COMPONENT_UPDATER_SODA_LANGUAGE_PACK_COMPONENT_INSTALLER_H_

#include <string>
#include <vector>

#include "base/containers/flat_set.h"
#include "base/functional/callback.h"
#include "base/gtest_prod_util.h"
#include "base/values.h"
#include "chrome/browser/component_updater/soda_component_installer.h"
#include "components/component_updater/component_installer.h"
#include "components/soda/constants.h"
#include "components/update_client/update_client.h"

class PrefService;

namespace base {
class FilePath;
}  // namespace base

namespace component_updater {

class SodaLanguagePackComponentInstallerPolicy
    : public ComponentInstallerPolicy {
 public:
  SodaLanguagePackComponentInstallerPolicy(
      speech::SodaLanguagePackComponentConfig language_config,
      PrefService* prefs,
      OnSodaLanguagePackComponentReadyCallback on_ready_callback);
  ~SodaLanguagePackComponentInstallerPolicy() override;

  SodaLanguagePackComponentInstallerPolicy(
      const SodaLanguagePackComponentInstallerPolicy&) = delete;
  SodaLanguagePackComponentInstallerPolicy& operator=(
      const SodaLanguagePackComponentInstallerPolicy&) = delete;

  static std::string GetExtensionId(speech::LanguageCode language_code);
  static base::flat_set<std::string> GetExtensionIds();
  static void UpdateSodaLanguagePackComponentOnDemand(
      speech::LanguageCode language_code);

 private:
  FRIEND_TEST_ALL_PREFIXES(SodaLanguagePackComponentInstallerTest,
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

  speech::SodaLanguagePackComponentConfig language_config_;

  raw_ptr<PrefService> prefs_;
  OnSodaLanguagePackComponentReadyCallback on_ready_callback_;
};

void RegisterSodaLanguagePackComponent(
    speech::SodaLanguagePackComponentConfig language_config,
    ComponentUpdateService* cus,
    PrefService* prefs,
    OnSodaLanguagePackComponentReadyCallback on_ready_callback);

}  // namespace component_updater

#endif  // CHROME_BROWSER_COMPONENT_UPDATER_SODA_LANGUAGE_PACK_COMPONENT_INSTALLER_H_
