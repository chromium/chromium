// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/on_device_translation/service_controller.h"

#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/no_destructor.h"
#include "base/ranges/algorithm.h"
#include "base/strings/strcat.h"
#include "base/strings/string_split.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/component_updater/translate_kit_language_pack_component_installer.h"
#include "chrome/browser/on_device_translation/constants.h"
#include "chrome/browser/on_device_translation/language_pack_util.h"
#include "chrome/browser/on_device_translation/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/services/on_device_translation/public/cpp/features.h"
#include "components/services/on_device_translation/public/mojom/on_device_translation_service.mojom.h"
#include "components/services/on_device_translation/public/mojom/translator.mojom.h"
#include "content/public/browser/service_process_host.h"

#if BUILDFLAG(IS_WIN)
#include "base/strings/utf_string_conversions.h"
#endif  // BUILDFLAG(IS_WIN)

using on_device_translation::kLanguagePackComponentConfigMap;
using on_device_translation::LanguagePackKey;
using on_device_translation::ToLanguageCode;
using on_device_translation::mojom::OnDeviceTranslationLanguagePackage;
using on_device_translation::mojom::OnDeviceTranslationLanguagePackagePtr;
using on_device_translation::mojom::OnDeviceTranslationServiceConfig;
using on_device_translation::mojom::OnDeviceTranslationServiceConfigPtr;

namespace {

const char kTranslateKitPackagePaths[] = "translate-kit-packages";

const char kOnDeviceTranslationServiceDisplayName[] =
    "On-device Translation Service";

base::FilePath GetFilePathFromGlobalPrefs(std::string_view pref_name) {
  PrefService* global_prefs = g_browser_process->local_state();
  CHECK(global_prefs);
  base::FilePath path_in_pref = global_prefs->GetFilePath(pref_name);
  return path_in_pref;
}

base::FilePath GetTranslateKitRootDir() {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(on_device_translation::kTranslateKitRootDir)) {
    return command_line->GetSwitchValuePath(
        on_device_translation::kTranslateKitRootDir);
  }
  if (base::FeatureList::IsEnabled(
          on_device_translation::kEnableTranslateKitComponent)) {
    return GetFilePathFromGlobalPrefs(prefs::kTranslateKitRootDir);
  }
  return base::FilePath();
}

base::FilePath GetTranslateKitLibraryPath() {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(on_device_translation::kTranslateKitBinaryPath)) {
    return command_line->GetSwitchValuePath(
        on_device_translation::kTranslateKitBinaryPath);
  }
  if (base::FeatureList::IsEnabled(
          on_device_translation::kEnableTranslateKitComponent)) {
    return GetFilePathFromGlobalPrefs(prefs::kTranslateKitBinaryPath);
  }
  return base::FilePath();
}

std::string ToString(base::FilePath path) {
#if BUILDFLAG(IS_WIN)
  // TODO(crbug.com/362123222): Get rid of conditional decoding.
  return path.AsUTF8Unsafe();
#else
  return path.value();
#endif  // BUILDFLAG(IS_WIN)
}

std::vector<OnDeviceTranslationLanguagePackagePtr>
GetLanguagePackagesFromCommnandLineString(
    base::CommandLine::StringType packages_string) {
  std::vector<base::CommandLine::StringType> splitted_strings =
      base::SplitString(packages_string,
#if BUILDFLAG(IS_WIN)
                        L",",
#else   // !BUILDFLAG(IS_WIN)
                        ",",
#endif  // BUILDFLAG(IS_WIN)
                        base::KEEP_WHITESPACE, base::SPLIT_WANT_ALL);
  if (splitted_strings.size() % 3 != 0) {
    LOG(ERROR) << "Invalid --translate-kit-packages flag";
    return std::vector<OnDeviceTranslationLanguagePackagePtr>();
  }

  std::vector<OnDeviceTranslationLanguagePackagePtr> packages;
  auto it = splitted_strings.begin();
  while (it != splitted_strings.end()) {
    if (!base::IsStringASCII(*it) || !base::IsStringASCII(*(it + 1))) {
      LOG(ERROR) << "Invalid --translate-kit-packages flag";
      return std::vector<OnDeviceTranslationLanguagePackagePtr>();
    }
    OnDeviceTranslationLanguagePackagePtr package =
        OnDeviceTranslationLanguagePackage::New();
#if BUILDFLAG(IS_WIN)
    package->language1 = base::WideToUTF8(*(it++));
    package->language2 = base::WideToUTF8(*(it++));
#else  // !BUILDFLAG(IS_WIN)
    package->language1 = *(it++);
    package->language2 = *(it++);
#endif
    package->package_path = base::FilePath(*(it++));
    packages.push_back(std::move(package));
  }
  return packages;
}

