// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/speech/chrome_speech_recognition_service.h"

#include "chrome/browser/browser_process.h"
#include "chrome/browser/component_updater/soda_language_pack_component_installer.h"
#include "chrome/browser/service_sandbox_type.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/generated_resources.h"
#include "components/prefs/pref_service.h"
#include "components/soda/constants.h"
#include "components/user_prefs/user_prefs.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/service_process_host.h"
#include "content/public/browser/storage_partition.h"
#include "services/network/network_context.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace speech {

constexpr base::TimeDelta kIdleProcessTimeout = base::TimeDelta::FromSeconds(5);

ChromeSpeechRecognitionService::ChromeSpeechRecognitionService(
    content::BrowserContext* context)
    : context_(context),
      enable_soda_(
          base::FeatureList::IsEnabled(media::kUseSodaForLiveCaption)) {}

ChromeSpeechRecognitionService::~ChromeSpeechRecognitionService() = default;

void ChromeSpeechRecognitionService::Create(
    mojo::PendingReceiver<media::mojom::SpeechRecognitionContext> receiver) {
  LaunchIfNotRunning();

  if (speech_recognition_service_.is_bound())
    speech_recognition_service_->BindContext(std::move(receiver));
}

void ChromeSpeechRecognitionService::OnNetworkServiceDisconnect() {
  if (!enable_soda_) {
    // If the Speech On-Device API
    // is not enabled, pass the URL
    // loader factory to
    // the speech recognition service to allow network requests to the Open
    // Speech API.
    mojo::PendingRemote<network::mojom::URLLoaderFactory> url_loader_factory;
    network::mojom::URLLoaderFactoryParamsPtr params =
        network::mojom::URLLoaderFactoryParams::New();
    params->process_id = network::mojom::kBrowserProcessId;
    params->is_trusted = false;
    params->automatically_assign_isolation_info = true;
    network::mojom::NetworkContext* network_context =
        content::BrowserContext::GetDefaultStoragePartition(context_)
            ->GetNetworkContext();
    network_context->CreateURLLoaderFactory(
        url_loader_factory.InitWithNewPipeAndPassReceiver(), std::move(params));
    speech_recognition_service_->SetUrlLoaderFactory(
        std::move(url_loader_factory));
  }
}

void ChromeSpeechRecognitionService::LaunchIfNotRunning() {
  if (speech_recognition_service_.is_bound())
    return;

  PrefService* profile_prefs = user_prefs::UserPrefs::Get(context_);
  PrefService* global_prefs = g_browser_process->local_state();
  DCHECK(profile_prefs);
  DCHECK(global_prefs);

  if (!profile_prefs->GetBoolean(prefs::kLiveCaptionEnabled))
    return;

  auto binary_path = global_prefs->GetFilePath(prefs::kSodaBinaryPath);
  auto config_path =
      ChromeSpeechRecognitionService::GetSodaConfigPath(profile_prefs);
  if (enable_soda_ && (binary_path.empty() || config_path.empty())) {
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

  speech_recognition_service_client_.reset();

  if (enable_soda_) {
    speech_recognition_service_->SetSodaPath(binary_path, config_path);
  }

  speech_recognition_service_->BindSpeechRecognitionServiceClient(
      speech_recognition_service_client_.BindNewPipeAndPassRemote());
  OnNetworkServiceDisconnect();
}

base::FilePath ChromeSpeechRecognitionService::GetSodaConfigPath(
    PrefService* prefs) {
  base::Optional<speech::SodaLanguagePackComponentConfig> language_config =
      speech::GetLanguageComponentConfig(
          prefs->GetString(prefs::kLiveCaptionLanguageCode));

  if (language_config) {
    return g_browser_process->local_state()->GetFilePath(
        language_config.value().config_path_pref);
  }
  return base::FilePath();
}
}  // namespace speech
