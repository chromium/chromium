// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COMPONENT_UPDATER_AUTOFILL_CUSTOM_FORM_CONTROL_SCRIPTS_COMPONENT_INSTALLER_H_
#define CHROME_BROWSER_COMPONENT_UPDATER_AUTOFILL_CUSTOM_FORM_CONTROL_SCRIPTS_COMPONENT_INSTALLER_H_

#include <stdint.h>

#include <string>
#include <vector>

#include "components/component_updater/component_installer.h"

namespace base {
class DictValue;
class FilePath;
class Version;
}  // namespace base

namespace component_updater {

class ComponentUpdateService;

// Responsible for receiving Autofill custom form control scripts from
// component updater and forwarding them to the
// `autofill::CustomFormControlScriptIndex`.
class AutofillCustomFormControlScriptsComponentInstallerPolicy
    : public ComponentInstallerPolicy {
 public:
  AutofillCustomFormControlScriptsComponentInstallerPolicy();
  AutofillCustomFormControlScriptsComponentInstallerPolicy(
      const AutofillCustomFormControlScriptsComponentInstallerPolicy&) = delete;
  AutofillCustomFormControlScriptsComponentInstallerPolicy& operator=(
      const AutofillCustomFormControlScriptsComponentInstallerPolicy&) = delete;
  ~AutofillCustomFormControlScriptsComponentInstallerPolicy() override;

 private:
  // ComponentInstallerPolicy:
  bool SupportsGroupPolicyEnabledComponentUpdates() const override;
  bool RequiresNetworkEncryption() const override;
  update_client::CrxInstaller::Result OnCustomInstall(
      const base::DictValue& manifest,
      const base::FilePath& install_dir) override;
  void OnCustomUninstall() override;
  bool VerifyInstallation(const base::DictValue& manifest,
                          const base::FilePath& install_dir) const override;
  void ComponentReady(const base::Version& version,
                      const base::FilePath& install_dir,
                      base::DictValue manifest) override;
  base::FilePath GetRelativeInstallDir() const override;
  void GetHash(std::vector<uint8_t>* hash) const override;
  std::string GetName() const override;
  update_client::InstallerAttributes GetInstallerAttributes() const override;
};

// Registers the policy installer above when `kAutofillCustomFormControlScripts`
// is enabled and no `--autofill-custom-form-control-scripts-dir` override is
// set.
void RegisterAutofillCustomFormControlScriptsComponent(
    ComponentUpdateService* cus);

}  // namespace component_updater

#endif  // CHROME_BROWSER_COMPONENT_UPDATER_AUTOFILL_CUSTOM_FORM_CONTROL_SCRIPTS_COMPONENT_INSTALLER_H_
