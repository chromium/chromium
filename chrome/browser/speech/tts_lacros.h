// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SPEECH_TTS_LACROS_H_
#define CHROME_BROWSER_SPEECH_TTS_LACROS_H_

#include "base/memory/raw_ptr.h"
#include "base/no_destructor.h"
#include "base/observer_list.h"
#include "base/scoped_observation.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_manager_observer.h"
#include "content/public/browser/tts_controller.h"
#include "content/public/browser/tts_platform.h"

namespace content {
class BrowserContext;
class TtsUtterance;
}  // namespace content

// Implements content::TtsPlatform.
// Creates TtsClientLacros when user profile is loaded, and handles TTS
// requests for TTS extension API and SpeechSynthesis web API in Lacros.
class TtsPlatformImplLacros : public content::TtsPlatform,
                              public ProfileManagerObserver {
 public:
  static TtsPlatformImplLacros* GetInstance();

  TtsPlatformImplLacros(const TtsPlatformImplLacros&) = delete;
  TtsPlatformImplLacros& operator=(const TtsPlatformImplLacros&) = delete;

  // TtsPlatform :
  bool PlatformImplSupported() override;
  bool PlatformImplInitialized() override;
  content::ExternalPlatformDelegate* GetExternalPlatformDelegate() override;

  // Unimplemented.
  void LoadBuiltInTtsEngine(content::BrowserContext* browser_context) override {
  }
  void Speak(int utterance_id,
             const std::string& utterance,
             const std::string& lang,
             const content::VoiceData& voice,
             const content::UtteranceContinuousParameters& params,
             base::OnceCallback<void(bool)> on_speak_finished) override {}
  bool StopSpeaking() override;
  void GetVoices(std::vector<content::VoiceData>* out_voices) override {}
  std::string GetError() override;
  void ClearError() override {}
  void SetError(const std::string& error) override {}
  bool IsSpeaking() override;
  void FinalizeVoiceOrdering(std::vector<content::VoiceData>& voices) override;
  void Pause() override {}
  void Resume() override {}
  void WillSpeakUtteranceWithVoice(
      content::TtsUtterance* utterance,
      const content::VoiceData& voice_data) override {}
  void Shutdown() override {}
  void RefreshVoices() override {}

 private:
  friend class base::NoDestructor<TtsPlatformImplLacros>;
  TtsPlatformImplLacros();
  ~TtsPlatformImplLacros() override;

  // ProfileManagerObserver:
  void OnProfileAdded(Profile* profile) override;
  void OnProfileManagerDestroying() override;

  raw_ptr<content::ExternalPlatformDelegate> external_platform_delegate_ =
      nullptr;

  base::ScopedObservation<ProfileManager, ProfileManagerObserver>
      profile_manager_observation_{this};

  // A set of delegates that want to be notified when the voices change.
  base::ObserverList<content::VoicesChangedDelegate> voices_changed_delegates_;
};

#endif  // CHROME_BROWSER_SPEECH_TTS_LACROS_H_
