// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/speech/tts_client_lacros.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/logging.h"
#include "base/memory/raw_ptr.h"
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

// This class is used as the UtteranceEventDelegate for the Ash utterance
// sent to Lacros to be spoken with Lacros speech engine.
// The lifetime of instance of this class is bound to the lifetime of the
// associated TtsUtterance.
class TtsClientLacros::AshUtteranceEventDelegate
    : public content::UtteranceEventDelegate {
 public:
  AshUtteranceEventDelegate(
      TtsClientLacros* owner,
      int utterance_id,
      mojo::PendingRemote<crosapi::mojom::TtsUtteranceClient> client)
      : owner_(owner), utterance_id_(utterance_id), client_(std::move(client)) {
    client_.set_disconnect_handler(base::BindOnce(
        &AshUtteranceEventDelegate::OnTtsUtteranceClientDisconnected,
        weak_ptr_factory_.GetWeakPtr()));
  }

  AshUtteranceEventDelegate(const AshUtteranceEventDelegate&) = delete;
  AshUtteranceEventDelegate& operator=(const AshUtteranceEventDelegate&) =
      delete;
  ~AshUtteranceEventDelegate() override = default;

  // content::UtteranceEventDelegate:
  void OnTtsEvent(content::TtsUtterance* utterance,
                  content::TtsEventType event_type,
                  int char_index,
                  int char_length,
                  const std::string& error_message) override {
    DCHECK_EQ(utterance->GetId(), utterance_id_);
    // Forward the Tts event to Ash.
    // Note: If |client_| is disconnected, this will be a no-op.
    client_->OnTtsEvent(tts_crosapi_util::ToMojo(event_type), char_index,
                        char_length, error_message);

    if (utterance->IsFinished()) {
      owner_->OnAshUtteranceFinished(utterance->GetId());
      // |this| is deleted at this point.
    }
  }

 private:
  void OnTtsUtteranceClientDisconnected() {
    // This will be triggered when the web contents associated with the ash
    // utterance destroyed in ash.
    owner_->OnAshUtteranceBecameInvalid(utterance_id_);
    // |this| is deleted at this point.
  }

  const raw_ptr<TtsClientLacros> owner_;  // not owned
  // Id of the TtsUtterance created in Lacros for the ash utterance.
  int utterance_id_;

  // Used to forward the Tts events back to Ash. Its disconnect handler will
  // be invoked when its original utterance instance in Ash becomes invalid.
  mojo::Remote<crosapi::mojom::TtsUtteranceClient> client_;

  base::WeakPtrFactory<AshUtteranceEventDelegate> weak_ptr_factory_{this};
};

// This class implements crosapi::mojom::TtsUtteranceClient.
// It is used to create a remote pending utterance client for a Lacros
// utterance sent to Ash's TtsController.
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
  // Called from Ash to forward the Ash speech engine event back to the original
  // TtsUtterance in Lacros, which will forward the event to its
  // UtteranceEventDelegate. This is used when the utterance is spoken by
  // a Ash voice.
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

  // Handle TtsEvent received from Lacros speech engine if the utterance is
  // spoken by a Lacros TTS engine, forward the event to callback function and
  // finishing the current utterance processing if receiving completion or error
  // events.
  void OnLacrosSpeechEngineTtsEvent(int utterance_id,
                                    content::TtsEventType event_type,
                                    int char_index,
                                    int length,
                                    const std::string& error_message) {
    utterance_->OnTtsEvent(event_type, char_index, length, error_message);

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

  content::TtsUtterance* GetUttenrance() { return utterance_.get(); }

 private:
  raw_ptr<TtsClientLacros> owner_;  // now owned
  // This is the original utterance in Lacros, owned.
  std::unique_ptr<content::TtsUtterance> utterance_;
  mojo::Receiver<crosapi::mojom::TtsUtteranceClient> receiver_{this};
};

