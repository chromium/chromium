// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SPEECH_EXTENSION_API_TTS_ENGINE_EXTENSION_API_CHROMEOS_H_
#define CHROME_BROWSER_SPEECH_EXTENSION_API_TTS_ENGINE_EXTENSION_API_CHROMEOS_H_

#include "base/scoped_observation.h"
#include "base/values.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_observer.h"
#include "chrome/browser/speech/extension_api/tts_engine_extension_api.h"
#include "chromeos/services/tts/public/mojom/tts_service.mojom.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace content {
class BrowserContext;
}

// TtsEngineDelegate implementation used by TtsController on Chrome OS.
class TtsExtensionEngineChromeOS
    : public TtsExtensionEngine,
      public chromeos::tts::mojom::TtsEventObserver,
      public ProfileObserver {
 public:
  TtsExtensionEngineChromeOS();
  ~TtsExtensionEngineChromeOS() override;

  // TtsExtensionEngine:
  void SendAudioBuffer(int utterance_id,
                       const std::vector<float>& audio_buffer,
                       int char_index,
                       bool is_last_buffer) override;

  // TtsEngineDelegate:
  void Speak(content::TtsUtterance* utterance,
             const content::VoiceData& voice) override;
  void Stop(content::TtsUtterance* utterance) override;
  void Pause(content::TtsUtterance* utterance) override;
  void Resume(content::TtsUtterance* utterance) override;
  void LoadBuiltInTtsEngine(content::BrowserContext* browser_context) override;
  bool IsBuiltInTtsEngineInitialized(
      content::BrowserContext* browser_context) override;

  // TtsEventObserver:
  void OnStart() override;
  void OnTimepoint(int32_t char_index) override;
  void OnEnd() override;
  void OnError() override;

  // ProfileObserver:
  void OnProfileWillBeDestroyed(Profile* profile) override;

 private:
  // Unconditionally updates audio stream options.
  void UpdateAudioStreamOptions(
      chromeos::tts::mojom::AudioParametersPtr audio_parameters);

  // Refresh audio stream options from an extension's manifest. Returns true if
  // parameters were updated.
  bool RefreshAudioStreamOptionsForExtension(const std::string& engine_id,
                                             Profile* profile);

  // Helper to start audio playback.
  void Play(base::Value::List args,
            const std::string& engine_id,
            Profile* profile);

  mojo::Remote<chromeos::tts::mojom::PlaybackTtsStream> playback_tts_stream_;
  mojo::ReceiverSet<chromeos::tts::mojom::TtsEventObserver>
      tts_event_observer_receiver_set_;

  int current_utterance_id_ = -1;
  int current_utterance_length_ = -1;
  double current_volume_ = 1;
  base::ScopedObservation<Profile, ProfileObserver>
      current_utterance_profile_observer_{this};

  chromeos::tts::mojom::AudioParametersPtr audio_parameters_;
  extensions::ExtensionId current_playback_engine_;
};

#endif  // CHROME_BROWSER_SPEECH_EXTENSION_API_TTS_ENGINE_EXTENSION_API_CHROMEOS_H_
