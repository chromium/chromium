// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COMPONENT_UPDATER_GAMES_COMPONENT_INSTALLER_H_
#define CHROME_BROWSER_COMPONENT_UPDATER_GAMES_COMPONENT_INSTALLER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/values.h"
#include "components/component_updater/component_installer.h"
#include "components/component_updater/component_updater_service.h"
#include "components/games/core/games_service.h"
#include "components/prefs/pref_service.h"
#include "components/update_client/update_client.h"

namespace component_updater {

// Success callback to be run after the component is downloaded.
using OnGamesComponentReadyCallback =
    base::RepeatingCallback<void(const base::FilePath&)>;

class GamesComponentInstallerPolicy : public ComponentInstallerPolicy {
 public:
  explicit GamesComponentInstallerPolicy(
      OnGamesComponentReadyCallback callback);
  ~GamesComponentInstallerPolicy() override;

 private:
  FRIEND_TEST_ALL_PREFIXES(GamesComponentInstallerTest,
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

  OnGamesComponentReadyCallback on_component_ready_callback_;

  DISALLOW_COPY_AND_ASSIGN(GamesComponentInstallerPolicy);
};

// Call once during startup to make the component update service aware of
// the File Type Policies component.
void RegisterGamesComponent(ComponentUpdateService* cus, PrefService* prefs);

}  // namespace component_updater

#endif  // CHROME_BROWSER_COMPONENT_UPDATER_GAMES_COMPONENT_INSTALLER_H_
