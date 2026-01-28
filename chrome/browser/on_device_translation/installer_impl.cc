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

OnDeviceTranslationInstallerImpl::OnDeviceTranslationInstallerImpl() = default;
OnDeviceTranslationInstallerImpl::~OnDeviceTranslationInstallerImpl() = default;

std::set<LanguagePackKey>
OnDeviceTranslationInstallerImpl::InstalledLanguagePacks() const {
  std::set<LanguagePackKey> installed_pack_keys;
  for (const auto& it : kLanguagePackComponentConfigMap) {
    if (!GetFilePathFromGlobalPrefs(g_browser_process->local_state(),
                                    GetComponentPathPrefName(*it.second))
             .empty()) {
      installed_pack_keys.insert(it.first);
    }
  }
  return installed_pack_keys;
}

std::set<LanguagePackKey>
OnDeviceTranslationInstallerImpl::RegisteredLanguagePacks() const {
  std::set<LanguagePackKey> registered_pack_keys;
  for (const auto& it : kLanguagePackComponentConfigMap) {
    if (g_browser_process->local_state()->GetBoolean(
            GetRegisteredFlagPrefName(*it.second))) {
      registered_pack_keys.insert(it.first);
    }
  }
  return registered_pack_keys;
}

bool OnDeviceTranslationInstallerImpl::IsInit() const {
  return !GetTranslateKitLibraryPath(g_browser_process->local_state()).empty();
}

void OnDeviceTranslationInstallerImpl::Init(
    base::RepeatingClosure on_ready_callback) {
  component_updater::RegisterTranslateKitComponent(
      g_browser_process->component_updater(), g_browser_process->local_state(),
      /*force_install=*/true,
      /*registered_callback=*/
      base::BindOnce(&component_updater::TranslateKitComponentInstallerPolicy::
                         UpdateComponentOnDemand,
                     base::Unretained(g_browser_process->component_updater())),
      /*on_ready_callback=*/
      std::move(on_ready_callback));
}

void OnDeviceTranslationInstallerImpl::InstallLanguagePack(
    LanguagePackKey language_pack) {
  if (!IsInit()) {
    return;
  }

  // Registers the TranslateKit language pack component.
  component_updater::RegisterTranslateKitLanguagePackComponent(
      g_browser_process->component_updater(), g_browser_process->local_state(),
      language_pack,
      base::BindOnce(
          &component_updater::TranslateKitLanguagePackComponentInstallerPolicy::
              UpdateComponentOnDemand,
          base::Unretained(g_browser_process->component_updater()),
          language_pack),
      base::BindRepeating(
          &OnDeviceTranslationInstallerImpl::OnLanguagePackInstalled,
          weak_ptr_factory_.GetWeakPtr(), language_pack));
}

void OnDeviceTranslationInstallerImpl::UnInstallLanguagePack(
    LanguagePackKey language_pack) {
  // Uninstalls the TranslateKit language pack component.
  component_updater::UninstallTranslateKitLanguagePackComponent(
      g_browser_process->component_updater(), g_browser_process->local_state(),
      language_pack);
}

void OnDeviceTranslationInstallerImpl::AddOserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void OnDeviceTranslationInstallerImpl::OnLanguagePackInstalled(
    LanguagePackKey language_pack) {
  for (Observer& observer : observers_) {
    observer.OnLanguagePackInstalled(language_pack);
  }
}

}  // namespace on_device_translation
