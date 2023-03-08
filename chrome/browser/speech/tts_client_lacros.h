// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SPEECH_TTS_CLIENT_LACROS_H_
#define CHROME_BROWSER_SPEECH_TTS_CLIENT_LACROS_H_

#include <memory>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/unguessable_token.h"
#include "chromeos/crosapi/mojom/tts.mojom.h"
#include "content/public/browser/tts_controller.h"
#include "extensions/browser/browser_context_keyed_api_factory.h"
#include "extensions/browser/event_router.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "net/base/network_change_notifier.h"

namespace content {
class BrowserContext;
}

// Implements crosapi::mojom::TtsClient, which is called by ash to handle
// TTS requests to Lacros such as retrieving voice data, etc. It also manages
// To send TTS requests ash. TtsClientLacros is created per BrowserContext.
class TtsClientLacros
    : public extensions::BrowserContextKeyedAPI,
      public crosapi::mojom::TtsClient,
      public net::NetworkChangeNotifier::NetworkChangeObserver,
      public extensions::EventRouter::Observer {
 public:
  explicit TtsClientLacros(content::BrowserContext* context);
  TtsClientLacros(const TtsClientLacros&) = delete;
  TtsClientLacros& operator=(const TtsClientLacros&) = delete;
  ~TtsClientLacros() override;

  // crosapi::mojom::TtsClient:
  void VoicesChanged(
      std::vector<crosapi::mojom::TtsVoicePtr> mojo_all_voices) override;
  void SpeakWithLacrosVoice(
      crosapi::mojom::TtsUtterancePtr utterance,
      crosapi::mojom::TtsVoicePtr voice,
      mojo::PendingRemote<crosapi::mojom::TtsUtteranceClient>
          ash_utterance_client) override;
  void Stop(const std::string& engine_id) override;
  void Pause(const std::string& engine_id) override;
  void Resume(const std::string& engine_id) override;

  const base::UnguessableToken& browser_context_id() const {
    return browser_context_id_;
  }

  // Returns the cached voices in |out_voices|, which are the voices available
  // for Lacros including the ones provided by both Ash and Lacros.
  void GetAllVoices(std::vector<content::VoiceData>* out_voices);

  // Forwards the given utterance to Ash to be processed by Ash TtsController.
  void SpeakOrEnqueue(std::unique_ptr<content::TtsUtterance> utterance);

  // Forwards the Stop request (for stopping the current utterance if it matches
  // the given |source_url|) to Ash.
  void RequestStop(const GURL& source_url);

  // Forwards the Pause request from Lacros Tts client (Tts extension api or
  // speechSynthesis web api) to Ash, so that the request will be processed by
  // Ash's TtsController.
  void RequestPause();

  // Forwards the Resume request from Lacros Tts client (Tts extension api or
  // speechSynthesis web api) to Ash, so that the request will be processed by
  // Ash's TtsController.
  void RequestResume();

  // Forwards the request to query Ash TtsController's IsSpeaking() state and
  // returns the result in callback.
  void IsSpeaking(base::OnceCallback<void(bool)> callback);

  // Handle events received from the Lacros speech engine.
  void OnLacrosSpeechEngineTtsEvent(int utterance_id,
                                    content::TtsEventType event_type,
                                    int char_index,
                                    int length,
                                    const std::string& error_message);
  void OnAshUtteranceFinished(int utterance_id);
  void OnAshUtteranceBecameInvalid(int utterance_id);

  void DeletePendingUtteranceClient(int utterance_id);

  content::BrowserContext* browser_context() { return browser_context_; }

  static TtsClientLacros* GetForBrowserContext(
      content::BrowserContext* context);

 private:
  class TtsUtteraneClient;
  class AshUtteranceEventDelegate;
  friend class extensions::BrowserContextKeyedAPIFactory<TtsClientLacros>;

  // net::NetworkChangeNotifier::NetworkChangeObserver:
  void OnNetworkChanged(
      net::NetworkChangeNotifier::ConnectionType type) override;

  // extensions::EventRouter::Observer:
  void OnListenerAdded(const extensions::EventListenerInfo& details) override;
  void OnListenerRemoved(const extensions::EventListenerInfo& details) override;

  bool IsLoadedTtsEngine(const std::string& extension_id) const;
  // Notifies Ash about Lacros voices change.
  void NotifyLacrosVoicesChanged();

  void OnGetAllVoices(std::vector<crosapi::mojom::TtsVoicePtr> mojo_voices);

  // KeyedServivce:
  void Shutdown() override;

  raw_ptr<content::BrowserContext> browser_context_;  // not owned.
  base::UnguessableToken browser_context_id_;
  mojo::Receiver<crosapi::mojom::TtsClient> receiver_{this};

  // Cached voices for |browser_context_|, including both ash and lacros voices.
  std::vector<content::VoiceData> all_voices_;

  bool is_offline_;

  // Pending Lacros Tts Utterance clients by by utterance id.
  std::map<int, std::unique_ptr<TtsUtteraneClient>> pending_utterance_clients_;

  // Pending Ash utterance to be spoken with Lacros speech engine.
  std::unique_ptr<content::TtsUtterance> pending_ash_utterance_;
  std::unique_ptr<AshUtteranceEventDelegate> ash_utterance_event_delegate_;

  base::WeakPtrFactory<TtsClientLacros> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_SPEECH_TTS_CLIENT_LACROS_H_
