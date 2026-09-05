// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/component_updater/aim_eligibility_component_installer.h"

#include <stdint.h>

#include <array>
#include <memory>
#include <string>
#include <utility>

#include "base/feature_list.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/component_loader.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/grit/aim_eligibility_extension_resources.h"
#include "components/component_updater/component_updater_service.h"
#include "components/omnibox/common/omnibox_features.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_thread.h"
#include "ui/base/resource/resource_bundle.h"

namespace component_updater {

namespace {

// The SHA256 of the SubjectPublicKeyInfo used to sign the extension.
// The extension id is: kabpfonokkamggpgbldambnmkliehlbh
constexpr std::array<uint8_t, 32> kAimEligibilityPublicKeySHA256 = {
    0xa0, 0x1f, 0x5e, 0xde, 0xaa, 0x0c, 0x66, 0xf6, 0x1b, 0x30, 0xc1,
    0xdc, 0xab, 0x84, 0x7b, 0x17, 0xa0, 0xb9, 0x5f, 0x0e, 0xa6, 0x9c,
    0xf7, 0x89, 0x39, 0x17, 0xb2, 0x95, 0x5b, 0x02, 0x89, 0xb8};

constexpr char kAimEligibilityManifestName[] =
    "AIM Eligibility Component Extension";

// Clears the staged version and manifest preferences. This ensures that
// `extensions::ComponentLoader` will fall back to loading the bundled component
// extension rather than attempting to load the staged version from disk on the
// next browser startup.
void ClearStagedExtensionPrefs() {
  if (g_browser_process && g_browser_process->local_state()) {
    PrefService* local_state = g_browser_process->local_state();
    local_state->ClearPref(
        extension_misc::kAimEligibilityExtensionStagedVersionPref);
    local_state->ClearPref(
        extension_misc::kAimEligibilityExtensionStagedManifestPref);
  }
}

}  // namespace

AimEligibilityComponentInstallerPolicy::
    AimEligibilityComponentInstallerPolicy() = default;
AimEligibilityComponentInstallerPolicy::
    ~AimEligibilityComponentInstallerPolicy() = default;

// static
void AimEligibilityComponentInstallerPolicy::RegisterPrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterStringPref(
      extension_misc::kAimEligibilityExtensionStagedVersionPref, std::string());
  registry->RegisterStringPref(
      extension_misc::kAimEligibilityExtensionStagedManifestPref,
      std::string());
}

bool AimEligibilityComponentInstallerPolicy::VerifyInstallation(
    const base::DictValue& /* manifest */,
    const base::FilePath& install_dir) const {
  return base::PathExists(
      install_dir.Append(FILE_PATH_LITERAL("manifest.json")));
}

bool AimEligibilityComponentInstallerPolicy::
    SupportsGroupPolicyEnabledComponentUpdates() const {
  return true;
}

bool AimEligibilityComponentInstallerPolicy::RequiresNetworkEncryption() const {
  return false;
}

update_client::CrxInstaller::Result
AimEligibilityComponentInstallerPolicy::OnCustomInstall(
    const base::DictValue& /* manifest */,
    const base::FilePath& /* install_dir */) {
  return update_client::CrxInstaller::Result(0);  // Nothing custom here.
}

void AimEligibilityComponentInstallerPolicy::OnCustomUninstall() {
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(&ClearStagedExtensionPrefs));
}

void AimEligibilityComponentInstallerPolicy::ComponentReady(
    const base::Version& version,
    const base::FilePath& install_dir,
    base::DictValue manifest) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // Ignore versions that are less than or equal to the bundled version.
  std::string bundled_manifest =
      ui::ResourceBundle::GetSharedInstance().LoadDataResourceString(
          IDR_AIM_ELIGIBILITY_EXTENSION_MANIFEST_JSON);
  base::Version bundled_version =
      extensions::ComponentLoader::GetVersionFromManifest(bundled_manifest);
  if (bundled_version.IsValid() && version <= bundled_version) {
    VLOG(1) << "Downloaded version (" << version.GetString()
            << ") is <= bundled version (" << bundled_version.GetString()
            << "). Clearing staged prefs.";
    ClearStagedExtensionPrefs();
    return;
  }

  VLOG(1) << "AIM Eligibility Component ready, version " << version.GetString()
          << " in " << install_dir.value();

  // Stage the update in the Prefs to be loaded on the next startup.
  if (g_browser_process && g_browser_process->local_state()) {
    PrefService* local_state = g_browser_process->local_state();
    std::string manifest_json;
    if (base::JSONWriter::Write(manifest, &manifest_json)) {
      local_state->SetString(
          extension_misc::kAimEligibilityExtensionStagedVersionPref,
          version.GetString());
      local_state->SetString(
          extension_misc::kAimEligibilityExtensionStagedManifestPref,
          manifest_json);
      VLOG(1) << "Staged AIM Eligibility Extension version "
              << version.GetString() << " for next startup.";
    }
  }
}

base::FilePath AimEligibilityComponentInstallerPolicy::GetRelativeInstallDir()
    const {
  return base::FilePath(extension_misc::kAimEligibilityExtensionDirName);
}

void AimEligibilityComponentInstallerPolicy::GetHash(
    std::vector<uint8_t>* hash) const {
  hash->assign_range(kAimEligibilityPublicKeySHA256);
}

std::string AimEligibilityComponentInstallerPolicy::GetName() const {
  return kAimEligibilityManifestName;
}

update_client::InstallerAttributes
AimEligibilityComponentInstallerPolicy::GetInstallerAttributes() const {
  return update_client::InstallerAttributes();
}

void ManageAimEligibilityComponentRegistration(ComponentUpdateService* cus) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (base::FeatureList::IsEnabled(
          omnibox::kAimEligibilityComponentExtension) &&
      omnibox::kAimEligibilityUseComponentUpdater.Get()) {
    VLOG(1) << "Registering AIM Eligibility component.";
    base::MakeRefCounted<ComponentInstaller>(
        std::make_unique<AimEligibilityComponentInstallerPolicy>())
        ->Register(cus, base::OnceClosure());
  } else {
    VLOG(1) << "Uninstalling AIM Eligibility component.";
    base::MakeRefCounted<ComponentInstaller>(
        std::make_unique<AimEligibilityComponentInstallerPolicy>())
        ->Uninstall();
  }
}

}  // namespace component_updater
