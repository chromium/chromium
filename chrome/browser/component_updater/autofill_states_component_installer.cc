// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/component_updater/autofill_states_component_installer.h"

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/ranges/algorithm.h"
#include "base/task/post_task.h"
#include "components/autofill/core/browser/geo/country_data.h"
#include "components/autofill/core/common/autofill_prefs.h"
#include "components/component_updater/component_updater_service.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"

using content::BrowserThread;

namespace {

// The SHA256 of the SubjectPublicKeyInfo used to sign the extension.
// The extension id is: eeigpngbgcognadeebkilcpcaedhellh
const std::array<uint8_t, 32> kAutofillStatesPublicKeySHA256 = {
    0x44, 0x86, 0xfd, 0x61, 0x62, 0xe6, 0xd0, 0x34, 0x41, 0xa8, 0xb2,
    0xf2, 0x04, 0x37, 0x4b, 0xb7, 0x0b, 0xae, 0x93, 0x12, 0x9d, 0x58,
    0x15, 0xb5, 0xdd, 0x89, 0xf2, 0x98, 0x73, 0xd3, 0x08, 0x97};

// Update the files installation path in prefs.
void UpdateAutofillStatesInstallDirPref(PrefService* prefs,
                                        const base::FilePath& install_dir) {
  prefs->SetFilePath(autofill::prefs::kAutofillStatesDataDir, install_dir);
}

// Returns the filenames corresponding to the states data.
std::vector<base::FilePath> AutofillStateFileNames() {
  std::vector<base::FilePath> filenames;
  for (const auto& country_code :
       autofill::CountryDataMap::GetInstance()->country_codes()) {
    filenames.push_back(base::FilePath().AppendASCII(country_code));
  }
  return filenames;
}

}  // namespace

namespace component_updater {

AutofillStatesComponentInstallerPolicy::AutofillStatesComponentInstallerPolicy(
    OnAutofillStatesReadyCallback callback)
    : on_component_ready_callback_on_ui_(callback) {}

AutofillStatesComponentInstallerPolicy::
    ~AutofillStatesComponentInstallerPolicy() = default;

bool AutofillStatesComponentInstallerPolicy::
    SupportsGroupPolicyEnabledComponentUpdates() const {
  return false;
}

bool AutofillStatesComponentInstallerPolicy::RequiresNetworkEncryption() const {
  return false;
}

update_client::CrxInstaller::Result
AutofillStatesComponentInstallerPolicy::OnCustomInstall(
    const base::DictionaryValue& manifest,
    const base::FilePath& install_dir) {
  return update_client::CrxInstaller::Result(update_client::InstallError::NONE);
}

void AutofillStatesComponentInstallerPolicy::OnCustomUninstall() {}

void AutofillStatesComponentInstallerPolicy::ComponentReady(
    const base::Version& version,
    const base::FilePath& install_dir,
    std::unique_ptr<base::DictionaryValue> manifest) {
  DVLOG(1) << "Component ready, version " << version.GetString() << " in "
           << install_dir.value();

  content::GetUIThreadTaskRunner(
      {base::MayBlock(), base::TaskPriority::BEST_EFFORT})
      ->PostTask(FROM_HERE, base::BindOnce(on_component_ready_callback_on_ui_,
                                           install_dir));
}

// Called during startup and installation before ComponentReady().
bool AutofillStatesComponentInstallerPolicy::VerifyInstallation(
    const base::DictionaryValue& manifest,
    const base::FilePath& install_dir) const {
  // Verify that state files are present.
  return base::ranges::count(
             AutofillStateFileNames(), true, [&](const auto& filename) {
               return base::PathExists(install_dir.Append(filename));
             }) > 0;
}

base::FilePath AutofillStatesComponentInstallerPolicy::GetRelativeInstallDir()
    const {
  return base::FilePath(FILE_PATH_LITERAL("AutofillStates"));
}

void AutofillStatesComponentInstallerPolicy::GetHash(
    std::vector<uint8_t>* hash) const {
  hash->assign(kAutofillStatesPublicKeySHA256.begin(),
               kAutofillStatesPublicKeySHA256.end());
}

std::string AutofillStatesComponentInstallerPolicy::GetName() const {
  return "Autofill States Data";
}

update_client::InstallerAttributes
AutofillStatesComponentInstallerPolicy::GetInstallerAttributes() const {
  return update_client::InstallerAttributes();
}

std::vector<std::string> AutofillStatesComponentInstallerPolicy::GetMimeTypes()
    const {
  return std::vector<std::string>();
}

void RegisterAutofillStatesComponent(ComponentUpdateService* cus,
                                     PrefService* prefs) {
  DVLOG(1) << "Registering Autofill States data component.";
  auto installer = base::MakeRefCounted<ComponentInstaller>(
      std::make_unique<AutofillStatesComponentInstallerPolicy>(
          base::BindRepeating(&UpdateAutofillStatesInstallDirPref, prefs)));
  installer->Register(cus, base::OnceClosure());
}

}  // namespace component_updater
