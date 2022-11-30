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
#include "content/public/browser/tts_utterance.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "extensions/browser/event_router.h"

namespace {

bool IsOffline(net::NetworkChangeNotifier::ConnectionType type) {
  return type == net::NetworkChangeNotifier::CONNECTION_NONE;
}

// A value to be used to indicate that there is no char index available.
const int kInvalidCharIndex = -1;

// A value to be used to indicate that there is no length available.
const int kInvalidLength = -1;

constexpr char kErrorUnsupportedVersion[] = "crosapi: Unsupported ash version";

}  // namespace

// This class implements crosapi::mojom::TtsUtteranceClient.
// It observes the WebContent associated with the original utterance in Lacros.
class TtsClientLacros::TtsUtteraneClient
    : public crosapi::mojom::TtsUtteranceClient,
      public content::WebContentsObserver {
 public:
  TtsUtteraneClient(TtsClientLacros* owner,
                    std::unique_ptr<content::TtsUtterance> utterance)
      : content::WebContentsObserver(utterance->GetWebContents()),
        owner_(owner),
        utterance_(std::move(utterance)) {}

  TtsUtteraneClient(const TtsUtteraneClient&) = delete;
  TtsUtteraneClient& operator=(const TtsUtteraneClient&) = delete;
  ~TtsUtteraneClient() override = default;

  // crosapi::mojom::TtsUtteranceClient:
  // Called from Ash to forward the speech engine event back to the original
  // TtsUtterance in Lacros, which will forward the event to its
  // UtteranceEventDelegate.
  void OnTtsEvent(crosapi::mojom::TtsEventType mojo_tts_event,
                  uint32_t char_index,
                  uint32_t char_length,
                  const std::string& error_message) override {
    content::TtsEventType event_type =
        tts_crosapi_util::FromMojo(mojo_tts_event);
    utterance_->OnTtsEvent(event_type, char_index, char_length, error_message);

    if (content::IsFinalTtsEventType(event_type)) {
      utterance_->Finish();
      owner_->DeletePendingUtteranceClient(utterance_->GetId());
      // Note: |this| is deleted at this point.
    }
  }

  // content::WebContentsObserver:
  void WebContentsDestroyed() override {
    // Clean up the utterance in Lacros.
    utterance_->OnTtsEvent(content::TTS_EVENT_INTERRUPTED, kInvalidCharIndex,
                           kInvalidLength, std::string());
    utterance_->Finish();

    // Deleting the pending utterance client will trigger Ash to stop and
    // remove the utterance.
    owner_->DeletePendingUtteranceClient(utterance_->GetId());
    // Note: |this| is deleted at this point.
  }

  mojo::PendingRemote<crosapi::mojom::TtsUtteranceClient>
  BindTtsUtteranceClient() {
    return receiver_.BindNewPipeAndPassRemoteWithVersion();
  }

 private:
  TtsClientLacros* owner_;  // now owned
  // This is the original utterance in Lacros, owned.
  std::unique_ptr<content::TtsUtterance> utterance_;
  mojo::Receiver<crosapi::mojom::TtsUtteranceClient> receiver_{this};
};

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

void TtsClientLacros::SpeakOrEnqueue(
    std::unique_ptr<content::TtsUtterance> utterance) {
  auto* lacros_service = chromeos::LacrosService::Get();
  if (!lacros_service->IsAvailable<crosapi::mojom::Tts>() ||
      static_cast<uint32_t>(
          lacros_service->GetInterfaceVersion(crosapi::mojom::Tts::Uuid_)) <
          crosapi::mojom::Tts::kSpeakOrEnqueueMinVersion) {
    LOG(WARNING) << kErrorUnsupportedVersion;
    return;
  }

  int utterance_id = utterance->GetId();
  crosapi::mojom::TtsUtterancePtr mojo_utterance =
      tts_crosapi_util::ToMojo(utterance.get());
  mojo_utterance->browser_context_id = browser_context_id_;
  auto pending_client =
      std::make_unique<TtsUtteraneClient>(this, std::move(utterance));
  lacros_service->GetRemote<crosapi::mojom::Tts>()->SpeakOrEnqueue(
      std::move(mojo_utterance), pending_client->BindTtsUtteranceClient());

  // We don't need this for just supporting speaking ash voice from lacros.
  pending_utterance_clients_.emplace(utterance_id, std::move(pending_client));
}

void TtsClientLacros::DeletePendingUtteranceClient(int utterance_id) {
  pending_utterance_clients_.erase(utterance_id);
}