// Creates a config from the command line flag --translate-kit-packages.
OnDeviceTranslationServiceConfigPtr CreateConfigFromCommandLine() {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (!command_line->HasSwitch(kTranslateKitPackagePaths)) {
    return nullptr;
  }
  OnDeviceTranslationServiceConfigPtr config =
      OnDeviceTranslationServiceConfig::New();
  config->packages = GetLanguagePackagesFromCommnandLineString(
      command_line->GetSwitchValueNative(kTranslateKitPackagePaths));
  return config;
}

// Returns the language packs that are installed.
std::set<LanguagePackKey> GetInstalledLanguagePacks() {
  std::set<LanguagePackKey> insalled_pack_keys;
  for (const auto& it : kLanguagePackComponentConfigMap) {
    if (!GetFilePathFromGlobalPrefs(it.second->config_path_pref).empty()) {
      insalled_pack_keys.insert(it.first);
    }
  }
  return insalled_pack_keys;
}

}  // namespace

OnDeviceTranslationServiceController::OnDeviceTranslationServiceController()
    : config_from_command_line_(CreateConfigFromCommandLine()) {
  // Initialize the pref change registrar.
  pref_change_registrar_.Init(g_browser_process->local_state());
  // Start listening to pref changes for language pack keys.
  for (const auto& it : kLanguagePackComponentConfigMap) {
    pref_change_registrar_.Add(
        it.second->config_path_pref,
        base::BindRepeating(
            &OnDeviceTranslationServiceController::OnLanguagePackKeyPrefChanged,
            base::Unretained(this)));
  }
  // Register all the installed language pack components.
  RegisterInstalledLanguagePackComponent();
  auto receiver = service_remote_.BindNewPipeAndPassReceiver();
  service_remote_.reset_on_disconnect();

  const std::string root_dir = ToString(GetTranslateKitRootDir());
  const std::string binary_path = ToString(GetTranslateKitLibraryPath());
  if (root_dir.empty()) {
    LOG(ERROR) << "Got an empty root dir for TranslateKit.";
  }
  if (binary_path.empty()) {
    LOG(ERROR) << "Got an empty path to TranslateKit binary on the device.";
  }

  std::vector<std::string> extra_switches;
  extra_switches.push_back(base::StrCat(
      {on_device_translation::kTranslateKitRootDir, "=", root_dir}));
  extra_switches.push_back(base::StrCat(
      {on_device_translation::kTranslateKitBinaryPath, "=", binary_path}));

  content::ServiceProcessHost::Launch<
      on_device_translation::mojom::OnDeviceTranslationService>(
      std::move(receiver),
      content::ServiceProcessHost::Options()
          .WithDisplayName(kOnDeviceTranslationServiceDisplayName)
          .WithExtraCommandLineSwitches(extra_switches)
          .Pass());
  service_remote_->SetServiceConfig(GetConfig());
}

OnDeviceTranslationServiceController::~OnDeviceTranslationServiceController() =
    default;

