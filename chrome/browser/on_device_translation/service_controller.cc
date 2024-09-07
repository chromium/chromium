// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/on_device_translation/service_controller.h"

#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/no_destructor.h"
#include "base/strings/strcat.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/on_device_translation/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/services/on_device_translation/public/cpp/features.h"
#include "components/services/on_device_translation/public/mojom/on_device_translation_service.mojom.h"
#include "components/services/on_device_translation/public/mojom/translator.mojom.h"
#include "content/public/browser/service_process_host.h"

namespace {

const char kOnDeviceTranslationServiceDisplayName[] =
    "On-device Translation Service";

std::string GetFilePathFromGlobalPrefs(std::string_view pref_name) {
  PrefService* global_prefs = g_browser_process->local_state();
  CHECK(global_prefs);

  base::FilePath path = global_prefs->GetFilePath(pref_name);
  // TODO(crbug.com/362123222): Get rid of conditional decoding.
#if BUILDFLAG(IS_WIN)
  std::string path_str = path.AsUTF8Unsafe();
#else
  std::string path_str = path.value();
#endif  // BUILDFLAG(IS_WIN)
  return path_str;
}

}  // namespace

OnDeviceTranslationServiceController::OnDeviceTranslationServiceController() {
  auto receiver = service_remote_.BindNewPipeAndPassReceiver();
  service_remote_.reset_on_disconnect();

  std::vector<std::string> extra_switches;
  if (base::FeatureList::IsEnabled(
          on_device_translation::kEnableTranslateKitComponent)) {
    std::string translate_kit_root_dir =
        GetFilePathFromGlobalPrefs(prefs::kTranslateKitRootDir);
    if (translate_kit_root_dir.empty()) {
      LOG(ERROR) << "Got an empty root dir for TranslateKit.";
    }
    std::string translate_kit_binary_path =
        GetFilePathFromGlobalPrefs(prefs::kTranslateKitBinaryPath);
    if (translate_kit_binary_path.empty()) {
      LOG(ERROR) << "Got an empty path to TranslateKit binary on the device.";
    }
    // TODO(crbug.com/362123222): Pass the path via mojom instead of cmd
    // switches.
    extra_switches.push_back(
        base::StrCat({on_device_translation::kTranslateKitRootDir, "=",
                      translate_kit_root_dir}));
    extra_switches.push_back(
        base::StrCat({on_device_translation::kTranslateKitBinaryPath, "=",
                      translate_kit_binary_path}));
  }

  content::ServiceProcessHost::Launch<
      on_device_translation::mojom::OnDeviceTranslationService>(
      std::move(receiver),
      content::ServiceProcessHost::Options()
          .WithDisplayName(kOnDeviceTranslationServiceDisplayName)
          .WithExtraCommandLineSwitches(extra_switches)
          .Pass());
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
