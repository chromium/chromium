// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/on_device_translation/installer_impl.h"

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/component_updater/translate_kit_component_installer.h"
#include "chrome/browser/component_updater/translate_kit_language_pack_component_installer.h"
#include "components/component_updater/component_updater_paths.h"
#include "components/component_updater/component_updater_service.h"
#include "components/on_device_translation/features.h"
#include "components/on_device_translation/installer.h"
#include "components/on_device_translation/public/language_pack.h"
#include "components/on_device_translation/public/paths.h"
#include "components/on_device_translation/public/pref_names.h"

namespace on_device_translation {
namespace {

base::FilePath GetFilePathFromGlobalPrefs(PrefService* prefs,
                                          std::string_view pref_name) {
  CHECK(prefs);
  base::FilePath path_in_pref = prefs->GetFilePath(pref_name);
  return path_in_pref;
}

base::FilePath GetTranslateKitLibraryPath(PrefService* prefs) {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(kTranslateKitBinaryPath)) {
    return command_line->GetSwitchValuePath(kTranslateKitBinaryPath);
  }
  return GetFilePathFromGlobalPrefs(prefs, prefs::kTranslateKitBinaryPath);
}

}  // namespace

OnDeviceTranslationInstallerImpl::OnDeviceTranslationInstallerImpl(
    component_updater::ComponentUpdateService* cus)
    : cus_(cus) {
  DCHECK(cus_);
}
OnDeviceTranslationInstallerImpl::~OnDeviceTranslationInstallerImpl() = default;

std::set<LanguagePackKey>
OnDeviceTranslationInstallerImpl::InstalledLanguagePacks(
    PrefService* prefs) const {
  std::set<LanguagePackKey> installed_pack_keys;
  for (const auto& it : kLanguagePackComponentConfigMap) {
    if (!GetFilePathFromGlobalPrefs(prefs, GetComponentPathPrefName(*it.second))
             .empty()) {
      installed_pack_keys.insert(it.first);
    }
  }
  return installed_pack_keys;
}

std::set<LanguagePackKey>
OnDeviceTranslationInstallerImpl::RegisteredLanguagePacks(
    PrefService* prefs) const {
  std::set<LanguagePackKey> registered_pack_keys;
  for (const auto& it : kLanguagePackComponentConfigMap) {
    if (prefs->GetBoolean(GetRegisteredFlagPrefName(*it.second))) {
      registered_pack_keys.insert(it.first);
    }
  }
  return registered_pack_keys;
}

bool OnDeviceTranslationInstallerImpl::IsInit(PrefService* prefs) const {
  return !GetTranslateKitLibraryPath(prefs).empty();
}

void OnDeviceTranslationInstallerImpl::Init(
    PrefService* pref_service,
    base::RepeatingClosure on_ready_callback) {
  component_updater::RegisterTranslateKitComponent(
      cus_, pref_service,
      /*force_install=*/true,
      /*registered_callback=*/
      base::BindOnce(&component_updater::TranslateKitComponentInstallerPolicy::
                         UpdateComponentOnDemand,
                     base::Unretained(cus_)),
      /*on_ready_callback=*/
      std::move(on_ready_callback));
}

bool OnDeviceTranslationInstallerImpl::InstallLanguagePack(
    LanguagePackKey language_pack,
    PrefService* pref_service) {
  if (!IsInit(pref_service)) {
    return false;
  }

  // Registers the TranslateKit language pack component.
  component_updater::RegisterTranslateKitLanguagePackComponent(
      cus_, pref_service, language_pack,
      base::BindOnce(
          &component_updater::TranslateKitLanguagePackComponentInstallerPolicy::
              UpdateComponentOnDemand,
          base::Unretained(cus_), language_pack),
      base::BindRepeating(
          &OnDeviceTranslationInstallerImpl::OnLanguagePackInstalled,
          weak_ptr_factory_.GetWeakPtr(), language_pack));
  return true;
}

bool OnDeviceTranslationInstallerImpl::UnInstallLanguagePack(
    LanguagePackKey language_pack,
    PrefService* pref_service) {
  // Uninstalls the TranslateKit language pack component.
  component_updater::UninstallTranslateKitLanguagePackComponent(
      cus_, pref_service, language_pack);
  return true;
}

void OnDeviceTranslationInstallerImpl::AddOserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void OnDeviceTranslationInstallerImpl::OnLanguagePackInstalled(
    LanguagePackKey language_pack) {
  installed_language_packs_.insert(language_pack);
  for (Observer& observer : observers_) {
    observer.OnLanguagePackInstalled(language_pack);
  }
}

}  // namespace on_device_translation
