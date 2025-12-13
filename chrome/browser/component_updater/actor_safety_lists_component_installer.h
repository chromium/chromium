// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COMPONENT_UPDATER_ACTOR_SAFETY_LISTS_COMPONENT_INSTALLER_H_
#define CHROME_BROWSER_COMPONENT_UPDATER_ACTOR_SAFETY_LISTS_COMPONENT_INSTALLER_H_

#include <stdint.h>

#include <string>

#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/values.h"
#include "components/component_updater/component_installer.h"

namespace base {
class FilePath;
}  // namespace base

namespace component_updater {

class ComponentUpdateService;

class ActorSafetyListsComponentInstallerPolicy
    : public ComponentInstallerPolicy {
 public:
  using OnActorSafetyListsComponentReadyCallback =
      base::RepeatingCallback<void(const std::optional<std::string>&)>;

  explicit ActorSafetyListsComponentInstallerPolicy(
      OnActorSafetyListsComponentReadyCallback on_component_ready_cb);
  ActorSafetyListsComponentInstallerPolicy();
  ~ActorSafetyListsComponentInstallerPolicy() override;

  ActorSafetyListsComponentInstallerPolicy(
      const ActorSafetyListsComponentInstallerPolicy&) = delete;
  ActorSafetyListsComponentInstallerPolicy& operator=(
      const ActorSafetyListsComponentInstallerPolicy&) = delete;
  ActorSafetyListsComponentInstallerPolicy(
      ActorSafetyListsComponentInstallerPolicy&&) = delete;
  ActorSafetyListsComponentInstallerPolicy& operator=(
      ActorSafetyListsComponentInstallerPolicy&&) = delete;

  void ComponentReadyForTesting(const base::Version& version,
                                const base::FilePath& install_dir,
                                base::Value::Dict manifest) {
    ComponentReady(version, install_dir, std::move(manifest));
  }

  bool VerifyInstallation(const base::Value::Dict& manifest,
                          const base::FilePath& install_dir) const override;

  static base::FilePath GetInstalledPathForTesting(const base::FilePath& base);

 private:
  // The following methods override ComponentInstallerPolicy.
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

  static base::FilePath GetInstalledPath(const base::FilePath& base);

  OnActorSafetyListsComponentReadyCallback on_component_ready_cb_;
};

// Call once during startup to make the component update service aware of
// the File Type Policies component.
void RegisterActorSafetyListsComponent(ComponentUpdateService* cus,
                                       base::OnceClosure callback);

}  // namespace component_updater

#endif  // CHROME_BROWSER_COMPONENT_UPDATER_ACTOR_SAFETY_LISTS_COMPONENT_INSTALLER_H_
