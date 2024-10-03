// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "ash/webui/recorder_app_ui/recorder_app_ui.h"

#include "ash/constants/ash_features.h"
#include "ash/webui/common/trusted_types_util.h"
#include "ash/webui/metrics/structured_metrics_service_wrapper.h"
#include "ash/webui/recorder_app_ui/recorder_app_ui_delegate.h"
#include "ash/webui/recorder_app_ui/resources.h"
#include "ash/webui/recorder_app_ui/resources/grit/recorder_app_resources.h"
#include "ash/webui/recorder_app_ui/resources/grit/recorder_app_resources_map.h"
#include "ash/webui/recorder_app_ui/url_constants.h"
#include "base/ranges/algorithm.h"
#include "chromeos/ash/components/audio/cras_audio_handler.h"
#include "chromeos/ash/components/mojo_service_manager/connection.h"
#include "chromeos/services/machine_learning/public/cpp/service_connection.h"
#include "components/media_device_salt/media_device_salt_service.h"
#include "components/soda/soda_features.h"
#include "components/soda/soda_installer.h"
#include "components/soda/soda_util.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/media_device_id.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/common/url_constants.h"
#include "google_apis/google_api_keys.h"
#include "services/on_device_model/public/cpp/buildflags.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "third_party/cros_system_api/mojo/service_constants.h"
#include "ui/message_center/message_center.h"
#include "ui/webui/webui_allowlist.h"

namespace ash {

namespace {

// TODO(pihsun): Handle multiple languages.
constexpr speech::LanguageCode kLanguageCode = speech::LanguageCode::kEnUs;

std::string_view SodaInstallerErrorCodeToString(
    speech::SodaInstaller::ErrorCode error) {
  switch (error) {
    case speech::SodaInstaller::ErrorCode::kNeedsReboot:
      return "kNeedsReboot";
    case speech::SodaInstaller::ErrorCode::kUnspecifiedError:
      return "kUnspecifiedError";
  }
}

void GotSalt(
    const url::Origin& origin,
    const std::string& source_id,
    base::OnceCallback<void(const std::optional<std::string>&)> callback,
    const std::string& salt) {
  // TODO(kamchonlathorn): Add a test to cover this function.
  auto callback_on_io_thread = base::BindOnce(
      [](const std::string& salt, const url::Origin& origin,
         const std::string& source_id,
         base::OnceCallback<void(const std::optional<std::string>&)> callback) {
        content::GetMediaDeviceIDForHMAC(
            blink::mojom::MediaStreamType::DEVICE_AUDIO_CAPTURE, salt,
            std::move(origin), source_id, content::GetUIThreadTaskRunner({}),
            std::move(callback));
      },
      salt, std::move(origin), source_id, std::move(callback));
  content::GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE, std::move(callback_on_io_thread));
}

void TranslateAudioDeviceId(
    content::BrowserContext* browser_context,
    media_device_salt::MediaDeviceSaltService* salt_service,
    const url::Origin& origin,
    const std::string& source_id,
    base::OnceCallback<void(const std::optional<std::string>&)> callback) {
  if (salt_service) {
    salt_service->GetSalt(
        blink::StorageKey::CreateFirstParty(origin),
        base::BindOnce(&GotSalt, origin, source_id, std::move(callback)));
  } else {
    // If the embedder does not provide a salt service, use the browser
    // context's unique ID as salt.
    GotSalt(origin, source_id, std::move(callback),
            browser_context->UniqueId());
  }
}

int GetResourceIdFromStringName(const std::string& name) {
  auto iter = base::ranges::find(
      kLocalizedStrings, name,
      [](const webui::LocalizedString& s) { return s.name; });
  CHECK(iter != std::end(kLocalizedStrings));
  return iter->id;
}

}  // namespace

bool RecorderAppUIConfig::IsWebUIEnabled(
    content::BrowserContext* browser_context) {
  return base::FeatureList::IsEnabled(ash::features::kConch) ||
         base::FeatureList::IsEnabled(
             speech::kFeatureManagementCrosSodaConchLanguages);
}

