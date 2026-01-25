// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COMPONENT_UPDATER_CAPTCHA_PROVIDER_COMPONENT_INSTALLER_H_
#define CHROME_BROWSER_COMPONENT_UPDATER_CAPTCHA_PROVIDER_COMPONENT_INSTALLER_H_

#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/functional/callback.h"
#include "base/values.h"
#include "components/component_updater/component_installer.h"

namespace base {
class FilePath;
}  // namespace base

namespace component_updater {

class ComponentUpdateService;

class CaptchaProviderComponentInstallerPolicy
    : public ComponentInstallerPolicy {
 public:
  using CaptchaProvidersReadyRepeatingCallback =
      base::RepeatingCallback<void(const std::optional<std::string>)>;

  explicit CaptchaProviderComponentInstallerPolicy(
      CaptchaProvidersReadyRepeatingCallback on_ready);
  CaptchaProviderComponentInstallerPolicy();
  CaptchaProviderComponentInstallerPolicy(
      const CaptchaProviderComponentInstallerPolicy&) = delete;
  CaptchaProviderComponentInstallerPolicy& operator=(
      const CaptchaProviderComponentInstallerPolicy&) = delete;
  ~CaptchaProviderComponentInstallerPolicy() override;

  void ComponentReadyForTesting(const base::Version& version,
                                const base::FilePath& install_dir,
                                base::DictValue manifest) {
    ComponentReady(version, install_dir, std::move(manifest));
  }

  static base::FilePath GetInstalledPath(const base::FilePath& install_dir);

  // ComponentInstallerPolicy:
  bool VerifyInstallation(const base::DictValue& manifest,
                          const base::FilePath& install_dir) const override;

 private:
  // ComponentInstallerPolicy:
  bool SupportsGroupPolicyEnabledComponentUpdates() const override;
  bool RequiresNetworkEncryption() const override;
  update_client::CrxInstaller::Result OnCustomInstall(
      const base::DictValue& manifest,
      const base::FilePath& install_dir) override;
  void OnCustomUninstall() override;
  void ComponentReady(const base::Version& version,
                      const base::FilePath& install_dir,
                      base::DictValue manifest) override;
  base::FilePath GetRelativeInstallDir() const override;
  void GetHash(std::vector<uint8_t>* hash) const override;
  std::string GetName() const override;
  update_client::InstallerAttributes GetInstallerAttributes() const override;

  CaptchaProvidersReadyRepeatingCallback on_ready_;
};

// Called when the Captcha Provider component is ready.
void OnCaptchaProviderComponentReady(std::optional<std::string> json_content);

// Call once during startup to make the component update service aware of
// the Captcha Provider component.
void RegisterCaptchaProviderComponent(ComponentUpdateService* cus);

}  // namespace component_updater

#endif  // CHROME_BROWSER_COMPONENT_UPDATER_CAPTCHA_PROVIDER_COMPONENT_INSTALLER_H_