TtsClientLacros::TtsClientLacros(content::BrowserContext* browser_context)
    : browser_context_(browser_context),
      is_offline_(IsOffline(net::NetworkChangeNotifier::GetConnectionType())) {
  auto* service = chromeos::LacrosService::Get();
  if (!service->IsAvailable<crosapi::mojom::Tts>() ||
      !tts_crosapi_util::ShouldEnableLacrosTtsSupport()) {
    return;
  }

  browser_context_id_ = base::UnguessableToken::Create();
  bool is_primary_profile = ProfileManager::GetPrimaryUserProfile() ==
                            Profile::FromBrowserContext(browser_context);

  // TODO(crbug.com/40792881): Support secondary profiles when it becomes
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

void TtsClientLacros::SpeakWithLacrosVoice(
    crosapi::mojom::TtsUtterancePtr mojo_utterance,
    crosapi::mojom::TtsVoicePtr mojo_voice,
    mojo::PendingRemote<crosapi::mojom::TtsUtteranceClient>
        ash_pending_utterance_client) {
  // Speak a Lacros utterance with a Lacros voice.
  content::VoiceData voice = tts_crosapi_util::FromMojo(mojo_voice);
  if (!ash_pending_utterance_client) {
    auto item = pending_utterance_clients_.find(mojo_utterance->utterance_id);
    if (item != pending_utterance_clients_.end()) {
      // Speaking a Lacros utterance.
      content::TtsUtterance* current_utterance_to_speak =
          item->second->GetUttenrance();
      current_utterance_to_speak->SetEngineId(mojo_utterance->engine_id);
      content::TtsController::GetInstance()->GetTtsEngineDelegate()->Speak(
          current_utterance_to_speak, voice);
    }
  } else {
    // Speaking an Ash utterance.
    // There should NOT be an active pending ash utterance, since Ash
    // TtsController won't process the next utterance in the utterance queue
    // until the current one is finished.
    DCHECK(!pending_ash_utterance_ && !ash_utterance_event_delegate_);
    // Create a TtsUtterance in Lacros for the Ash utterance to be
    // spoken with Lacros speech engine.
    pending_ash_utterance_ = tts_crosapi_util::CreateUtteranceFromMojo(
        mojo_utterance, /*should_always_be_spoken=*/false);
    ash_utterance_event_delegate_.reset(
        new AshUtteranceEventDelegate(this, pending_ash_utterance_->GetId(),
                                      std::move(ash_pending_utterance_client)));
    pending_ash_utterance_->SetEventDelegate(
        ash_utterance_event_delegate_.get());
    // Request Lacros Tts engine to speak the Ash utterance.
    content::TtsController::GetInstance()->GetTtsEngineDelegate()->Speak(
        pending_ash_utterance_.get(), voice);
  }
}

void TtsClientLacros::Stop(const std::string& engine_id) {
  TtsExtensionEngine::GetInstance()->Stop(browser_context_, engine_id);
}

void TtsClientLacros::Pause(const std::string& engine_id) {
  TtsExtensionEngine::GetInstance()->Pause(browser_context_, engine_id);
}

void TtsClientLacros::Resume(const std::string& engine_id) {
  TtsExtensionEngine::GetInstance()->Resume(browser_context_, engine_id);
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
          lacros_service->GetInterfaceVersion<crosapi::mojom::Tts>()) <
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

void TtsClientLacros::RequestStop(const GURL& source_url) {
  auto* lacros_service = chromeos::LacrosService::Get();
  if (!lacros_service->IsAvailable<crosapi::mojom::Tts>() ||
      static_cast<uint32_t>(
          lacros_service->GetInterfaceVersion<crosapi::mojom::Tts>()) <
          crosapi::mojom::Tts::kStopMinVersion) {
    LOG(WARNING) << kErrorUnsupportedVersion;
    return;
  }
  lacros_service->GetRemote<crosapi::mojom::Tts>()->Stop(source_url);
}

void TtsClientLacros::RequestPause() {
  auto* lacros_service = chromeos::LacrosService::Get();
  if (!lacros_service->IsAvailable<crosapi::mojom::Tts>() ||
      static_cast<uint32_t>(
          lacros_service->GetInterfaceVersion<crosapi::mojom::Tts>()) <
          crosapi::mojom::Tts::kPauseMinVersion) {
    LOG(WARNING) << kErrorUnsupportedVersion;
    return;
  }
  lacros_service->GetRemote<crosapi::mojom::Tts>()->Pause();
}

void TtsClientLacros::RequestResume() {
  auto* lacros_service = chromeos::LacrosService::Get();
  if (!lacros_service->IsAvailable<crosapi::mojom::Tts>() ||
      static_cast<uint32_t>(
          lacros_service->GetInterfaceVersion<crosapi::mojom::Tts>()) <
          crosapi::mojom::Tts::kResumeMinVersion) {
    LOG(WARNING) << kErrorUnsupportedVersion;
    return;
  }
  lacros_service->GetRemote<crosapi::mojom::Tts>()->Resume();
}

void TtsClientLacros::IsSpeaking(base::OnceCallback<void(bool)> callback) {
  auto* lacros_service = chromeos::LacrosService::Get();
  if (!lacros_service->IsAvailable<crosapi::mojom::Tts>() ||
      static_cast<uint32_t>(
          lacros_service->GetInterfaceVersion<crosapi::mojom::Tts>()) <
          crosapi::mojom::Tts::kIsSpeakingMinVersion) {
    LOG(WARNING) << kErrorUnsupportedVersion;
    std::move(callback).Run(false);
    return;
  }
  lacros_service->GetRemote<crosapi::mojom::Tts>()->IsSpeaking(
      std::move(callback));
}

void TtsClientLacros::OnLacrosSpeechEngineTtsEvent(
    int utterance_id,
    content::TtsEventType event_type,
    int char_index,
    int length,
    const std::string& error_message) {
  auto item = pending_utterance_clients_.find(utterance_id);
  if (item != pending_utterance_clients_.end()) {
    // Lacros utterance.
    item->second->OnLacrosSpeechEngineTtsEvent(
        utterance_id, event_type, char_index, length, error_message);
  } else if (pending_ash_utterance_) {
    // Ash utterance.
    DCHECK_EQ(utterance_id, pending_ash_utterance_->GetId());
    pending_ash_utterance_->OnTtsEvent(event_type, char_index, length,
                                       error_message);
  }
}

void TtsClientLacros::DeletePendingUtteranceClient(int utterance_id) {
  pending_utterance_clients_.erase(utterance_id);
}

void TtsClientLacros::OnAshUtteranceFinished(int utterance_id) {
  DCHECK(pending_ash_utterance_);
  DCHECK_EQ(pending_ash_utterance_->GetId(), utterance_id);
  pending_ash_utterance_.reset();
  ash_utterance_event_delegate_.reset();
}

void TtsClientLacros::OnAshUtteranceBecameInvalid(int utterance_id) {
  DCHECK(pending_ash_utterance_);
  pending_ash_utterance_->Finish();
  OnAshUtteranceFinished(utterance_id);
}