RecorderAppUI::RecorderAppUI(content::WebUI* web_ui,
                             std::unique_ptr<RecorderAppUIDelegate> delegate)
    : ui::MojoWebUIController(web_ui), delegate_(std::move(delegate)) {
  content::BrowserContext* browser_context =
      web_ui->GetWebContents()->GetBrowserContext();

  // Register auto-granted permissions.
  auto* allowlist = WebUIAllowlist::GetOrCreate(browser_context);
  const url::Origin host_origin =
      url::Origin::Create(GURL(kChromeUIRecorderAppURL));
  allowlist->RegisterAutoGrantedPermission(
      host_origin, ContentSettingsType::MEDIASTREAM_MIC);
  allowlist->RegisterAutoGrantedPermission(
      host_origin, ContentSettingsType::DISPLAY_MEDIA_SYSTEM_AUDIO);

  // Setup the data source
  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      web_ui->GetWebContents()->GetBrowserContext(), kChromeUIRecorderAppHost);

  source->AddResourcePaths(
      base::make_span(kRecorderAppResources, kRecorderAppResourcesSize));

  source->AddResourcePath("", IDR_RECORDER_APP_INDEX_HTML);

  source->AddLocalizedStrings(kLocalizedStrings);

  source->UseStringsJs();

  ash::EnableTrustedTypesCSP(source);
  // TODO(pihsun): Add other needed CSP.
  source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::MediaSrc,
      std::string("media-src 'self' blob:;"));

  if (speech::IsOnDeviceSpeechRecognitionSupported()) {
    auto* soda_installer = speech::SodaInstaller::GetInstance();
    speech::SodaInstaller::GetInstance()->AddObserver(this);
    if (soda_installer->IsSodaInstalled(kLanguageCode)) {
      soda_state_ = {recorder_app::mojom::ModelStateType::kInstalled,
                     std::nullopt};
    } else {
      soda_state_ = {recorder_app::mojom::ModelStateType::kNotInstalled,
                     std::nullopt};
    }
  } else {
    soda_state_ = {recorder_app::mojom::ModelStateType::kUnavailable,
                   std::nullopt};
  }

  // Add salt translator
  device_id_mapping_callback_ =
      base::BindRepeating(&TranslateAudioDeviceId, browser_context,
                          delegate_->GetMediaDeviceSaltService(browser_context),
                          url::Origin::Create(GURL(kChromeUIRecorderAppURL)));

  auto* message_center = message_center::MessageCenter::Get();
  message_center->AddObserver(this);
  in_quiet_mode_ = message_center->IsQuietMode();
}

RecorderAppUI::~RecorderAppUI() {
  message_center::MessageCenter::Get()->RemoveObserver(this);

  if (speech::IsOnDeviceSpeechRecognitionSupported()) {
    speech::SodaInstaller::GetInstance()->RemoveObserver(this);
  }
}

void RecorderAppUI::BindInterface(
    mojo::PendingReceiver<color_change_listener::mojom::PageHandler> receiver) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  color_provider_handler_ = std::make_unique<ui::ColorChangeHandler>(
      web_ui()->GetWebContents(), std::move(receiver));
}

void RecorderAppUI::BindInterface(
    mojo::PendingReceiver<recorder_app::mojom::PageHandler> receiver) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  page_receivers_.Add(this, std::move(receiver));
}

void RecorderAppUI::BindInterface(
    mojo::PendingReceiver<crosapi::mojom::StructuredMetricsService> receiver) {
  structured_metrics_service_wrapper_ =
      std::make_unique<ash::StructuredMetricsServiceWrapper>();
  structured_metrics_service_wrapper_->BindReceiver(std::move(receiver));
}

void RecorderAppUI::EnsureOnDeviceModelService() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