void OnDeviceTranslationServiceController::CreateTranslator(
    const std::string& source_lang,
    const std::string& target_lang,
    mojo::PendingReceiver<on_device_translation::mojom::Translator> receiver,
    base::OnceCallback<void(bool)> callback) {
  MaybeTriggerLanguagePackInstall(source_lang, target_lang);
  // TODO(crbug.com/358030919): Implement a logic to defer the CreateTranslator
  // IPC call when a new language pack was installed.
  service_remote_->CreateTranslator(source_lang, target_lang,
                                    std::move(receiver), std::move(callback));
}

void OnDeviceTranslationServiceController::CanTranslate(
    const std::string& source_lang,
    const std::string& target_lang,
    base::OnceCallback<void(bool)> callback) {
  MaybeTriggerLanguagePackInstall(source_lang, target_lang);
  // TODO(crbug.com/358030919): Implement a logic to defer the CanTranslate
  // IPC call when a new language pack was installed.
  service_remote_->CanTranslate(source_lang, target_lang, std::move(callback));
}

// Get the config for the service.
OnDeviceTranslationServiceConfigPtr
OnDeviceTranslationServiceController::GetConfig() {
  if (config_from_command_line_) {
    return config_from_command_line_->Clone();
  }

  OnDeviceTranslationServiceConfigPtr config =
      OnDeviceTranslationServiceConfig::New();
  for (const auto& it : kLanguagePackComponentConfigMap) {
    auto file_path = GetFilePathFromGlobalPrefs(it.second->config_path_pref);
    if (!file_path.empty()) {
      OnDeviceTranslationLanguagePackagePtr package =
          OnDeviceTranslationLanguagePackage::New();
      package->language1 = ToLanguageCode(it.second->language1);
      package->language2 = ToLanguageCode(it.second->language2);
      package->package_path = file_path;
      config->packages.push_back(std::move(package));
    }
  }
  return config;
}

// Register the installed language pack components.
void OnDeviceTranslationServiceController::
    RegisterInstalledLanguagePackComponent() {
  for (const auto& language_pack : GetInstalledLanguagePacks()) {
    RegisterLanguagePackComponent(language_pack);
  }
}

// Maybe trigger the language pack install if the required language packs are
// not installed.
void OnDeviceTranslationServiceController::MaybeTriggerLanguagePackInstall(
    const std::string& source_lang,
    const std::string& target_lang) {
  const auto required_packs =
      on_device_translation::CalculateRequiredLanguagePacks(source_lang,
                                                            target_lang);
  if (required_packs.empty()) {
    return;
  }
  const auto installed_packs = GetInstalledLanguagePacks();
  std::vector<LanguagePackKey> differences;
  base::ranges::set_difference(required_packs, installed_packs,
                               std::back_inserter(differences));
  if (differences.empty()) {
    return;
  }
  std::vector<LanguagePackKey> to_be_installed;
  base::ranges::set_difference(differences, registered_language_packs_,
                               std::back_inserter(to_be_installed));
  for (const auto& language_pack : to_be_installed) {
    RegisterLanguagePackComponent(language_pack);
  }
}

// Register the language pack component.
void OnDeviceTranslationServiceController::RegisterLanguagePackComponent(
    LanguagePackKey language_pack) {
  CHECK(!registered_language_packs_.contains(language_pack));
  registered_language_packs_.insert(language_pack);
  component_updater::RegisterTranslateKitLanguagePackComponent(
      g_browser_process->component_updater(), g_browser_process->local_state(),
      language_pack, base::BindOnce([]() {
        // TODO(crbug.com/358030919): Consider calling
        // OnDemandUpdater::OnDemandUpdate() to trigger an update check.
      }));
}

// Called when the language pack key pref is changed.
void OnDeviceTranslationServiceController::OnLanguagePackKeyPrefChanged(
    const std::string& pref_name) {
  // Set the service config to the service.
  service_remote_->SetServiceConfig(GetConfig());
}

// static
OnDeviceTranslationServiceController*
OnDeviceTranslationServiceController::GetInstance() {
  static base::NoDestructor<OnDeviceTranslationServiceController> instance;
  return instance.get();
}
