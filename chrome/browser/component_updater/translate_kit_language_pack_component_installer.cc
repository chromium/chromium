// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/component_updater/translate_kit_language_pack_component_installer.h"

#include <memory>
#include <string>
#include <utility>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/ranges/algorithm.h"
#include "base/strings/strcat.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/on_device_translation/constants.h"
#include "chrome/browser/on_device_translation/language_pack_util.h"
#include "components/component_updater/component_updater_service.h"
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
  pref_service_->SetFilePath(GetConfig().config_path_pref, install_dir);
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

void RegisterTranslateKitLanguagePackComponent(
    ComponentUpdateService* cus,
    PrefService* pref_service,
    LanguagePackKey language_pack_key,
    base::OnceClosure registered_callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  auto installer = base::MakeRefCounted<ComponentInstaller>(
      std::make_unique<TranslateKitLanguagePackComponentInstallerPolicy>(
          pref_service, language_pack_key));
  installer->Register(cus, std::move(registered_callback));
}

}  // namespace component_updater
