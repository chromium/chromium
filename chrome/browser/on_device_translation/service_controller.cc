// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/on_device_translation/service_controller.h"

#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/no_destructor.h"
#include "base/strings/strcat.h"
#include "base/strings/string_split.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/on_device_translation/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/services/on_device_translation/public/cpp/features.h"
#include "components/services/on_device_translation/public/mojom/on_device_translation_service.mojom.h"
#include "components/services/on_device_translation/public/mojom/translator.mojom.h"
#include "content/public/browser/service_process_host.h"

#if BUILDFLAG(IS_WIN)
#include "base/strings/utf_string_conversions.h"
#endif  // BUILDFLAG(IS_WIN)

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

std::vector<OnDeviceTranslationLanguagePackagePtr> GetLanguagePackages() {
  std::vector<OnDeviceTranslationLanguagePackagePtr> packages;
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(kTranslateKitPackagePaths)) {
    return GetLanguagePackagesFromCommnandLineString(
        command_line->GetSwitchValueNative(kTranslateKitPackagePaths));
  }
  // TODO(crbug.com/358030919): Check PrefService to get the path of the
  // installed language packages.
  return std::vector<OnDeviceTranslationLanguagePackagePtr>();
}

OnDeviceTranslationServiceConfigPtr CreateConfig() {
  OnDeviceTranslationServiceConfigPtr config =
      OnDeviceTranslationServiceConfig::New();
  config->packages = GetLanguagePackages();
  return config;
}

}  // namespace

OnDeviceTranslationServiceController::OnDeviceTranslationServiceController() {
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
  service_remote_->SetServiceConfig(CreateConfig());
}

OnDeviceTranslationServiceController::~OnDeviceTranslationServiceController() =
    default;

void OnDeviceTranslationServiceController::CreateTranslator(
    const std::string& source_lang,
    const std::string& target_lang,
    mojo::PendingReceiver<on_device_translation::mojom::Translator> receiver,
    base::OnceCallback<void(bool)> callback) {
  service_remote_->CreateTranslator(source_lang, target_lang,
                                    std::move(receiver), std::move(callback));
}

void OnDeviceTranslationServiceController::CanTranslate(
    const std::string& source_lang,
    const std::string& target_lang,
    base::OnceCallback<void(bool)> callback) {
  service_remote_->CanTranslate(source_lang, target_lang, std::move(callback));
}

// static
OnDeviceTranslationServiceController*
OnDeviceTranslationServiceController::GetInstance() {
  static base::NoDestructor<OnDeviceTranslationServiceController> instance;
  return instance.get();
}
