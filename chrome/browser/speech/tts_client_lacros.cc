// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/speech/tts_client_lacros.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/logging.h"
#include "base/unguessable_token.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/speech/extension_api/tts_engine_extension_api.h"
#include "chrome/browser/speech/tts_client_factory_lacros.h"
#include "chrome/browser/speech/tts_crosapi_util.h"
#include "chromeos/lacros/lacros_service.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/tts_controller.h"
#include "content/public/browser/tts_platform.h"
#include "extensions/browser/event_router.h"

namespace {

bool IsOffline(net::NetworkChangeNotifier::ConnectionType type) {
  return type == net::NetworkChangeNotifier::CONNECTION_NONE;
}

}  // namespace

TtsClientLacros::TtsClientLacros(content::BrowserContext* browser_context)
    : browser_context_(browser_context),
      is_offline_(IsOffline(net::NetworkChangeNotifier::GetConnectionType())) {
  auto* service = chromeos::LacrosService::Get();
  if (!service->IsAvailable<crosapi::mojom::Tts>())
    return;

  browser_context_id_ = base::UnguessableToken::Create();
  bool is_primary_profile = ProfileManager::GetPrimaryUserProfile() ==
                            Profile::FromBrowserContext(browser_context);

  // TODO(crbug.com/1251979): Support secondary profiles when it becomes
  // available for Lacros.
  if (!is_primary_profile)
    return;

  service->GetRemote<crosapi::mojom::Tts>()->RegisterTtsClient(
      receiver_.BindNewPipeAndPassRemoteWithVersion(), browser_context_id_,
      /*is_primary_profile=*/is_primary_profile);

  net::NetworkChangeNotifier::AddNetworkChangeObserver(this);

  extensions::EventRouter* event_router = extensions::EventRouter::Get(
      Profile::FromBrowserContext(browser_context_));
  DCHECK(event_router);
  event_router->RegisterObserver(this, tts_engine_events::kOnSpeak);
  event_router->RegisterObserver(this, tts_engine_events::kOnStop);

  // Push Lacros voices to Ash.
  NotifyLacrosVoicesChanged();
}

void TtsClientLacros::VoicesChanged(
    std::vector<crosapi::mojom::TtsVoicePtr> mojo_all_voices) {
  // Update the cached voices.
  all_voices_.clear();
  for (const auto& mojo_voice : mojo_all_voices) {
    all_voices_.push_back(tts_crosapi_util::FromMojo(mojo_voice));
  }

  // Notify TtsPlatform that the cached voices have changed.
  content::TtsController::GetInstance()->VoicesChanged();
}

void TtsClientLacros::GetAllVoices(
    std::vector<content::VoiceData>* out_voices) {
  // Return the cached voices that should be available for the associated
  // |browser_context_|, including voices provided by both Ash and Lacros.
  for (const auto& voice : all_voices_)
    out_voices->push_back(voice);
}

void TtsClientLacros::Shutdown() {
  extensions::EventRouter::Get(Profile::FromBrowserContext(browser_context_))
      ->UnregisterObserver(this);
}

TtsClientLacros::~TtsClientLacros() {
  net::NetworkChangeNotifier::RemoveNetworkChangeObserver(this);
}

TtsClientLacros* TtsClientLacros::GetForBrowserContext(
    content::BrowserContext* context) {
  return TtsClientFactoryLacros::GetForBrowserContext(context);
}

void TtsClientLacros::OnNetworkChanged(
    net::NetworkChangeNotifier::ConnectionType type) {
  // Since the remote voices are NOT returned by TtsExtensionEngine::GetVoices()
  // if the system is offline, threfore, when the network status changes, the
  // Lacros voices need to be refreshed to ensure the remote voices to be
  // included or excluded according to the current network state.
  bool is_offline = IsOffline(type);
  if (is_offline_ != is_offline) {
    is_offline_ = is_offline;
    NotifyLacrosVoicesChanged();
  }
}

void TtsClientLacros::OnListenerAdded(
    const extensions::EventListenerInfo& details) {
  if (!IsLoadedTtsEngine(details.extension_id))
    return;

  NotifyLacrosVoicesChanged();
}

void TtsClientLacros::OnListenerRemoved(
    const extensions::EventListenerInfo& details) {
  if (details.event_name != tts_engine_events::kOnSpeak &&
      details.event_name != tts_engine_events::kOnStop) {
    return;
  }

  NotifyLacrosVoicesChanged();
}

bool TtsClientLacros::IsLoadedTtsEngine(const std::string& extension_id) const {
  extensions::EventRouter* event_router = extensions::EventRouter::Get(
      Profile::FromBrowserContext(browser_context_));
  DCHECK(event_router);
  return event_router->ExtensionHasEventListener(extension_id,
                                                 tts_engine_events::kOnSpeak) &&
         event_router->ExtensionHasEventListener(extension_id,
                                                 tts_engine_events::kOnStop);
}

void TtsClientLacros::NotifyLacrosVoicesChanged() {
  chromeos::LacrosService* service = chromeos::LacrosService::Get();
  if (!service->IsAvailable<crosapi::mojom::Tts>())
    return;

  // Get the voices registered in Lacros.
  std::vector<content::VoiceData> voices;
  content::TtsEngineDelegate* tts_engine_delegate =
      content::TtsController::GetInstance()->GetTtsEngineDelegate();
  DCHECK(tts_engine_delegate);
  tts_engine_delegate->GetVoices(browser_context_, GURL(), &voices);

  // Convert to mojo voices.
  std::vector<crosapi::mojom::TtsVoicePtr> mojo_voices;
  for (const auto& voice : voices)
    mojo_voices.push_back(tts_crosapi_util::ToMojo(voice));

  // Push new Lacros voices to ash.
  service->GetRemote<crosapi::mojom::Tts>()->VoicesChanged(
      browser_context_id_, std::move(mojo_voices));
}

void TtsClientLacros::OnGetAllVoices(
    std::vector<crosapi::mojom::TtsVoicePtr> mojo_voices) {
  // Update the cached voices.
  all_voices_.clear();
  for (const auto& mojo_voice : mojo_voices) {
    all_voices_.push_back(tts_crosapi_util::FromMojo(mojo_voice));
  }

  // Notify TtsPlatform that the cached voices have changed.
  content::TtsController::GetInstance()->VoicesChanged();
}