#if BUILDFLAG(USE_CHROMEOS_MODEL_SERVICE)
  if (!on_device_model_service_) {
    auto pipe_handle =
        on_device_model_service_.BindNewPipeAndPassReceiver().PassPipe();
    on_device_model_service_.reset_on_disconnect();
    ash::mojo_service_manager::GetServiceManagerProxy()->Request(
        chromeos::mojo_services::kCrosOdmlService, std::nullopt,
        std::move(pipe_handle));
  }
#endif
}

void RecorderAppUI::AddModelMonitor(
    const base::Uuid& model_id,
    ::mojo::PendingRemote<recorder_app::mojom::ModelStateMonitor> monitor,
    AddModelMonitorCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!delegate_->CanUseGenerativeAiForCurrentProfile()) {
    // TODO(pihsun): Return a dedicate error when GenAI can't be used.
    std::move(callback).Run(recorder_app::mojom::ModelState{
        recorder_app::mojom::ModelStateType::kUnavailable, std::nullopt}
                                .Clone());
    return;
  }

  EnsureOnDeviceModelService();

  if (!on_device_model_service_) {
    std::move(callback).Run(recorder_app::mojom::ModelState{
        recorder_app::mojom::ModelStateType::kUnavailable, std::nullopt}
                                .Clone());
    return;
  }

  recorder_app::mojom::ModelState model_state;

  auto model_state_iter = model_states_.find(model_id);
  if (model_state_iter == model_states_.end()) {
    model_state = {recorder_app::mojom::ModelStateType::kUnavailable,
                   std::nullopt};
    model_states_.insert({model_id, model_state});
    // TODO(pihsun): This currently only gets the model state once on first
    // monitor, and updates with progress when LoadModel is called. So if the
    // model is installed by any other mean (like downloaded as a base model or
    // by other app), the state won't be updated. Consider if we should expose
    // the observer API from DLC for each individual model.
    on_device_model_service_->GetPlatformModelState(
        model_id, base::BindOnce(&RecorderAppUI::GetPlatformModelStateCallback,
                                 weak_ptr_factory_.GetWeakPtr(), model_id));
  } else {
    model_state = model_state_iter->second;
  }
  model_monitors_[model_id].Add(std::move(monitor));
  std::move(callback).Run(model_state.Clone());
}

void RecorderAppUI::AddQuietModeMonitor(
    ::mojo::PendingRemote<recorder_app::mojom::QuietModeMonitor> monitor,
    AddQuietModeMonitorCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  quiet_mode_monitors_.Add(std::move(monitor));
  std::move(callback).Run(in_quiet_mode_);
}

void RecorderAppUI::LoadModel(
    const base::Uuid& model_id,
    mojo::PendingReceiver<on_device_model::mojom::OnDeviceModel> model,
    LoadModelCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!delegate_->CanUseGenerativeAiForCurrentProfile()) {
    // TODO(pihsun): Return a dedicate error when GenAI can't be used.
    std::move(callback).Run(
        on_device_model::mojom::LoadModelResult::kFailedToLoadLibrary);
    return;
  }

  EnsureOnDeviceModelService();

  if (!on_device_model_service_) {
    std::move(callback).Run(
        on_device_model::mojom::LoadModelResult::kFailedToLoadLibrary);
  }

  mojo::PendingReceiver<on_device_model::mojom::PlatformModelProgressObserver>
      progress_receiver;

  on_device_model_service_->LoadPlatformModel(
      model_id, std::move(model),
      progress_receiver.InitWithNewPipeAndPassRemote(), std::move(callback));

  model_progress_receivers_.Add(this, std::move(progress_receiver), model_id);

  // The first callback from the progress callback of LoadPlatformModel is
  // often very slow, so we do optimistic update here and show the installing
  // state for the model. Note that if the model is already installed,
  // UpdateModelState would prevent it from going back to installing.
  UpdateModelState(model_id,
                   {recorder_app::mojom::ModelStateType::kInstalling, 0});
}

