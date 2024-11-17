// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/component_updater/translate_kit_language_pack_component_installer.h"

#include <algorithm>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/ranges/algorithm.h"
#include "base/strings/strcat.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/on_device_translation/constants.h"
#include "chrome/browser/on_device_translation/language_pack_util.h"
#include "components/component_updater/component_updater_service.h"
#include "components/crx_file/id_util.h"
#include "components/update_client/update_client_errors.h"
#include "content/public/browser/browser_thread.h"

using on_device_translation::LanguagePackKey;

namespace component_updater {
namespace {

// The manifest name prefix of the TranslateKit language pack component.
constexpr char kTranslateKitLanguagePackManifestNamePrefix[] =
    "Chrome TranslateKit ";

}  // namespace

TranslateKitLanguagePackComponentInstallerPolicy::
    TranslateKitLanguagePackComponentInstallerPolicy(
        PrefService* pref_service,
        LanguagePackKey language_pack_key)
    : language_pack_key_(language_pack_key), pref_service_(pref_service) {}

TranslateKitLanguagePackComponentInstallerPolicy::
    ~TranslateKitLanguagePackComponentInstallerPolicy() = default;

bool TranslateKitLanguagePackComponentInstallerPolicy::VerifyInstallation(
    const base::Value::Dict& manifest,
    const base::FilePath& install_dir) const {
  // Check that the sub-directories of the package install directory exist.
  return base::ranges::all_of(
      GetPackageInstallSubDirNamesForVerification(language_pack_key_),
      [&install_dir](const std::string& sub_dir_name) {
        return base::PathExists(install_dir.AppendASCII(sub_dir_name));
      });
}

bool TranslateKitLanguagePackComponentInstallerPolicy::
    SupportsGroupPolicyEnabledComponentUpdates() const {
  return true;
}

bool TranslateKitLanguagePackComponentInstallerPolicy::
    RequiresNetworkEncryption() const {
  return false;
}

update_client::CrxInstaller::Result
TranslateKitLanguagePackComponentInstallerPolicy::OnCustomInstall(
    const base::Value::Dict& manifest,
    const base::FilePath& install_dir) {
  // Nothing custom here.
  return update_client::CrxInstaller::Result(0);
}

void TranslateKitLanguagePackComponentInstallerPolicy::OnCustomUninstall() {}

void TranslateKitLanguagePackComponentInstallerPolicy::ComponentReady(
    const base::Version& version,
    const base::FilePath& install_dir,
    base::Value::Dict manifest) {
  CHECK(pref_service_);
  pref_service_->SetFilePath(
      on_device_translation::GetComponentPathPrefName(GetConfig()),
      install_dir);
}

base::FilePath
TranslateKitLanguagePackComponentInstallerPolicy::GetRelativeInstallDir()
    const {
  return base::FilePath(on_device_translation::
                            kTranslateKitLanguagePackInstallationRelativeDir)
      .AppendASCII(
          on_device_translation::GetPackageInstallDirName(language_pack_key_));
}

void TranslateKitLanguagePackComponentInstallerPolicy::GetHash(
    std::vector<uint8_t>* hash) const {
  auto const& config = GetConfig();
  hash->assign(std::begin(config.public_key_sha),
               std::end(config.public_key_sha));
}

std::string TranslateKitLanguagePackComponentInstallerPolicy::GetName() const {
  return base::StrCat(
      {kTranslateKitLanguagePackManifestNamePrefix,
       on_device_translation::GetPackageNameSuffix(language_pack_key_)});
}

update_client::InstallerAttributes
TranslateKitLanguagePackComponentInstallerPolicy::GetInstallerAttributes()
    const {
  return update_client::InstallerAttributes();
}

const on_device_translation::LanguagePackComponentConfig&
TranslateKitLanguagePackComponentInstallerPolicy::GetConfig() const {
  return on_device_translation::GetLanguagePackComponentConfig(
      language_pack_key_);
}

// static
void TranslateKitLanguagePackComponentInstallerPolicy::UpdateComponentOnDemand(
    LanguagePackKey language_pack_key) {
  auto language_pack_crx_id = crx_file::id_util::GenerateIdFromHash(
      on_device_translation::GetLanguagePackComponentConfig(language_pack_key)
          .public_key_sha);
  g_browser_process->component_updater()->GetOnDemandUpdater().OnDemandUpdate(
      language_pack_crx_id,
      component_updater::OnDemandUpdater::Priority::FOREGROUND,
      base::BindOnce([](update_client::Error error) {
        if (error != update_client::Error::NONE &&
            error != update_client::Error::UPDATE_IN_PROGRESS) {
          // TODO(crbug.com/358030919): Add UMA.
          LOG(ERROR) << "Failed to update TranslateKit language pack:"
                     << static_cast<int>(error);
        }
      }));
}

void RegisterTranslateKitLanguagePackComponent(
    ComponentUpdateService* cus,
    PrefService* pref_service,
    LanguagePackKey language_pack_key,
    base::OnceClosure registered_callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // If the component is already installed, do nothing.
  const std::vector<std::string> component_ids = cus->GetComponentIDs();
  if (std::find(component_ids.begin(), component_ids.end(),
                crx_file::id_util::GenerateIdFromHash(
                    on_device_translation::GetLanguagePackComponentConfig(
                        language_pack_key)
                        .public_key_sha)) != component_ids.end()) {
    return;
  }

  pref_service->SetBoolean(
      on_device_translation::GetRegisteredFlagPrefName(
          *on_device_translation::kLanguagePackComponentConfigMap.at(
              language_pack_key)),
      true);
  base::MakeRefCounted<ComponentInstaller>(
      std::make_unique<TranslateKitLanguagePackComponentInstallerPolicy>(
          pref_service, language_pack_key))
      ->Register(cus, std::move(registered_callback));
}

void RegisterTranslateKitLanguagePackComponentsForUpdate(
    ComponentUpdateService* cus,
    PrefService* pref_service) {
  for (const auto& it :
       on_device_translation::kLanguagePackComponentConfigMap) {
    if (pref_service->GetBoolean(
            on_device_translation::GetRegisteredFlagPrefName(*it.second))) {
      RegisterTranslateKitLanguagePackComponent(cus, pref_service, it.first,
                                                base::OnceClosure());
    }
  }
}

void UninstallTranslateKitLanguagePackComponent(
    ComponentUpdateService* cus,
    PrefService* pref_service,
    LanguagePackKey language_pack_key) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  const auto* config =
      on_device_translation::kLanguagePackComponentConfigMap.at(
          language_pack_key);
  pref_service->SetBoolean(
      on_device_translation::GetRegisteredFlagPrefName(*config), false);
  pref_service->SetFilePath(
      on_device_translation::GetComponentPathPrefName(*config),
      base::FilePath());
  cus->UnregisterComponent(crx_file::id_util::GenerateIdFromHash(
      on_device_translation::GetLanguagePackComponentConfig(language_pack_key)
          .public_key_sha));
}

}  // namespace component_updater
