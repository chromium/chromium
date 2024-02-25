// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/component_updater/soda_language_pack_component_installer.h"

#include <iterator>
#include <memory>
#include <string>
#include <vector>

#include "base/containers/flat_set.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/notreached.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "base/version.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/component_updater/soda_component_installer.h"
#include "components/component_updater/component_updater_service.h"
#include "components/crx_file/id_util.h"
#include "components/prefs/pref_service.h"
#include "components/soda/constants.h"
#include "components/update_client/update_client_errors.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"

namespace component_updater {

namespace {

constexpr char kLanguagePackManifestName[] = "SODA %s Models";

}  // namespace

SodaLanguagePackComponentInstallerPolicy::
    SodaLanguagePackComponentInstallerPolicy(
        speech::SodaLanguagePackComponentConfig language_config,
        PrefService* prefs,
        OnSodaLanguagePackComponentReadyCallback on_ready_callback)
    : language_config_(language_config),
      prefs_(prefs),
      on_ready_callback_(std::move(on_ready_callback)) {}

SodaLanguagePackComponentInstallerPolicy::
    ~SodaLanguagePackComponentInstallerPolicy() {
  prefs_ = nullptr;
}

std::string SodaLanguagePackComponentInstallerPolicy::GetExtensionId(
    speech::LanguageCode language_code) {
  std::optional<speech::SodaLanguagePackComponentConfig> config =
      speech::GetLanguageComponentConfig(language_code);

  if (config) {
    return crx_file::id_util::GenerateIdFromHash(config.value().public_key_sha);
  }

  return std::string();
}

base::flat_set<std::string>
SodaLanguagePackComponentInstallerPolicy::GetExtensionIds() {
  base::flat_set<std::string> ids;
  for (const speech::SodaLanguagePackComponentConfig& config :
       speech::kLanguageComponentConfigs) {
    ids.insert(crx_file::id_util::GenerateIdFromHash(config.public_key_sha));
  }

  return ids;
}

void SodaLanguagePackComponentInstallerPolicy::
    UpdateSodaLanguagePackComponentOnDemand(
        speech::LanguageCode language_code) {
  const std::string crx_id =
      SodaLanguagePackComponentInstallerPolicy::GetExtensionId(language_code);
  g_browser_process->component_updater()->GetOnDemandUpdater().OnDemandUpdate(
      crx_id, OnDemandUpdater::Priority::FOREGROUND,
      base::BindOnce([](update_client::Error error) {
        if (error != update_client::Error::NONE &&
            error != update_client::Error::UPDATE_IN_PROGRESS) {
          LOG(ERROR)
              << "On demand update of the SODA language component failed "
                 "with error: "
              << static_cast<int>(error);
        }
      }));
}

bool SodaLanguagePackComponentInstallerPolicy::VerifyInstallation(
    const base::Value::Dict& manifest,
    const base::FilePath& install_dir) const {
  return base::PathExists(
      install_dir.Append(speech::kSodaLanguagePackDirectoryRelativePath));
}

bool SodaLanguagePackComponentInstallerPolicy::
    SupportsGroupPolicyEnabledComponentUpdates() const {
  return true;
}

bool SodaLanguagePackComponentInstallerPolicy::RequiresNetworkEncryption()
    const {
  return true;
}

update_client::CrxInstaller::Result
SodaLanguagePackComponentInstallerPolicy::OnCustomInstall(
    const base::Value::Dict& manifest,
    const base::FilePath& install_dir) {
  return SodaComponentInstallerPolicy::SetComponentDirectoryPermission(
      install_dir);
}

void SodaLanguagePackComponentInstallerPolicy::OnCustomUninstall() {}

void SodaLanguagePackComponentInstallerPolicy::ComponentReady(
    const base::Version& version,
    const base::FilePath& install_dir,
    base::Value::Dict manifest) {
  VLOG(1) << "Component ready, version " << version.GetString() << " in "
          << install_dir.value();

#if !BUILDFLAG(IS_ANDROID)
  prefs_->SetFilePath(
      language_config_.config_path_pref,
      install_dir.Append(speech::kSodaLanguagePackDirectoryRelativePath));
#endif  //! BUILDFLAG(IS_ANDROID)

  if (on_ready_callback_) {
    std::move(on_ready_callback_).Run(language_config_.language_code);
  }
}

base::FilePath SodaLanguagePackComponentInstallerPolicy::GetRelativeInstallDir()
    const {
  return base::FilePath(speech::kSodaLanguagePacksRelativePath)
      .AppendASCII(language_config_.language_name);
}

void SodaLanguagePackComponentInstallerPolicy::GetHash(
    std::vector<uint8_t>* hash) const {
  hash->assign(std::begin(language_config_.public_key_sha),
               std::end(language_config_.public_key_sha));
}

std::string SodaLanguagePackComponentInstallerPolicy::GetName() const {
  return base::StringPrintf(kLanguagePackManifestName,
                            language_config_.language_name);
}

update_client::InstallerAttributes
SodaLanguagePackComponentInstallerPolicy::GetInstallerAttributes() const {
  return update_client::InstallerAttributes();
}

void RegisterSodaLanguagePackComponent(
    speech::SodaLanguagePackComponentConfig language_config,
    ComponentUpdateService* cus,
    PrefService* prefs,
    OnSodaLanguagePackComponentReadyCallback on_ready_callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  auto installer = base::MakeRefCounted<ComponentInstaller>(
      std::make_unique<SodaLanguagePackComponentInstallerPolicy>(
          language_config, prefs, std::move(on_ready_callback)));

  installer->Register(
      cus, base::BindOnce(&SodaLanguagePackComponentInstallerPolicy::
                              UpdateSodaLanguagePackComponentOnDemand,
                          language_config.language_code));
}

}  // namespace component_updater
