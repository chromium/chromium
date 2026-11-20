// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/component_updater/autofill_custom_form_control_scripts_component_installer.h"

#include <stdint.h>

#include <array>
#include <memory>
#include <string>
#include <vector>

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/values.h"
#include "base/version.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/global_features.h"
#include "components/autofill/core/browser/integrators/custom_form_controls/custom_form_control_script_index.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_switches.h"
#include "components/update_client/update_client.h"
#include "components/update_client/update_client_errors.h"

namespace component_updater {

AutofillCustomFormControlScriptsComponentInstallerPolicy::
    AutofillCustomFormControlScriptsComponentInstallerPolicy() = default;

AutofillCustomFormControlScriptsComponentInstallerPolicy::
    ~AutofillCustomFormControlScriptsComponentInstallerPolicy() = default;

bool AutofillCustomFormControlScriptsComponentInstallerPolicy::
    SupportsGroupPolicyEnabledComponentUpdates() const {
  return true;
}

bool AutofillCustomFormControlScriptsComponentInstallerPolicy::
    RequiresNetworkEncryption() const {
  return false;
}

update_client::CrxInstaller::Result
AutofillCustomFormControlScriptsComponentInstallerPolicy::OnCustomInstall(
    const base::DictValue& manifest,
    const base::FilePath& install_dir) {
  return update_client::CrxInstaller::Result(update_client::InstallError::NONE);
}

void AutofillCustomFormControlScriptsComponentInstallerPolicy::
    OnCustomUninstall() {}

bool AutofillCustomFormControlScriptsComponentInstallerPolicy::
    VerifyInstallation(const base::DictValue& manifest,
                       const base::FilePath& install_dir) const {
  // `autofill::CustomFormControlScriptIndex::InitializeFromDirectory()`
  // validates the contents more thoroughly.
  return base::PathExists(install_dir.Append(FILE_PATH_LITERAL("index.json")));
}

void AutofillCustomFormControlScriptsComponentInstallerPolicy::ComponentReady(
    const base::Version& version,
    const base::FilePath& install_dir,
    base::DictValue manifest) {
  if (!g_browser_process || !g_browser_process->GetFeatures()) {
    return;
  }
  if (autofill::CustomFormControlScriptIndex* index =
          g_browser_process->GetFeatures()
              ->custom_form_control_script_index()) {
    index->InitializeFromDirectory(install_dir);
  }
}

base::FilePath AutofillCustomFormControlScriptsComponentInstallerPolicy::
    GetRelativeInstallDir() const {
  return base::FilePath(FILE_PATH_LITERAL("AutofillCustomFormControlScripts"));
}

void AutofillCustomFormControlScriptsComponentInstallerPolicy::GetHash(
    std::vector<uint8_t>* hash) const {
  // The SHA256 of the SubjectPublicKeyInfo used to sign the component.
  // The component's id is: aadedcgdooinnahidejdmjkhmpgaofpk
  static constexpr std::array<uint8_t, 32> kPublicKeySHA256 = {
      0x00, 0x34, 0x32, 0x63, 0xee, 0x8d, 0xd0, 0x78, 0x34, 0x93, 0xc9,
      0xa7, 0xcf, 0x60, 0xe5, 0xfa, 0x62, 0xc9, 0x1e, 0x7c, 0xd5, 0xe3,
      0x13, 0x91, 0xe4, 0x02, 0x81, 0xf6, 0x85, 0x62, 0xff, 0x87};
  hash->assign_range(kPublicKeySHA256);
}

std::string AutofillCustomFormControlScriptsComponentInstallerPolicy::GetName()
    const {
  return "Autofill Custom Form Control Scripts";
}

update_client::InstallerAttributes
AutofillCustomFormControlScriptsComponentInstallerPolicy::
    GetInstallerAttributes() const {
  return update_client::InstallerAttributes();
}

void RegisterAutofillCustomFormControlScriptsComponent(
    ComponentUpdateService* cus) {
  if (!base::FeatureList::IsEnabled(
          autofill::features::kAutofillCustomFormControlScripts) ||
      !base::CommandLine::ForCurrentProcess()
           ->GetSwitchValuePath(
               autofill::switches::kAutofillCustomFormControlScriptsDir)
           .empty()) {
    return;
  }
  auto installer = base::MakeRefCounted<ComponentInstaller>(
      std::make_unique<
          AutofillCustomFormControlScriptsComponentInstallerPolicy>());
  installer->Register(cus, base::OnceClosure());
}

}  // namespace component_updater
