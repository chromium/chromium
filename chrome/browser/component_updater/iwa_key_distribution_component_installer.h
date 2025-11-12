// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COMPONENT_UPDATER_IWA_KEY_DISTRIBUTION_COMPONENT_INSTALLER_H_
#define CHROME_BROWSER_COMPONENT_UPDATER_IWA_KEY_DISTRIBUTION_COMPONENT_INSTALLER_H_

#include <cstdint>
#include <string>
#include <vector>

#include "base/feature_list.h"
#include "base/functional/callback.h"
#include "base/types/pass_key.h"
#include "base/values.h"
#include "components/component_updater/component_installer.h"
#include "components/update_client/update_client.h"

namespace base {
class FilePath;
class Version;
}  // namespace base

namespace web_app {
class IwaKeyDistributionInfoProvider;
}  // namespace web_app

namespace component_updater {

#if BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
BASE_DECLARE_FEATURE(kIwaKeyDistributionComponent);
#endif  // BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)

inline constexpr char kIwaKeyDistributionComponentExpCohort[] =
    "iwa-kd-component-exp-cohort";

class ComponentUpdateService;

class IwaKeyDistributionComponentInstallerPolicy
    : public ComponentInstallerPolicy {
 public:
  static constexpr base::FilePath::CharType kRelativeInstallDirName[] =
      FILE_PATH_LITERAL("IwaKeyDistribution");
  static constexpr char kManifestName[] = "Iwa Key Distribution";
  static constexpr base::FilePath::CharType kDataFileName[] =
      FILE_PATH_LITERAL("iwa-key-distribution.pb");

  IwaKeyDistributionComponentInstallerPolicy();
  ~IwaKeyDistributionComponentInstallerPolicy() override;

  IwaKeyDistributionComponentInstallerPolicy(
      const IwaKeyDistributionComponentInstallerPolicy&) = delete;
  IwaKeyDistributionComponentInstallerPolicy operator=(
      const IwaKeyDistributionComponentInstallerPolicy&) = delete;

  // Tells whether the component is supported on a particular platform wrt to
  // the feature flags.
  static bool IsSupported();

  // Triggers an on-demand update for the component. Returns whether the update
  // has been queued.
  // This function is supposed to be used by `IwaKeyDistributionInfoProvider`.
  // Takes no effect if called before the component registration completes.
  static bool QueueOnDemandUpdate(
      base::PassKey<web_app::IwaKeyDistributionInfoProvider>);

 private:
  // ComponentInstallerPolicy:
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
};

// Called once during startup to make the component update service aware of
// the IWA Key Distribution component.
void RegisterIwaKeyDistributionComponent(ComponentUpdateService* cus);

}  // namespace component_updater

#endif  // CHROME_BROWSER_COMPONENT_UPDATER_IWA_KEY_DISTRIBUTION_COMPONENT_INSTALLER_H_
