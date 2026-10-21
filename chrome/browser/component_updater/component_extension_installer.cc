// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/component_updater/component_extension_installer.h"

#include <stdint.h>

#include <array>
#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/component_loader.h"
#include "chrome/browser/extensions/component_loader_prefs.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/grit/aim_eligibility_extension_resources.h"
#include "components/component_updater/component_updater_service.h"
#include "components/omnibox/common/omnibox_features.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/common/file_util.h"
#include "ui/webui/buildflags.h"

#if BUILDFLAG(ENABLE_WEBUI_NTP)
#include "chrome/grit/new_tab_page_extension_resources.h"
#include "components/search/ntp_features.h"
#endif  // BUILDFLAG(ENABLE_WEBUI_NTP)

namespace component_updater {

namespace {

// Note that the component ids (derived from `public_key_sha256`) and
// `extension_id` are independent and are expected to differ. The component
// (CRX) is signed with its own key and has its own manifest.json, while
// `extension_id` is derived from the "key" in extension_manifest.json.

// The SHA256 of the SubjectPublicKeyInfo used to sign the extension.
// The extension id is: kabpfonokkamggpgbldambnmkliehlbh
constexpr std::array<uint8_t, 32> kAimEligibilityPublicKeySHA256 = {
    0xa0, 0x1f, 0x5e, 0xde, 0xaa, 0x0c, 0x66, 0xf6, 0x1b, 0x30, 0xc1,
    0xdc, 0xab, 0x84, 0x7b, 0x17, 0xa0, 0xb9, 0x5f, 0x0e, 0xa6, 0x9c,
    0xf7, 0x89, 0x39, 0x17, 0xb2, 0x95, 0x5b, 0x02, 0x89, 0xb8};

// The name of the AIM Eligibility component.
constexpr char kAimEligibilityManifestName[] =
    "AIM Eligibility Component Extension";

// The manifest filename of the component extension.
constexpr base::FilePath::CharType kExtensionManifestFilename[] =
    FILE_PATH_LITERAL("extension_manifest.json");

#if BUILDFLAG(ENABLE_WEBUI_NTP)
// The SHA256 of the SubjectPublicKeyInfo used to sign the component.
// The component's id is: pkohanfffhahpakjbppgjidngeadjcfj
constexpr std::array<uint8_t, 32> kNewTabPagePublicKeySHA256 = {
    0xfa, 0xe7, 0x0d, 0x55, 0x57, 0x07, 0xf0, 0xa9, 0x1f, 0xf6, 0x98,
    0x3d, 0x64, 0x03, 0x92, 0x59, 0xf3, 0x78, 0x98, 0x81, 0x72, 0x58,
    0x18, 0xad, 0xf3, 0x0d, 0xfb, 0x31, 0xfa, 0xaa, 0xc2, 0x07};

// The name of the New Tab Page component.
constexpr char kNewTabPageManifestName[] = "New Tab Page Component Extension";
#endif  // BUILDFLAG(ENABLE_WEBUI_NTP)

// Clears the staged extension preferences. This ensures that
// `extensions::ComponentLoader` will fall back to loading the bundled component
// extension rather than attempting to load the staged version from disk on the
// next browser startup.
void ClearStagedExtensionPrefs(const std::string& extension_id) {
  if (g_browser_process && g_browser_process->local_state()) {
    extensions::component_loader_prefs::ClearExtension(
        *g_browser_process->local_state(), extension_id);
  }
}

// Tries to load and parse the extension manifest from `install_dir`.
std::optional<base::DictValue> LoadExtensionManifest(
    const base::FilePath& install_dir) {
  std::string error;
  std::optional<base::DictValue> manifest = extensions::file_util::LoadManifest(
      install_dir, kExtensionManifestFilename, &error);
  if (!manifest) {
    VLOG(1) << "Failed to load extension manifest from " << install_dir << ": "
            << error;
  }
  return manifest;
}

// Tries to stage `manifest` in Local State preferences for the next startup.
void OnExtensionManifestLoaded(const ComponentExtensionInstallerConfig& config,
                               const base::FilePath& relative_path,
                               std::optional<base::DictValue> manifest) {
  CHECK_CURRENTLY_ON(content::BrowserThread::UI, base::NotFatalUntil::M161);
  if (!g_browser_process || !g_browser_process->local_state()) {
    return;
  }
  if (!extensions::ComponentLoader::MaybeStageExtension(
          *g_browser_process->local_state(), config.extension_id,
          config.bundled_manifest_resource_id, relative_path,
          std::move(manifest))) {
    VLOG(1) << "Failed to stage " << config.name << " from " << relative_path;
  }
}

// Called once `installer` has looked for installed versions and registered the
// component. If no valid installation was found, e.g. because the installed
// files were removed externally, this clears the staged extension preferences.
// Otherwise, `extensions::ComponentLoader` would keep loading the extension
// from a missing directory on every startup, rather than falling back to the
// bundled extension. If an installation was found, `ComponentReady()` handles
// it instead, by re-staging the extension from it or clearing the staged
// extension if its manifest is rejected.
void OnRegistrationFinished(scoped_refptr<ComponentInstaller> installer,
                            const std::string& extension_id) {
  CHECK_CURRENTLY_ON(content::BrowserThread::UI, base::NotFatalUntil::M161);
  if (!installer->GetInstalledFile(
          base::FilePath(kExtensionManifestFilename).MaybeAsASCII())) {
    ClearStagedExtensionPrefs(extension_id);
  }
}

void ManageComponentExtensionRegistration(
    ComponentUpdateService* cus,
    const ComponentExtensionInstallerConfig& config,
    bool enabled) {
  CHECK_CURRENTLY_ON(content::BrowserThread::UI, base::NotFatalUntil::M161);
  if (enabled) {
    VLOG(1) << "Registering " << config.name << ".";
    auto installer = base::MakeRefCounted<ComponentInstaller>(
        std::make_unique<ComponentExtensionInstallerPolicy>(config));
    installer->Register(cus, base::BindOnce(&OnRegistrationFinished, installer,
                                            config.extension_id));
  } else {
    VLOG(1) << "Uninstalling " << config.name << ".";
    base::MakeRefCounted<ComponentInstaller>(
        std::make_unique<ComponentExtensionInstallerPolicy>(config))
        ->Uninstall();
  }
}

}  // namespace

ComponentExtensionInstallerPolicy::ComponentExtensionInstallerPolicy(
    const ComponentExtensionInstallerConfig& config)
    : config_(config) {}
ComponentExtensionInstallerPolicy::~ComponentExtensionInstallerPolicy() =
    default;

bool ComponentExtensionInstallerPolicy::VerifyInstallation(
    const base::DictValue& /* manifest */,
    const base::FilePath& install_dir) const {
  return base::PathExists(install_dir.Append(kExtensionManifestFilename));
}

bool ComponentExtensionInstallerPolicy::
    SupportsGroupPolicyEnabledComponentUpdates() const {
  return true;
}

bool ComponentExtensionInstallerPolicy::RequiresNetworkEncryption() const {
  return false;
}

update_client::CrxInstaller::Result
ComponentExtensionInstallerPolicy::OnCustomInstall(
    const base::DictValue& /* manifest */,
    const base::FilePath& /* install_dir */) {
  return update_client::CrxInstaller::Result(0);  // Nothing custom here.
}

void ComponentExtensionInstallerPolicy::OnCustomUninstall() {
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&ClearStagedExtensionPrefs, config_.extension_id));
}

