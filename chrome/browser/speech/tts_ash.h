// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SPEECH_TTS_ASH_H_
#define CHROME_BROWSER_SPEECH_TTS_ASH_H_

#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/unguessable_token.h"
#include "chrome/browser/profiles/profile_manager_observer.h"
#include "chromeos/crosapi/mojom/tts.mojom.h"
#include "content/public/browser/tts_controller.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver_set.h"

class GURL;
class ProfileManager;

namespace crosapi {
// Implements tts interface to allow Lacros to call ash to handle TTS
// requests, manages remote TtsClient objects registered by Lacros, and
// caches the voices from Lacros.
class TtsAsh : public mojom::Tts,
               public content::VoicesChangedDelegate,
               public ProfileManagerObserver {
 public:
  explicit TtsAsh(ProfileManager* profile_manager);
  TtsAsh(const TtsAsh&) = delete;
  TtsAsh& operator=(const TtsAsh&) = delete;
  ~TtsAsh() override;

  void BindReceiver(mojo::PendingReceiver<mojom::Tts> pending_receiver);

  // Returns true if there is any registered Tts Client.
  bool HasTtsClient() const;

  // Returns the browser context id for primary profile.
  base::UnguessableToken GetPrimaryProfileBrowserContextId() const;

  // Returns the cached lacros voices in |out_voices| for
  // |browser_context_id|.
  void GetCrosapiVoices(base::UnguessableToken browser_context_id,
                        std::vector<content::VoiceData>* out_voices);

  // Requests to the associated Lacros speech engine to speak the given
  // |utterance| with the given |voice|.
  void SpeakWithLacrosVoice(content::TtsUtterance* utterance,
                            const content::VoiceData& voice);

  // Requests the associated Lacros speech engine to stop speaking the
  // |utterance|.
  void StopRemoteEngine(content::TtsUtterance* utterance);

  // Requests the associated Lacros speech engine to pause speaking the
  // |utterance|.
  void PauseRemoteEngine(content::TtsUtterance* utterance);

  // Requests the associated Lacros speech engine to resume speaking the
  // |utterance|.
  void ResumeRemoteEngine(content::TtsUtterance* utterance);

  void DeletePendingAshUtteranceClient(int utterance_id);

  // crosapi::mojom::Tts:
  void RegisterTtsClient(mojo::PendingRemote<mojom::TtsClient> client,
                         const base::UnguessableToken& browser_context_id,
                         bool from_primary_profile) override;
  void VoicesChanged(const base::UnguessableToken& browser_context_id,
                     std::vector<mojom::TtsVoicePtr> lacros_voices) override;
  void SpeakOrEnqueue(
      mojom::TtsUtterancePtr utterance,
      mojo::PendingRemote<mojom::TtsUtteranceClient> utterance_client) override;
  void Stop(const GURL& source_url) override;
  void Pause() override;
  void Resume() override;
  void IsSpeaking(IsSpeakingCallback callback) override;

 private:
  class TtsUtteranceClient;
  // content::VoicesChangedDelegate:
  void OnVoicesChanged() override;

  // ProfileManagerObserver:
  void OnProfileAdded(Profile* profile) override;
  void OnProfileManagerDestroying() override;

  // Called when a Tts client is disconnected.
  void TtsClientDisconnected(const base::UnguessableToken& browser_context_id);

  // Owned by g_browser_process.
  raw_ptr<ProfileManager> profile_manager_;

  // Support any number of connections.
  mojo::ReceiverSet<mojom::Tts> receivers_;

  // Registered TtsClients from Lacros by browser_context_id.
  std::map<base::UnguessableToken, mojo::Remote<mojom::TtsClient>> tts_clients_;

  // Cached lacros voices by browser_context_id.
  std::map<base::UnguessableToken, std::vector<content::VoiceData>>
      crosapi_voices_;

  base::UnguessableToken primary_profile_browser_context_id_;

  // Pending Ash Utterance clients (for the Ash uttenrances to be spoken by
  // Lacros speech engine) by utterance id.
  // Note: The size of |pending_ash_utterance_clients_| should not be greater
  // that one, since Ash TtsController process the utterances one at a time in
  // sequence and  will not send more than 1 utterance to Lacros to be spoken.
  std::map<int, std::unique_ptr<TtsUtteranceClient>>
      pending_ash_utterance_clients_;

  base::ScopedObservation<content::TtsController,
                          content::VoicesChangedDelegate>
      voices_changed_observation_{this};

  base::ScopedObservation<ProfileManager, ProfileManagerObserver>
      profile_manager_observation_{this};

  base::WeakPtrFactory<TtsAsh> weak_ptr_factory_{this};
};

}  // namespace crosapi

#endif  // CHROME_BROWSER_SPEECH_TTS_ASH_H_
