// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/component_updater/translate_kit_language_pack_component_installer.h"

#include <stdint.h>

#include <algorithm>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/containers/flat_set.h"
#include "base/feature_list.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_forward.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "base/strings/string_split.h"
#include "chrome/browser/browser_process.h"
#include "components/component_updater/component_updater_service.h"
#include "components/crx_file/id_util.h"
#include "components/on_device_translation/features.h"
#include "components/on_device_translation/public/language_pack.h"
#include "components/on_device_translation/public/paths.h"
#include "components/prefs/pref_service.h"
#include "components/update_client/update_client_errors.h"
#include "content/public/browser/browser_thread.h"

namespace component_updater {
namespace {

using ::on_device_translation::LanguagePackKey;

// The manifest name prefix of the TranslateKit language pack component.
constexpr char kTranslateKitLanguagePackManifestNamePrefix[] =
    "Chrome TranslateKit ";

bool VerifyLanguagePackDirectory(LanguagePackKey language_pack_key,
                                 const base::FilePath& install_dir) {
  return std::ranges::all_of(
      on_device_translation::GetPackageInstallSubDirNamesForVerification(
          language_pack_key),
      [&install_dir](const std::string& sub_dir_name) {
        const base::FilePath sub_dir = install_dir.AppendASCII(sub_dir_name);
        if (!base::DirectoryExists(sub_dir)) {
          return false;
        }
        base::FileEnumerator enumerator(sub_dir, /*recursive=*/true,
                                        base::FileEnumerator::FILES);
        for (base::FilePath file = enumerator.Next(); !file.empty();
             file = enumerator.Next()) {
          if (enumerator.GetInfo().GetSize() > 0) {
            return true;
          }
        }
        return false;
      });
}

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
  return VerifyLanguagePackDirectory(language_pack_key_, install_dir);
}

bool TranslateKitLanguagePackComponentInstallerPolicy::
    SupportsGroupPolicyEnabledComponentUpdates() const {
  return true;
}

bool TranslateKitLanguagePackComponentInstallerPolicy::
    RequiresNetworkEncryption() const {
  return false;
}

bool TranslateKitLanguagePackComponentInstallerPolicy::AllowCachedCopies()
    const {
  // Language packs update rarely and quantized model weights diff poorly, so
  // keeping ~58 MB cached CRX copies for differential updates costs disk space
  // per installed pair for little download savings.
  return false;
}

bool TranslateKitLanguagePackComponentInstallerPolicy::
    AllowUpdatesOnMeteredConnections() const {
  return !base::FeatureList::IsEnabled(
      on_device_translation::kTranslationAPIBlockDownloadsOnMeteredNetwork);
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
  const std::string path_pref =
      on_device_translation::GetComponentPathPrefName(GetConfig());
  const bool changed = pref_service_->GetFilePath(path_pref) != install_dir;
  pref_service_->SetFilePath(path_pref, install_dir);
  if (changed) {
    pref_service_->CommitPendingWrite();
  }
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
  hash->assign_range(config.public_key_sha);
}

std::string TranslateKitLanguagePackComponentInstallerPolicy::GetName() const {
  return base::StrCat(
      {kTranslateKitLanguagePackManifestNamePrefix,
       on_device_translation::GetPackageNameSuffix(language_pack_key_)});
}

update_client::InstallerAttributes
TranslateKitLanguagePackComponentInstallerPolicy::GetInstallerAttributes()
    const {
  update_client::InstallerAttributes attributes;
  if (base::FeatureList::IsEnabled(
          on_device_translation::kTranslateKitV3Models)) {
    attributes["model_version"] = "v3";
  }
  return attributes;
}

const on_device_translation::LanguagePackComponentConfig&
TranslateKitLanguagePackComponentInstallerPolicy::GetConfig() const {
  return on_device_translation::GetLanguagePackComponentConfig(
      language_pack_key_);
}

// static
void TranslateKitLanguagePackComponentInstallerPolicy::UpdateComponentOnDemand(
    component_updater::ComponentUpdateService* cus,
    LanguagePackKey language_pack_key,
    base::OnceCallback<void(update_client::Error)> callback) {
  auto language_pack_crx_id = crx_file::id_util::GenerateIdFromHash(
      on_device_translation::GetLanguagePackComponentConfig(language_pack_key)
          .public_key_sha);
  cus->GetOnDemandUpdater().OnDemandUpdate(
      language_pack_crx_id,
      component_updater::OnDemandUpdater::Priority::FOREGROUND,
      base::BindOnce(
          [](base::OnceCallback<void(update_client::Error)> callback,
             update_client::Error error) {
            if (error != update_client::Error::NONE &&
                error != update_client::Error::UPDATE_IN_PROGRESS) {
              base::UmaHistogramEnumeration(
                  "ComponentUpdater.TranslateKit.LanguagePack.UpdateError",
                  error);
            }
            std::move(callback).Run(error);
          },
          std::move(callback)));
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

  const std::string registered_pref =
      on_device_translation::GetRegisteredFlagPrefName(
          *on_device_translation::kLanguagePackComponentConfigMap.at(
              language_pack_key));
  if (!pref_service->GetBoolean(registered_pref)) {
    pref_service->SetBoolean(registered_pref, true);
    pref_service->CommitPendingWrite();
  }
  base::MakeRefCounted<ComponentInstaller>(
      std::make_unique<TranslateKitLanguagePackComponentInstallerPolicy>(
          pref_service, language_pack_key, std::move(on_ready_callback)))
      ->Register(cus, std::move(registered_callback));
}

void RegisterTranslateKitLanguagePackComponentsForUpdate(
    ComponentUpdateService* cus,
    PrefService* pref_service,
    bool resume_unfinished_downloads) {
  int installed_pack_count = 0;
  bool cleared_uninstalled_flag = false;
  for (const auto& [language_pack_key, config] :
       on_device_translation::kLanguagePackComponentConfigMap) {
    const std::string path_pref =
        on_device_translation::GetComponentPathPrefName(*config);
    const base::FilePath installed_path = pref_service->GetFilePath(path_pref);
    const bool installed = !installed_path.empty();
    if (installed) {
      ++installed_pack_count;
    }
    if (!pref_service->GetBoolean(
            on_device_translation::GetRegisteredFlagPrefName(*config))) {
      continue;
    }
    if (!installed && !resume_unfinished_downloads) {
      // No request waits for this pack and no download check runs here. The
      // next `create()` registers it again.
      pref_service->SetBoolean(
          on_device_translation::GetRegisteredFlagPrefName(*config), false);
      cleared_uninstalled_flag = true;
      continue;
    }
    if (installed) {
      auto ready_called =
          base::MakeRefCounted<base::RefCountedData<bool>>(false);
      // `ComponentInstaller::StartRegistration` verifies `manifest.json` and
      // calls `VerifyInstallation()` before `FinishRegistration()` invokes
      // `ComponentReady()` (setting `ready_called->data`) and then
      // `registered_callback`. `base::Unretained(cus)` and
      // `base::Unretained(pref_service)` are safe because both pointers belong
      // to `g_browser_process` (`component_updater()` and `local_state()`),
      // which outlive the UI thread task queue running registration callbacks.
      RegisterTranslateKitLanguagePackComponent(
          cus, pref_service, language_pack_key,
          /*registered_callback=*/
          base::BindOnce(
              [](ComponentUpdateService* cus, PrefService* pref_service,
                 LanguagePackKey key, base::FilePath expected_path,
                 scoped_refptr<base::RefCountedData<bool>> ready_called) {
                if (ready_called->data) {
                  return;
                }
                const auto* config =
                    on_device_translation::kLanguagePackComponentConfigMap.at(
                        key);
                if (pref_service->GetFilePath(
                        on_device_translation::GetComponentPathPrefName(
                            *config)) == expected_path) {
                  UninstallTranslateKitLanguagePackComponent(cus, pref_service,
                                                             key);
                }
              },
              base::Unretained(cus), base::Unretained(pref_service),
              language_pack_key, installed_path, ready_called),
          /*on_ready_callback=*/
          base::BindRepeating(
              [](scoped_refptr<base::RefCountedData<bool>> ready_called) {
                ready_called->data = true;
              },
              ready_called));
    } else {
      RegisterTranslateKitLanguagePackComponent(
          cus, pref_service, language_pack_key, base::OnceClosure(),
          base::RepeatingClosure());
    }
  }
  if (cleared_uninstalled_flag) {
    pref_service->CommitPendingWrite();
  }
  if (installed_pack_count > 0) {
    base::UmaHistogramExactLinear(
        "Translate.OnDeviceTranslation.InstalledLanguagePackCount",
        installed_pack_count, 50);
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
  for (std::string_view pair :
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
  const std::string registered_pref =
      on_device_translation::GetRegisteredFlagPrefName(*config);
  const std::string path_pref =
      on_device_translation::GetComponentPathPrefName(*config);
  const bool changed = pref_service->GetBoolean(registered_pref) ||
                       !pref_service->GetFilePath(path_pref).empty();
  pref_service->SetBoolean(registered_pref, false);
  pref_service->SetFilePath(path_pref, base::FilePath());
  if (changed) {
    pref_service->CommitPendingWrite();
  }
  cus->UnregisterComponent(crx_file::id_util::GenerateIdFromHash(
      on_device_translation::GetLanguagePackComponentConfig(language_pack_key)
          .public_key_sha));
}

}  // namespace component_updater
