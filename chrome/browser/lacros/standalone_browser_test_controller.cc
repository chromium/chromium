// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lacros/standalone_browser_test_controller.h"

#include <memory>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/extensions/extension_keeplist_chromeos.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/speech/tts_crosapi_util.h"
#include "chrome/browser/web_applications/user_display_mode.h"
#include "chrome/browser/web_applications/web_app_command_scheduler.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chromeos/crosapi/mojom/tts.mojom-forward.h"
#include "chromeos/lacros/lacros_service.h"
#include "components/webapps/browser/installable/installable_metrics.h"
#include "content/public/browser/tts_utterance.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest_constants.h"

namespace {

blink::mojom::DisplayMode WindowModeToDisplayMode(
    apps::WindowMode window_mode) {
  switch (window_mode) {
    case apps::WindowMode::kBrowser:
      return blink::mojom::DisplayMode::kBrowser;
    case apps::WindowMode::kTabbedWindow:
      return blink::mojom::DisplayMode::kTabbed;
    case apps::WindowMode::kWindow:
      return blink::mojom::DisplayMode::kStandalone;
    case apps::WindowMode::kUnknown:
      return blink::mojom::DisplayMode::kUndefined;
  }
}

web_app::UserDisplayMode WindowModeToUserDisplayMode(
    apps::WindowMode window_mode) {
  switch (window_mode) {
    case apps::WindowMode::kBrowser:
      return web_app::UserDisplayMode::kBrowser;
    case apps::WindowMode::kTabbedWindow:
      return web_app::UserDisplayMode::kTabbed;
    case apps::WindowMode::kWindow:
      return web_app::UserDisplayMode::kStandalone;
    case apps::WindowMode::kUnknown:
      return web_app::UserDisplayMode::kBrowser;
  }
}

}  // namespace

// With Lacros tts support enabled, all Lacros utterances will be sent to
// Ash to be processed by TtsController in Ash. When the utterance is spoken
// by a speech engine (provided by Ash or Lacros), we need to make sure that
// Tts events are routed back to the UtteranceEventDelegate in Lacros.
// This class can be set as UtteranceEventDelegate for Lacros Utterance used
// for testing.
class StandaloneBrowserTestController::LacrosUtteranceEventDelegate
    : public content::UtteranceEventDelegate {
 public:
  LacrosUtteranceEventDelegate(
      StandaloneBrowserTestController* controller,
      mojo::PendingRemote<crosapi::mojom::TtsUtteranceClient> client)
      : controller_(controller), client_(std::move(client)) {}

  LacrosUtteranceEventDelegate(const LacrosUtteranceEventDelegate&) = delete;
  LacrosUtteranceEventDelegate& operator=(const LacrosUtteranceEventDelegate&) =
      delete;
  ~LacrosUtteranceEventDelegate() override = default;

  // content::UtteranceEventDelegate methods:
  void OnTtsEvent(content::TtsUtterance* utterance,
                  content::TtsEventType event_type,
                  int char_index,
                  int char_length,
                  const std::string& error_message) override {
    // Forward the TtsEvent back to ash, so that ash browser test can verify
    // that TtsEvent has been routed to the UtteranceEventDelegate in Lacros.
    client_->OnTtsEvent(tts_crosapi_util::ToMojo(event_type), char_index,
                        char_length, error_message);

    if (utterance->IsFinished())
      controller_->OnUtteranceFinished(utterance->GetId());
    // Note: |this| is deleted if utterance->IsFinished().
  }

 private:
  // |controller_| is guaranteed to be valid during the lifetime of this class.
  const raw_ptr<StandaloneBrowserTestController> controller_;
  mojo::Remote<crosapi::mojom::TtsUtteranceClient> client_;
};

StandaloneBrowserTestController::StandaloneBrowserTestController(
    mojo::Remote<crosapi::mojom::TestController>& test_controller) {
  test_controller->RegisterStandaloneBrowserTestController(
      controller_receiver_.BindNewPipeAndPassRemoteWithVersion());
  test_controller.FlushAsync();
}

StandaloneBrowserTestController::~StandaloneBrowserTestController() = default;