void RecorderAppUI::FormatModelInput(
    const base::Uuid& model_id,
    on_device_model::mojom::FormatFeature feature,
    const base::flat_map<std::string, std::string>& fields,
    FormatModelInputCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  DCHECK(delegate_->CanUseGenerativeAiForCurrentProfile());

  EnsureOnDeviceModelService();

  if (!on_device_model_service_) {
    std::move(callback).Run(std::nullopt);
  }

  on_device_model_service_->FormatInput(model_id, feature, fields,
                                        std::move(callback));
}

void RecorderAppUI::ValidateSafetyResult(
    on_device_model::mojom::SafetyFeature safety_feature,
    const std::string& text,
    on_device_model::mojom::SafetyInfoPtr safety_info,
    ValidateSafetyResultCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  DCHECK(delegate_->CanUseGenerativeAiForCurrentProfile());

  EnsureOnDeviceModelService();

  if (!on_device_model_service_) {
    std::move(callback).Run(false);
  }

  on_device_model_service_->ValidateSafetyResult(
      safety_feature, text, std::move(safety_info), std::move(callback));
}

void RecorderAppUI::Progress(double progress) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto model_id = model_progress_receivers_.current_context();
  // The progress reported from ML service is in [0, 1], and use 1 as
  // installation done, but we want to report [0, 100].
  int scaled_progress = static_cast<int>(progress * 100);

  if (scaled_progress == 100) {
    UpdateModelState(model_id, {recorder_app::mojom::ModelStateType::kInstalled,
                                std::nullopt});
  } else {
    UpdateModelState(
        model_id,
        {recorder_app::mojom::ModelStateType::kInstalling, scaled_progress});
  }
}

void RecorderAppUI::GetPlatformModelStateCallback(
    const base::Uuid& model_id,
    on_device_model::mojom::PlatformModelState state) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  switch (state) {
    case on_device_model::mojom::PlatformModelState::kInstalledOnDisk:
      UpdateModelState(
          model_id,
          {recorder_app::mojom::ModelStateType::kInstalled, std::nullopt});
      break;
    case on_device_model::mojom::PlatformModelState::kInvalidDlcVerifiedState:
      // Not installed is classified as "not verified" in DLC.
      UpdateModelState(
          model_id,
          {recorder_app::mojom::ModelStateType::kNotInstalled, std::nullopt});
      break;
    case on_device_model::mojom::PlatformModelState::kInvalidDlcPackage:
    case on_device_model::mojom::PlatformModelState::kUnknownState:
    case on_device_model::mojom::PlatformModelState::kInvalidUuid:
    case on_device_model::mojom::PlatformModelState::kInvalidDlcClient:
    case on_device_model::mojom::PlatformModelState::kInvalidDlcInstall:
    case on_device_model::mojom::PlatformModelState::kInvalidModelFormat:
    case on_device_model::mojom::PlatformModelState::kInvalidModelDescriptor:
    case on_device_model::mojom::PlatformModelState::
        kInvalidBaseModelDescriptor:
      UpdateModelState(
          model_id,
          {recorder_app::mojom::ModelStateType::kUnavailable, std::nullopt});
      break;
  }
}

void RecorderAppUI::UpdateModelState(const base::Uuid& model_id,
                                     recorder_app::mojom::ModelState state) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (state.type == recorder_app::mojom::ModelStateType::kInstalling) {
    // On device model reports "loading" model and "download/installing" model
    // in a same way. To avoid confusing the user, don't go back to installing
    // state from installed state.
    // TODO: b/361718207 - Consider how to drop this workaround.
    auto old_state_iter = model_states_.find(model_id);
    if (old_state_iter != model_states_.end() &&
        old_state_iter->second.type ==
            recorder_app::mojom::ModelStateType::kInstalled) {
      return;
    }
  }

  for (const auto& monitor : model_monitors_[model_id]) {
    monitor->Update(state.Clone());
  }
  model_states_.insert_or_assign(model_id, state);
}

