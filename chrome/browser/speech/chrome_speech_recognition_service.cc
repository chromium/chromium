// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/speech/chrome_speech_recognition_service.h"

#include <string>
#include <unordered_set>

#include "base/containers/flat_map.h"
#include "base/metrics/histogram_functions.h"
#include "base/scoped_observation.h"
#include "base/strings/string_util.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/component_updater/soda_language_pack_component_installer.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/speech/on_device_live_caption_recognizer.h"
#include "chrome/grit/generated_resources.h"
#include "components/live_caption/pref_names.h"
#include "components/optimization_guide/core/model_execution/model_broker_client.h"
#include "components/optimization_guide/public/mojom/model_broker.mojom.h"
#include "components/prefs/pref_service.h"
#include "components/soda/constants.h"
#include "components/soda/soda_installer.h"
#include "components/user_prefs/user_prefs.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/service_process_host.h"
#include "media/base/media_switches.h"
#include "media/mojo/mojom/speech_recognition_service.mojom.h"
#include "mojo/public/cpp/bindings/callback_helpers.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "services/on_device_model/public/mojom/on_device_model.mojom.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "ash/constants/ash_features.h"
#endif  // BUILDFLAG(IS_CHROMEOS)

namespace speech {

constexpr base::TimeDelta kIdleProcessTimeout = base::Seconds(5);

ChromeSpeechRecognitionService::ChromeSpeechRecognitionService(
    content::BrowserContext* context)
    : context_(context) {
#if BUILDFLAG(IS_CHROMEOS)
  if (!base::FeatureList::IsEnabled(
          ash::features::kOnDeviceSpeechRecognition)) {
    return;
  }
#endif  // BUILDFLAG(IS_CHROMEOS)

  auto* soda_installer = speech::SodaInstaller::GetInstance();

  // The SodaInstaller might not exist in unit tests.
  if (soda_installer) {
    soda_installer_observer_.Observe(soda_installer);
  }

  if (base::FeatureList::IsEnabled(
          media::kLiveCaptionSpeechRecognitionSmallExpertModel)) {
    auto* model_installer =
        g_browser_process
            ? g_browser_process
                  ->speech_recognition_small_expert_model_installer()
            : nullptr;
    if (model_installer) {
      speech_recognition_small_expert_model_installer_observer_.Observe(
          model_installer);
    }
  }
}

ChromeSpeechRecognitionService::~ChromeSpeechRecognitionService() = default;

void ChromeSpeechRecognitionService::BindSpeechRecognitionContext(
    mojo::PendingReceiver<media::mojom::SpeechRecognitionContext> receiver) {
  speech_recognition_contexts_.Add(this, std::move(receiver));
}

void ChromeSpeechRecognitionService::BindRecognizer(
    mojo::PendingReceiver<media::mojom::SpeechRecognitionRecognizer> receiver,
    mojo::PendingRemote<media::mojom::SpeechRecognitionRecognizerClient> client,
    media::mojom::SpeechRecognitionOptionsPtr options,
    BindRecognizerCallback callback) {
  std::string language_name;
  if (options && options->language.has_value()) {
    language_name = options->language.value();
  } else {
    PrefService* profile_prefs = user_prefs::UserPrefs::Get(context_);
    if (profile_prefs) {
      language_name = prefs::GetLiveCaptionLanguageCode(profile_prefs);
    }
  }
  if (options && !options->language.has_value()) {
    options->language = language_name;
  }

  if (base::FeatureList::IsEnabled(
          media::kLiveCaptionSpeechRecognitionSmallExpertModel) &&
      options &&
      options->recognizer_client_type ==
          media::mojom::RecognizerClientType::kLiveCaption &&
      !options->is_server_based) {
    auto* installer =
        g_browser_process
            ? g_browser_process
                  ->speech_recognition_small_expert_model_installer()
            : nullptr;
    if (installer &&
        installer->IsSpeechRecognitionSmallExpertModelInstalled()) {
      Profile* profile = Profile::FromBrowserContext(context_);
      if (profile && !model_broker_client_) {
        auto* opt_guide_service =
            OptimizationGuideKeyedServiceFactory::GetForProfile(profile);
        if (opt_guide_service) {
          model_broker_client_ = opt_guide_service->CreateModelBrokerClient();
        }
      }
      if (model_broker_client_) {
        auto feature = optimization_guide::mojom::OnDeviceFeature::
            kSpeechRecognitionSmallExpertModel;
        model_broker_client_->RequestAssetsFor(feature);
        model_broker_client_->GetSubscriber(feature).WaitForClient(
            base::BindOnce(
                &ChromeSpeechRecognitionService::OnModelClientAvailable,
                weak_factory_.GetWeakPtr(), std::move(receiver),
                std::move(client), std::move(options),
                mojo::WrapCallbackWithDefaultInvokeIfNotRun(std::move(callback),
                                                            false)));
        return;
      }
    }
  }

  BindSodaRecognizer(std::move(receiver), std::move(client), std::move(options),
                     std::move(callback));
}

void ChromeSpeechRecognitionService::BindWebSpeechRecognizer(
    mojo::PendingReceiver<media::mojom::SpeechRecognitionSession>
        session_receiver,
    mojo::PendingRemote<media::mojom::SpeechRecognitionSessionClient>
        session_client,
    mojo::PendingReceiver<media::mojom::SpeechRecognitionAudioForwarder>
        audio_forwarder,
    int channel_count,
    int sample_rate,
    media::mojom::SpeechRecognitionOptionsPtr options,
    bool continuous) {
  LaunchIfNotRunning();
  if (speech_recognition_service_.is_bound()) {
    if (!speech_recognition_context_remote_.is_bound() ||
        !speech_recognition_context_remote_.is_connected()) {
      speech_recognition_context_remote_.reset();
      speech_recognition_service_->BindSpeechRecognitionContext(
          speech_recognition_context_remote_.BindNewPipeAndPassReceiver());
      speech_recognition_context_remote_.reset_on_disconnect();
    }
    speech_recognition_context_remote_->BindWebSpeechRecognizer(
        std::move(session_receiver), std::move(session_client),
        std::move(audio_forwarder), channel_count, sample_rate,
        std::move(options), continuous);
  }
}

void ChromeSpeechRecognitionService::OnModelClientAvailable(
    mojo::PendingReceiver<media::mojom::SpeechRecognitionRecognizer> receiver,
    mojo::PendingRemote<media::mojom::SpeechRecognitionRecognizerClient> client,
    media::mojom::SpeechRecognitionOptionsPtr options,
    BindRecognizerCallback callback,
    base::WeakPtr<optimization_guide::ModelClient> model_client) {
  if (!model_client) {
    LOG(WARNING)
        << "SpeechRecognitionSmallExpertModel feature is enabled, but model "
           "client is unavailable.";
    std::move(callback).Run(false);
    return;
  }

  auto params = on_device_model::mojom::SessionParams::New();
  params->capabilities.Put(on_device_model::CapabilityFlags::kAudioInput);
  mojo::Remote<on_device_model::mojom::Session> session;
  model_client->solution().CreateSession(session.BindNewPipeAndPassReceiver(),
                                         std::move(params));

  auto asr_options = on_device_model::mojom::AsrStreamOptions::New();
  asr_options->sample_rate_hz = 16000;
  asr_options->language = options->language;

  mojo::PendingRemote<on_device_model::mojom::AsrStreamInput> asr_stream_input;
  mojo::PendingReceiver<on_device_model::mojom::AsrStreamResponder>
      asr_stream_responder;

  session->AsrStream(std::move(asr_options),
                     asr_stream_input.InitWithNewPipeAndPassReceiver(),
                     asr_stream_responder.InitWithNewPipeAndPassRemote());

  bool mask_offensive_words = true;
  PrefService* profile_prefs = user_prefs::UserPrefs::Get(context_);
  if (profile_prefs) {
    mask_offensive_words =
        profile_prefs->GetBoolean(prefs::kLiveCaptionMaskOffensiveWords);
  }

  on_device_recognizers_.Add(
      std::make_unique<OnDeviceLiveCaptionRecognizer>(
          std::move(client), std::move(options), std::move(session),
          std::move(asr_stream_input), std::move(asr_stream_responder),
          mask_offensive_words),
      std::move(receiver));

  std::move(callback).Run(/*is_multichannel_supported=*/false);
}

void ChromeSpeechRecognitionService::BindSodaRecognizer(
    mojo::PendingReceiver<media::mojom::SpeechRecognitionRecognizer> receiver,
    mojo::PendingRemote<media::mojom::SpeechRecognitionRecognizerClient> client,
    media::mojom::SpeechRecognitionOptionsPtr options,
    BindRecognizerCallback callback) {
  LaunchIfNotRunning();
  if (speech_recognition_service_.is_bound()) {
    if (!speech_recognition_context_remote_.is_bound() ||
        !speech_recognition_context_remote_.is_connected()) {
      speech_recognition_context_remote_.reset();
      speech_recognition_service_->BindSpeechRecognitionContext(
          speech_recognition_context_remote_.BindNewPipeAndPassReceiver());
      speech_recognition_context_remote_.reset_on_disconnect();
    }

    speech_recognition_context_remote_->BindRecognizer(
        std::move(receiver), std::move(client), std::move(options),
        std::move(callback));
    return;
  }

  std::move(callback).Run(/*is_multichannel_supported=*/false);
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
    speech::LanguageCode /*language_code*/) {
  if (speech_recognition_service_.is_bound()) {
    speech_recognition_service_->SetSodaConfigPaths(
        ChromeSpeechRecognitionService::GetSodaConfigPaths());
  }
}

void ChromeSpeechRecognitionService::OnSodaInstallError(
    speech::LanguageCode /*language_code*/,
    speech::SodaInstaller::ErrorCode /*error_code*/) {}

void ChromeSpeechRecognitionService::OnSodaProgress(
    speech::LanguageCode /*language_code*/,
    int /*progress*/) {}

void ChromeSpeechRecognitionService::LaunchIfNotRunning() {
  if (speech_recognition_service_.is_bound()) {
    return;
  }

  speech_recognition_context_remote_.reset();

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
  if (speech_recognition_service_.is_bound()) {
    speech_recognition_service_->SetSodaPaths(binary_path, config_paths,
                                              language_name);

    bool mask_offensive_words =
        profile_prefs->GetBoolean(prefs::kLiveCaptionMaskOffensiveWords);
    speech_recognition_service_->SetSodaParams(mask_offensive_words);
  }
}

base::flat_map<std::string, base::FilePath>
ChromeSpeechRecognitionService::GetSodaConfigPaths() {
  base::flat_map<std::string, base::FilePath> config_file_paths;
  std::unordered_set<std::string_view> registered_language_packs;
  for (const auto& language : g_browser_process->local_state()->GetList(
           prefs::kSodaRegisteredLanguagePacks)) {
    registered_language_packs.insert(language.GetString());
  }

  for (const SodaLanguagePackComponentConfig& config :
       kLanguageComponentConfigs) {
    base::FilePath config_path =
        g_browser_process->local_state()->GetFilePath(config.config_path_pref);

    if (!config_path.empty() &&
        registered_language_packs.contains(config.language_name)) {
      config_file_paths[config.language_name] = config_path;
    }
  }

  return config_file_paths;
}

void ChromeSpeechRecognitionService::CloseOnDeviceModelServiceHandles() {
  weak_factory_.InvalidateWeakPtrs();
  on_device_recognizers_.Clear();
  model_broker_client_.reset();
}

void ChromeSpeechRecognitionService::
    OnSpeechRecognitionSmallExpertModelInstalled() {}

void ChromeSpeechRecognitionService::
    OnSpeechRecognitionSmallExpertModelStateChanged(
        speech::SpeechRecognitionSmallExpertModelInstaller::
            SpeechRecognitionSmallExpertModelState state) {
  using State = speech::SpeechRecognitionSmallExpertModelInstaller::
      SpeechRecognitionSmallExpertModelState;
  if (state == State::kNotInstalled || state == State::kError ||
      state == State::kErrorCorruptPersistent) {
    CloseOnDeviceModelServiceHandles();
  }
}

}  // namespace speech
