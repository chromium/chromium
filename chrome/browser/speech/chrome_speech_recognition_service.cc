// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/speech/chrome_speech_recognition_service.h"

#include <string>
#include <unordered_set>

#include "base/containers/contains.h"
#include "base/containers/flat_map.h"
#include "base/metrics/histogram_functions.h"
#include "base/scoped_observation.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/component_updater/soda_language_pack_component_installer.h"
#include "chrome/grit/generated_resources.h"
#include "components/live_caption/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/soda/constants.h"
#include "components/soda/soda_installer.h"
#include "components/user_prefs/user_prefs.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/service_process_host.h"
#include "media/base/media_switches.h"
#include "media/mojo/mojom/speech_recognition_service.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/constants/ash_features.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

namespace speech {

constexpr base::TimeDelta kIdleProcessTimeout = base::Seconds(5);

ChromeSpeechRecognitionService::ChromeSpeechRecognitionService(
    content::BrowserContext* context)
    : context_(context) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (!base::FeatureList::IsEnabled(
          ash::features::kOnDeviceSpeechRecognition)) {
    return;
  }
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  auto* soda_installer = speech::SodaInstaller::GetInstance();

  // The SodaInstaller might not exist in unit tests.
  if (soda_installer) {
    soda_installer_observer_.Observe(soda_installer);
  }
}

ChromeSpeechRecognitionService::~ChromeSpeechRecognitionService() = default;

void ChromeSpeechRecognitionService::BindSpeechRecognitionContext(
    mojo::PendingReceiver<media::mojom::SpeechRecognitionContext> receiver) {
  LaunchIfNotRunning();

  if (speech_recognition_service_.is_bound()) {
    speech_recognition_service_->BindSpeechRecognitionContext(
        std::move(receiver));
  }
}

void ChromeSpeechRecognitionService::BindAudioSourceSpeechRecognitionContext(
    mojo::PendingReceiver<media::mojom::AudioSourceSpeechRecognitionContext>
        receiver) {
  LaunchIfNotRunning();

  if (speech_recognition_service_.is_bound()) {
    speech_recognition_service_->BindAudioSourceSpeechRecognitionContext(
        std::move(receiver));
  }
}

void ChromeSpeechRecognitionService::OnSodaInstalled(
    speech::LanguageCode language_code) {
  if (speech_recognition_service_.is_bound()) {
    speech_recognition_service_->SetSodaConfigPaths(
        ChromeSpeechRecognitionService::GetSodaConfigPaths());
  }
}

void ChromeSpeechRecognitionService::OnSodaInstallError(
    speech::LanguageCode language_code,
    speech::SodaInstaller::ErrorCode error_code) {}

void ChromeSpeechRecognitionService::OnSodaProgress(
    speech::LanguageCode language_code,
    int progress) {}

void ChromeSpeechRecognitionService::LaunchIfNotRunning() {
  if (speech_recognition_service_.is_bound()) {
    return;
  }

  PrefService* profile_prefs = user_prefs::UserPrefs::Get(context_);
  PrefService* global_prefs = g_browser_process->local_state();
  DCHECK(profile_prefs);
  DCHECK(global_prefs);

  // TODO(crbug.com/40162502): Language pack path should be configurable per
  // SpeechRecognitionRecognizer to allow multiple features to use Speech
  // recognition. For now, only Live Caption uses SpeechRecognitionService on
  // non-Chrome OS Chrome, so hard-coding to the Live Caption language code.
  const std::string language_name =
      prefs::GetLiveCaptionLanguageCode(profile_prefs);

  std::optional<speech::SodaLanguagePackComponentConfig> language_config =
      speech::GetLanguageComponentConfig(language_name);
  CHECK(language_config);
  base::UmaHistogramEnumeration("Accessibility.LiveCaption.SodaLanguage",
                                language_config.value().language_code);

  base::FilePath binary_path;
  binary_path = global_prefs->GetFilePath(prefs::kSodaBinaryPath);
  base::flat_map<std::string, base::FilePath> config_paths =
      ChromeSpeechRecognitionService::GetSodaConfigPaths();

  if (binary_path.empty() || config_paths[language_name].empty()) {
    LOG(ERROR) << "Unable to find SODA files on the device.";
    return;
  }

  content::ServiceProcessHost::Launch(
      speech_recognition_service_.BindNewPipeAndPassReceiver(),
      content::ServiceProcessHost::Options()
          .WithDisplayName(IDS_UTILITY_PROCESS_SPEECH_RECOGNITION_SERVICE_NAME)
          .Pass());

  // Ensure that if the interface is ever disconnected (e.g. the service
  // process crashes) or goes idle for a short period of time -- meaning there
  // are no in-flight messages and no other interfaces bound through this
  // one -- then we will reset |remote|, causing the service process to be
  // terminated if it isn't already.
  speech_recognition_service_.reset_on_disconnect();
  speech_recognition_service_.reset_on_idle_timeout(kIdleProcessTimeout);
  speech_recognition_service_->SetSodaPaths(binary_path, config_paths,
                                            language_name);

  bool mask_offensive_words =
      profile_prefs->GetBoolean(prefs::kLiveCaptionMaskOffensiveWords);
  speech_recognition_service_->SetSodaParams(mask_offensive_words);
}

base::flat_map<std::string, base::FilePath>
ChromeSpeechRecognitionService::GetSodaConfigPaths() {
  base::flat_map<std::string, base::FilePath> config_file_paths;
  std::unordered_set<std::string> registered_language_packs;
  for (const auto& language : g_browser_process->local_state()->GetList(
           prefs::kSodaRegisteredLanguagePacks)) {
    registered_language_packs.insert(language.GetString());
  }

  for (const SodaLanguagePackComponentConfig& config :
       kLanguageComponentConfigs) {
    base::FilePath config_path =
        g_browser_process->local_state()->GetFilePath(config.config_path_pref);

    if (!config_path.empty() &&
        base::Contains(registered_language_packs, config.language_name)) {
      config_file_paths[config.language_name] = config_path;
    }
  }

  return config_file_paths;
}
}  // namespace speech