void StandaloneBrowserTestController::InstallWebApp(
    const std::string& start_url,
    apps::WindowMode window_mode,
    InstallWebAppCallback callback) {
  auto info = std::make_unique<WebAppInstallInfo>();
  info->title = u"Test Web App";
  info->start_url = GURL(start_url);
  info->display_mode = WindowModeToDisplayMode(window_mode);
  info->user_display_mode = WindowModeToUserDisplayMode(window_mode);
  Profile* profile = ProfileManager::GetPrimaryUserProfile();
  auto* provider = web_app::WebAppProvider::GetForWebApps(profile);
  provider->scheduler().InstallFromInfo(
      std::move(info),
      /*overwrite_existing_manifest_fields=*/false,
      webapps::WebappInstallSource::SYNC,
      base::BindOnce(&StandaloneBrowserTestController::WebAppInstallationDone,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void StandaloneBrowserTestController::LoadVpnExtension(
    const std::string& extension_name,
    LoadVpnExtensionCallback callback) {
  std::string error;
  auto extension = extensions::Extension::Create(
      base::FilePath(), extensions::mojom::ManifestLocation::kUnpacked,
      CreateVpnExtensionManifest(extension_name),
      extensions::Extension::NO_FLAGS, &error);
  if (!error.empty()) {
    std::move(callback).Run(error);
    return;
  }

  auto* extension_registry = extensions::ExtensionRegistry::Get(
      ProfileManager::GetPrimaryUserProfile());
  extension_registry->AddEnabled(extension);
  extension_registry->TriggerOnLoaded(extension.get());

  std::move(callback).Run(extension->id());
}

void StandaloneBrowserTestController::GetTtsVoices(
    GetTtsVoicesCallback callback) {
  std::vector<content::VoiceData> voices;
  tts_crosapi_util::GetAllVoicesForTesting(  // IN-TEST
      ProfileManager::GetActiveUserProfile(), GURL(), &voices);

  std::vector<crosapi::mojom::TtsVoicePtr> mojo_voices;
  for (const auto& voice : voices)
    mojo_voices.push_back(tts_crosapi_util::ToMojo(voice));

  std::move(callback).Run(std::move(mojo_voices));
}

void StandaloneBrowserTestController::TtsSpeak(
    crosapi::mojom::TtsUtterancePtr mojo_utterance,
    mojo::PendingRemote<crosapi::mojom::TtsUtteranceClient> utterance_client) {
  std::unique_ptr<content::TtsUtterance> lacros_utterance =
      tts_crosapi_util::CreateUtteranceFromMojo(
          mojo_utterance, /*should_always_be_spoken=*/true);
  auto event_delegate = std::make_unique<LacrosUtteranceEventDelegate>(
      this, std::move(utterance_client));
  lacros_utterance->SetEventDelegate(event_delegate.get());
  lacros_utterance_event_delegates_.emplace(lacros_utterance->GetId(),
                                            std ::move(event_delegate));
  tts_crosapi_util::SpeakForTesting(std::move(lacros_utterance));
}

void StandaloneBrowserTestController::OnUtteranceFinished(int utterance_id) {
  // Delete the utterace event delegate object when the utterance is finished.
  lacros_utterance_event_delegates_.erase(utterance_id);
}

void StandaloneBrowserTestController::GetExtensionKeeplist(
    GetExtensionKeeplistCallback callback) {
  auto mojo_keeplist = crosapi::mojom::ExtensionKeepList::New();

  for (const auto& id :
       extensions::GetExtensionsRunInOSAndStandaloneBrowser()) {
    mojo_keeplist->extensions_run_in_os_and_standalonebrowser.push_back(
        std::string(id));
  }

  for (const auto& id :
       extensions::GetExtensionAppsRunInOSAndStandaloneBrowser()) {
    mojo_keeplist->extension_apps_run_in_os_and_standalonebrowser.push_back(
        std::string(id));
  }

  for (const auto& id : extensions::GetExtensionsRunInOSOnly())
    mojo_keeplist->extensions_run_in_os_only.push_back(std::string(id));

  for (const auto& id : extensions::GetExtensionAppsRunInOSOnly())
    mojo_keeplist->extension_apps_run_in_os_only.push_back(std::string(id));

  std::move(callback).Run(std::move(mojo_keeplist));
}

void StandaloneBrowserTestController::WebAppInstallationDone(
    InstallWebAppCallback callback,
    const web_app::AppId& installed_app_id,
    webapps::InstallResultCode code) {
  std::move(callback).Run(code == webapps::InstallResultCode::kSuccessNewInstall
                              ? installed_app_id
                              : "");
}

base::Value::Dict StandaloneBrowserTestController::CreateVpnExtensionManifest(
    const std::string& extension_name) {
  base::Value::Dict manifest;

  manifest.Set(extensions::manifest_keys::kName, extension_name);
  manifest.Set(extensions::manifest_keys::kVersion, "1.0");
  manifest.Set(extensions::manifest_keys::kManifestVersion, 2);

  base::Value::List permissions;
  permissions.Append("vpnProvider");
  manifest.Set(extensions::manifest_keys::kPermissions, std::move(permissions));

  return manifest;
}