void ComponentExtensionInstallerPolicy::ComponentReady(
    const base::Version& version,
    const base::FilePath& install_dir,
    base::DictValue /* manifest */) {
  CHECK_CURRENTLY_ON(content::BrowserThread::UI, base::NotFatalUntil::M161);

  VLOG(1) << config_.name << " ready, version " << version.GetString() << " in "
          << install_dir;

  base::FilePath relative_path =
      GetRelativeInstallDir().AppendASCII(version.GetString());

  // The passed-in manifest is the component updater's manifest.json; load the
  // extension's extension_manifest.json from `install_dir`.
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
      base::BindOnce(&LoadExtensionManifest, install_dir),
      base::BindOnce(&OnExtensionManifestLoaded, config_,
                     std::move(relative_path)));
}

base::FilePath ComponentExtensionInstallerPolicy::GetRelativeInstallDir()
    const {
  return config_.relative_install_dir;
}

void ComponentExtensionInstallerPolicy::GetHash(
    std::vector<uint8_t>* hash) const {
  hash->assign_range(config_.public_key_sha256);
}

std::string ComponentExtensionInstallerPolicy::GetName() const {
  return config_.name;
}

update_client::InstallerAttributes
ComponentExtensionInstallerPolicy::GetInstallerAttributes() const {
  return update_client::InstallerAttributes();
}

void ManageComponentExtensionRegistrations(ComponentUpdateService* cus) {
  ManageComponentExtensionRegistration(
      cus,
      {.name = kAimEligibilityManifestName,
       .extension_id = extension_misc::kAimEligibilityExtensionId,
       .public_key_sha256 = kAimEligibilityPublicKeySHA256,
       .relative_install_dir =
           base::FilePath(extension_misc::kAimEligibilityExtensionDirName),
       .bundled_manifest_resource_id =
           IDR_AIM_ELIGIBILITY_EXTENSION_MANIFEST_JSON},
      base::FeatureList::IsEnabled(
          omnibox::kAimEligibilityComponentExtension) &&
          omnibox::kAimEligibilityUseComponentUpdater.Get());

#if BUILDFLAG(ENABLE_WEBUI_NTP)
  ManageComponentExtensionRegistration(
      cus,
      {.name = kNewTabPageManifestName,
       .extension_id = extension_misc::kNewTabPageExtensionId,
       .public_key_sha256 = kNewTabPagePublicKeySHA256,
       .relative_install_dir =
           base::FilePath(extension_misc::kNewTabPageExtensionDirName),
       .bundled_manifest_resource_id =
           IDR_NEW_TAB_PAGE_EXTENSION_MANIFEST_JSON},
      base::FeatureList::IsEnabled(ntp_features::kNtpComponentExtension) &&
          ntp_features::kNtpComponentExtensionUseComponentUpdater.Get());
#endif  // BUILDFLAG(ENABLE_WEBUI_NTP)
}

}  // namespace component_updater