mojo::Remote<chromeos::machine_learning::mojom::MachineLearningService>&
RecorderAppUI::GetMlService() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!ml_service_) {
    chromeos::machine_learning::ServiceConnection::GetInstance()
        ->BindMachineLearningService(ml_service_.BindNewPipeAndPassReceiver());
  }
  return ml_service_;
}

void RecorderAppUI::AddSodaMonitor(
    ::mojo::PendingRemote<recorder_app::mojom::ModelStateMonitor> monitor,
    AddSodaMonitorCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  soda_monitors_.Add(std::move(monitor));
  std::move(callback).Run(soda_state_.Clone());
}

void RecorderAppUI::InstallSoda(InstallSodaCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (speech::IsOnDeviceSpeechRecognitionSupported()) {
    auto state = soda_state_.type;
    if (state == recorder_app::mojom::ModelStateType::kNotInstalled ||
        state == recorder_app::mojom::ModelStateType::kError) {
      // Update SODA state to installing so the UI will show downloading
      // immediately, since the DLC download might start later.
      UpdateSodaState({recorder_app::mojom::ModelStateType::kInstalling, 0});
      delegate_->InstallSoda(kLanguageCode);
    }
  }
  std::move(callback).Run();
}

void RecorderAppUI::UpdateSodaState(recorder_app::mojom::ModelState state) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  soda_state_ = state;
  for (auto& monitor : soda_monitors_) {
    monitor->Update(soda_state_.Clone());
  }
}

void RecorderAppUI::OnSodaInstallError(
    speech::LanguageCode language_code,
    speech::SodaInstaller::ErrorCode error_code) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (language_code != kLanguageCode) {
    return;
  }

  LOG(ERROR) << "Failed to install Soda library DLC with error "
             << SodaInstallerErrorCodeToString(error_code);
  UpdateSodaState({recorder_app::mojom::ModelStateType::kError, std::nullopt});
}

void RecorderAppUI::OnSodaProgress(speech::LanguageCode language_code,
                                   int progress) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (language_code != kLanguageCode) {
    return;
  }

  UpdateSodaState({recorder_app::mojom::ModelStateType::kInstalling, progress});
}

void RecorderAppUI::OnSodaInstalled(speech::LanguageCode language_code) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (language_code != kLanguageCode) {
    return;
  }

  UpdateSodaState(
      {recorder_app::mojom::ModelStateType::kInstalled, std::nullopt});
}

void RecorderAppUI::LoadSpeechRecognizer(
    mojo::PendingRemote<chromeos::machine_learning::mojom::SodaClient>
        soda_client,
    mojo::PendingReceiver<chromeos::machine_learning::mojom::SodaRecognizer>
        soda_recognizer,
    LoadSpeechRecognizerCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!speech::IsOnDeviceSpeechRecognitionSupported()) {
    // TODO(pihsun): Returns different error when soda is not available.
    std::move(callback).Run(false);
    return;
  }

  auto* soda_installer = speech::SodaInstaller::GetInstance();
  if (!soda_installer->IsSodaInstalled(kLanguageCode)) {
    // TODO(pihsun): Returns different error when soda is not installed.
    std::move(callback).Run(false);
    return;
  }

  auto soda_library_path = soda_installer->GetSodaBinaryPath();
  auto soda_language_path =
      soda_installer->GetLanguagePath(speech::GetLanguageName(kLanguageCode));
  CHECK(!soda_library_path.empty());
  CHECK(!soda_language_path.empty());

  auto config = chromeos::machine_learning::mojom::SodaConfig::New();
  config->channel_count = 1;
  config->sample_rate = 16000;
  config->api_key = google_apis::GetSodaAPIKey();
  config->language_dlc_path = soda_language_path.value();
  config->library_dlc_path = soda_library_path.value();
  config->enable_formatting =
      chromeos::machine_learning::mojom::OptionalBool::kTrue;
  // This forces to use the large model.
  config->recognition_mode =
      chromeos::machine_learning::mojom::SodaRecognitionMode::kIme;
  config->speaker_diarization_mode = chromeos::machine_learning::mojom::
      SpeakerDiarizationMode::kSpeakerLabelDetection;
  config->max_speaker_count = 7;
  config->mask_offensive_words = true;

  GetMlService()->LoadSpeechRecognizer(
      std::move(config), std::move(soda_client), std::move(soda_recognizer),
      base::BindOnce(
          [](LoadSpeechRecognizerCallback callback,
             chromeos::machine_learning::mojom::LoadModelResult result) {
            if (result ==
                chromeos::machine_learning::mojom::LoadModelResult::OK) {
              std::move(callback).Run(true);
            } else {
              LOG(ERROR) << "Could not load recognizer, error: " << result;
              std::move(callback).Run(false);
            }
          },
          std::move(callback)));
}

