// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COMPONENT_UPDATER_FIRST_PARTY_SETS_COMPONENT_INSTALLER_H_
#define CHROME_BROWSER_COMPONENT_UPDATER_FIRST_PARTY_SETS_COMPONENT_INSTALLER_H_

#include <stdint.h>

#include <memory>
#include <string>
#include <vector>

#include "base/files/file.h"
#include "base/functional/callback.h"
#include "base/gtest_prod_util.h"
#include "base/strings/string_piece.h"
#include "base/values.h"
#include "base/version.h"
#include "components/component_updater/component_installer.h"

namespace base {
class FilePath;
}  // namespace base

namespace component_updater {

class ComponentUpdateService;

class FirstPartySetsComponentInstallerPolicy : public ComponentInstallerPolicy {
 public:
  using SetsReadyOnceCallback =
      base::OnceCallback<void(base::Version, base::File)>;

  // |on_sets_ready| will be called on the UI thread when the sets are ready. It
  // is exposed here for testing.
  explicit FirstPartySetsComponentInstallerPolicy(
      SetsReadyOnceCallback on_sets_ready);
  ~FirstPartySetsComponentInstallerPolicy() override;

  FirstPartySetsComponentInstallerPolicy(
      const FirstPartySetsComponentInstallerPolicy&) = delete;
  FirstPartySetsComponentInstallerPolicy operator=(
      const FirstPartySetsComponentInstallerPolicy&) = delete;

  void OnRegistrationComplete();

  // Resets static state. Should only be used to clear state during testing.
  static void ResetForTesting();

  // Seeds a component at `install_dir` with the given `contents`. Only to be
  // used in testing.
  static void WriteComponentForTesting(base::Version version,
                                       const base::FilePath& install_dir,
                                       base::StringPiece contents);

 private:
  FRIEND_TEST_ALL_PREFIXES(FirstPartySetsComponentInstallerFeatureEnabledTest,
                           NonexistentFile_OnComponentReady);
  FRIEND_TEST_ALL_PREFIXES(FirstPartySetsComponentInstallerFeatureEnabledTest,
                           NonexistentFile_OnRegistrationComplete);
  FRIEND_TEST_ALL_PREFIXES(FirstPartySetsComponentInstallerFeatureEnabledTest,
                           LoadsSets_OnComponentReady);
  FRIEND_TEST_ALL_PREFIXES(FirstPartySetsComponentInstallerFeatureEnabledTest,
                           IgnoreNewSets_NoInitialComponent);
  FRIEND_TEST_ALL_PREFIXES(FirstPartySetsComponentInstallerFeatureEnabledTest,
                           IgnoreNewSets_OnComponentReady);
  FRIEND_TEST_ALL_PREFIXES(FirstPartySetsComponentInstallerFeatureEnabledTest,
                           IgnoreNewSets_OnNetworkRestart);
  FRIEND_TEST_ALL_PREFIXES(FirstPartySetsComponentInstallerFeatureDisabledTest,
                           GetInstallerAttributes);
  FRIEND_TEST_ALL_PREFIXES(FirstPartySetsComponentInstallerFeatureEnabledTest,
                           GetInstallerAttributes);

  // The following methods override ComponentInstallerPolicy.
  bool SupportsGroupPolicyEnabledComponentUpdates() const override;
  bool RequiresNetworkEncryption() const override;
  update_client::CrxInstaller::Result OnCustomInstall(
      const base::Value::Dict& manifest,
      const base::FilePath& install_dir) override;
  void OnCustomUninstall() override;
  bool VerifyInstallation(const base::Value::Dict& manifest,
                          const base::FilePath& install_dir) const override;
  // After the first call, ComponentReady will be no-op for new versions
  // delivered from Component Updater, i.e. new components will be installed
  // (kept on-disk) but not propagated to the NetworkService until next
  // browser startup.
  void ComponentReady(const base::Version& version,
                      const base::FilePath& install_dir,
                      base::Value::Dict manifest) override;
  base::FilePath GetRelativeInstallDir() const override;
  void GetHash(std::vector<uint8_t>* hash) const override;
  std::string GetName() const override;
  update_client::InstallerAttributes GetInstallerAttributes() const override;

  static base::FilePath GetInstalledPath(const base::FilePath& base);

  // We use a OnceCallback to ensure we only pass along the sets file once
  // during Chrome's lifetime (modulo reconfiguring the network service).
  SetsReadyOnceCallback on_sets_ready_;
};

// Call once during startup to make the component update service aware of
// the First-Party Sets component.
void RegisterFirstPartySetsComponent(ComponentUpdateService* cus);

}  // namespace component_updater

#endif  // CHROME_BROWSER_COMPONENT_UPDATER_FIRST_PARTY_SETS_COMPONENT_INSTALLER_H_
