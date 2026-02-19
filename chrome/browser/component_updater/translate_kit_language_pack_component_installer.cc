// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/component_updater/translate_kit_language_pack_component_installer.h"

#include <algorithm>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/containers/flat_set.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/callback.h"
#include "base/functional/callback_forward.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "base/strings/string_split.h"
#include "chrome/browser/browser_process.h"
#include "components/component_updater/component_updater_service.h"
#include "components/crx_file/id_util.h"
#include "components/on_device_translation/features.h"
#include "components/on_device_translation/public/language_pack.h"
#include "components/on_device_translation/public/paths.h"
#include "components/update_client/update_client_errors.h"
#include "content/public/browser/browser_thread.h"

namespace component_updater {
namespace {

using ::on_device_translation::LanguagePackKey;

// The manifest name prefix of the TranslateKit language pack component.
constexpr char kTranslateKitLanguagePackManifestNamePrefix[] =
    "Chrome TranslateKit ";

}  // namespace

TranslateKitLanguagePackComponentInstallerPolicy::
    TranslateKitLanguagePackComponentInstallerPolicy(
        PrefService* pref_service,
        LanguagePackKey language_pack_key,
        base::RepeatingClosure on_ready_callback)
    : language_pack_key_(language_pack_key),
      pref_service_(pref_service),
      on_ready_callback_(std::move(on_ready_callback)) {}

TranslateKitLanguagePackComponentInstallerPolicy::
    ~TranslateKitLanguagePackComponentInstallerPolicy() = default;

bool TranslateKitLanguagePackComponentInstallerPolicy::VerifyInstallation(
    const base::DictValue& manifest,
    const base::FilePath& install_dir) const {
  // Check that the sub-directories of the package install directory exist.
  return std::ranges::all_of(
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
    const base::DictValue& manifest,
    const base::FilePath& install_dir) {
  // Nothing custom here.
  return update_client::CrxInstaller::Result(0);
}

void TranslateKitLanguagePackComponentInstallerPolicy::OnCustomUninstall() {}

void TranslateKitLanguagePackComponentInstallerPolicy::ComponentReady(
    const base::Version& version,
    const base::FilePath& install_dir,
    base::DictValue manifest) {
  CHECK(pref_service_);
  pref_service_->SetFilePath(
      on_device_translation::GetComponentPathPrefName(GetConfig()),
      install_dir);
  if (on_ready_callback_) {
    on_ready_callback_.Run();
  }
}

base::FilePath
TranslateKitLanguagePackComponentInstallerPolicy::GetRelativeInstallDir()
    const {
  return on_device_translation::GetLanguagePackRelativeInstallDir().AppendASCII(
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
    component_updater::ComponentUpdateService* cus,
    LanguagePackKey language_pack_key) {
  auto language_pack_crx_id = crx_file::id_util::GenerateIdFromHash(
      on_device_translation::GetLanguagePackComponentConfig(language_pack_key)
          .public_key_sha);
  cus->GetOnDemandUpdater().OnDemandUpdate(
      language_pack_crx_id,
      component_updater::OnDemandUpdater::Priority::FOREGROUND,
      base::BindOnce([](update_client::Error error) {
        if (error != update_client::Error::NONE &&
            error != update_client::Error::UPDATE_IN_PROGRESS) {
          base::UmaHistogramEnumeration(
              "ComponentUpdater.TranslateKit.LanguagePack.UpdateError", error);
        }
      }));
}

void RegisterTranslateKitLanguagePackComponent(
    ComponentUpdateService* cus,
    PrefService* pref_service,
    LanguagePackKey language_pack_key,
    base::OnceClosure registered_callback,
    base::RepeatingClosure on_ready_callback) {
  // If the component is already installed, do nothing.
  const std::vector<std::string> component_ids = cus->GetComponentIDs();
  if (std::ranges::find(
          component_ids,
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
          pref_service, language_pack_key, std::move(on_ready_callback)))
      ->Register(cus, std::move(registered_callback));
}

void RegisterTranslateKitLanguagePackComponentsForUpdate(
    ComponentUpdateService* cus,
    PrefService* pref_service) {
  for (const auto& [language_pack_key, config] :
       on_device_translation::kLanguagePackComponentConfigMap) {
    if (pref_service->GetBoolean(
            on_device_translation::GetRegisteredFlagPrefName(*config))) {
      RegisterTranslateKitLanguagePackComponent(
          cus, pref_service, language_pack_key, base::OnceClosure(),
          base::RepeatingClosure());
    }
  }
}

void RegisterTranslateKitLanguagePackComponentsForAutoDownload(
    ComponentUpdateService* cus,
    PrefService* pref_service) {
  if (!base::FeatureList::IsEnabled(
          on_device_translation::kAutoDownloadTranslateLanguagePacks)) {
    return;
  }

  // The list of language pairs for which language packs should be automatically
  // downloaded. The format is a comma-separated list of language pairs, e.g.
  // "en-es,en-fr".
  const std::string language_pairs_str =
      on_device_translation::kAutoDownloadTranslateLanguagePacksLanguagePairs
          .Get();
  if (language_pairs_str.empty()) {
    return;
  }

  base::flat_set<LanguagePackKey> keys_to_register;
  for (const std::string_view& pair :
       base::SplitStringPiece(language_pairs_str, ",", base::TRIM_WHITESPACE,
                              base::SPLIT_WANT_NONEMPTY)) {
    std::vector<std::string_view> languages = base::SplitStringPiece(
        pair, "-", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
    if (languages.size() != 2) {
      continue;
    }

    auto language1 = on_device_translation::ToSupportedLanguage(languages[0]);
    auto language2 = on_device_translation::ToSupportedLanguage(languages[1]);
    for (const auto& [key, config] :
         on_device_translation::kLanguagePackComponentConfigMap) {
      if ((language1 == config->language1 && language2 == config->language2) ||
          (language1 == config->language2 && language2 == config->language1)) {
        keys_to_register.insert(key);
        break;
      }
    }
  }

  for (const auto& key : keys_to_register) {
    RegisterTranslateKitLanguagePackComponent(
        cus, pref_service, key, base::OnceClosure(), base::RepeatingClosure());
  }
}

void UninstallTranslateKitLanguagePackComponent(
    ComponentUpdateService* cus,
    PrefService* pref_service,
    LanguagePackKey language_pack_key) {
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