void RecorderAppUI::OpenAiFeedbackDialog(
    const std::string& description_template) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  delegate_->OpenAiFeedbackDialog(description_template);
}

void RecorderAppUI::GetMicrophoneInfo(const std::string& source_id,
                                      GetMicrophoneInfoCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  device_id_mapping_callback_.Run(
      source_id,
      base::BindOnce(&RecorderAppUI::GetMicrophoneInfoWithDeviceId,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void RecorderAppUI::GetMicrophoneInfoWithDeviceId(
    GetMicrophoneInfoCallback callback,
    const std::optional<std::string>& device_id_str) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  recorder_app::mojom::MicrophoneInfoPtr info = nullptr;
  uint64_t default_mic_id =
      CrasAudioHandler::Get()->GetPrimaryActiveInputNode();
  if (device_id_str.has_value()) {
    uint64_t device_id;
    if (base::StringToUint64(*device_id_str, &device_id)) {
      const AudioDevice* device =
          CrasAudioHandler::Get()->GetDeviceFromId(device_id);
      if (device != nullptr) {
        info = recorder_app::mojom::MicrophoneInfo::New();
        info->is_default = device_id == default_mic_id;
        info->is_internal = device->IsInternalMic();
      }
    }
  }
  std::move(callback).Run(std::move(info));
}

void RecorderAppUI::OnQuietModeChanged(bool in_quiet_mode) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  in_quiet_mode_ = in_quiet_mode;
  for (auto& monitor : quiet_mode_monitors_) {
    monitor->Update(in_quiet_mode_);
  }
}

void RecorderAppUI::SetQuietMode(bool quiet_mode) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  message_center::MessageCenter::Get()->SetQuietMode(quiet_mode);
}

void RecorderAppUI::CanUseSpeakerLabel(CanUseSpeakerLabelCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!base::FeatureList::IsEnabled(
          speech::kFeatureManagementCrosSodaConchLanguages)) {
    // Large SODA model (which supports speaker label) isn't available.
    std::move(callback).Run(false);
  } else {
    std::move(callback).Run(delegate_->CanUseSpeakerLabelForCurrentProfile());
  }
}

void RecorderAppUI::RecordSpeakerLabelConsent(
    bool consent_given,
    const std::vector<std::string>& consent_description_names,
    const std::string& consent_confirmation_name) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  sync_pb::UserConsentTypes::RecorderSpeakerLabelConsent consent;
  for (const auto& name : consent_description_names) {
    consent.add_description_grd_ids(GetResourceIdFromStringName(name));
  }
  consent.set_confirmation_grd_id(
      GetResourceIdFromStringName(consent_confirmation_name));
  consent.set_status(consent_given ? sync_pb::UserConsentTypes::GIVEN
                                   : sync_pb::UserConsentTypes::NOT_GIVEN);
  delegate_->RecordSpeakerLabelConsent(consent);
}

void RecorderAppUI::CanCaptureSystemAudioWithLoopback(
    CanCaptureSystemAudioWithLoopbackCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Disallow audio loopback when capture system audio from microphone.
  std::move(callback).Run(
      !base::FeatureList::IsEnabled(ash::features::kConchSystemAudioFromMic));
}

WEB_UI_CONTROLLER_TYPE_IMPL(RecorderAppUI)

}  // namespace ash
