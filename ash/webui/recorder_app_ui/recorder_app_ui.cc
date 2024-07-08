// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/recorder_app_ui/recorder_app_ui.h"

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_switches.h"
#include "ash/webui/common/trusted_types_util.h"
#include "ash/webui/recorder_app_ui/recorder_app_ui_delegate.h"
#include "ash/webui/recorder_app_ui/resources.h"
#include "ash/webui/recorder_app_ui/resources/grit/recorder_app_resources.h"
#include "ash/webui/recorder_app_ui/resources/grit/recorder_app_resources_map.h"
#include "ash/webui/recorder_app_ui/url_constants.h"
#include "chromeos/services/machine_learning/public/cpp/service_connection.h"
#include "components/soda/soda_installer.h"
#include "components/soda/soda_util.h"
#include "content/public/browser/on_device_model_service_instance.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/common/url_constants.h"
#include "google_apis/google_api_keys.h"
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

}  // namespace

bool RecorderAppUIConfig::IsWebUIEnabled(
    content::BrowserContext* browser_context) {
  if (!base::FeatureList::IsEnabled(ash::features::kConch)) {
    return false;
  }

  return ash::switches::IsConchSecretKeyMatched();
}

RecorderAppUI::RecorderAppUI(content::WebUI* web_ui,
                             std::unique_ptr<RecorderAppUIDelegate> delegate)
    : ui::MojoWebUIController(web_ui), delegate_(std::move(delegate)) {
  // See go/cros-conch-key for the key
  // Add it to /etc/chrome_dev.conf:
  //  --conch-key="INSERT KEY HERE"
  //  --enable-features=Conch
  CHECK(ash::switches::IsConchSecretKeyMatched());

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

  // TODO(pihsun): See if there's a better way to handle client side
  // navigation.
  source->AddResourcePath("", IDR_STATIC_INDEX_HTML);
  source->AddResourcePath("playback", IDR_STATIC_INDEX_HTML);
  source->AddResourcePath("record", IDR_STATIC_INDEX_HTML);
  source->AddResourcePath("dev", IDR_STATIC_INDEX_HTML);

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
      soda_state_ = {recorder_app::mojom::SodaStateType::kInstalled,
                     std::nullopt};
    } else {
      soda_state_ = {recorder_app::mojom::SodaStateType::kNotInstalled,
                     std::nullopt};
    }
  } else {
    soda_state_ = {recorder_app::mojom::SodaStateType::kUnavailable,
                   std::nullopt};
  }
}

RecorderAppUI::~RecorderAppUI() {
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

on_device_model::mojom::OnDeviceModelService&
RecorderAppUI::GetOnDeviceModelService() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  const auto& remote = content::GetRemoteOnDeviceModelService();
  CHECK(remote);
  return *remote;
}

void RecorderAppUI::LoadModel(
    const base::Uuid& uuid,
    mojo::PendingReceiver<on_device_model::mojom::OnDeviceModel> model,
    LoadModelCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  GetOnDeviceModelService().LoadPlatformModel(uuid, std::move(model),
                                              std::move(callback));
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
    ::mojo::PendingRemote<recorder_app::mojom::SodaStateMonitor> monitor,
    AddSodaMonitorCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  soda_monitors_.Add(std::move(monitor));
  std::move(callback).Run(soda_state_.Clone());
}

void RecorderAppUI::InstallSoda(InstallSodaCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (speech::IsOnDeviceSpeechRecognitionSupported()) {
    auto state = soda_state_.type;
    if (state == recorder_app::mojom::SodaStateType::kNotInstalled ||
        state == recorder_app::mojom::SodaStateType::kError) {
      // Update SODA state to installing so the UI will show downloading
      // immediately, since the DLC download might start later.
      UpdateSodaState({recorder_app::mojom::SodaStateType::kInstalling, 0});
      delegate_->InstallSoda(kLanguageCode);
    }
  }
  std::move(callback).Run();
}

void RecorderAppUI::UpdateSodaState(recorder_app::mojom::SodaState state) {
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
  UpdateSodaState({recorder_app::mojom::SodaStateType::kError, std::nullopt});
}

void RecorderAppUI::OnSodaProgress(speech::LanguageCode language_code,
                                   int progress) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (language_code != kLanguageCode) {
    return;
  }

  UpdateSodaState({recorder_app::mojom::SodaStateType::kInstalling, progress});
}

void RecorderAppUI::OnSodaInstalled(speech::LanguageCode language_code) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (language_code != kLanguageCode) {
    return;
  }

  UpdateSodaState(
      {recorder_app::mojom::SodaStateType::kInstalled, std::nullopt});
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

WEB_UI_CONTROLLER_TYPE_IMPL(RecorderAppUI)

}  // namespace ash
