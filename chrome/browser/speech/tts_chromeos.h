// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SPEECH_TTS_CHROMEOS_H_
#define CHROME_BROWSER_SPEECH_TTS_CHROMEOS_H_

#include "base/no_destructor.h"
#include "content/public/browser/tts_platform.h"

// This class includes extension-based tts through LoadBuiltInTtsExtension and
// native tts through ARC.
class TtsPlatformImplChromeOs : public content::TtsPlatform {
 public:
  TtsPlatformImplChromeOs(const TtsPlatformImplChromeOs&) = delete;
  TtsPlatformImplChromeOs& operator=(const TtsPlatformImplChromeOs&) = delete;

  // Sets the voices exposed by this TtsPlatform.
  void SetVoices(std::vector<content::VoiceData> voices);

  // Called by ArcTtsService when it receives a word event.
  void ReceivedWordEvent();

  // TtsPlatform overrides:
  bool PlatformImplSupported() override;
  bool PlatformImplInitialized() override;
  void LoadBuiltInTtsEngine(content::BrowserContext* browser_context) override;
  void Speak(int utterance_id,
             const std::string& utterance,
             const std::string& lang,
             const content::VoiceData& voice,
             const content::UtteranceContinuousParameters& params,
             base::OnceCallback<void(bool)> on_speak_finished) override;
  bool StopSpeaking() override;
  void GetVoices(std::vector<content::VoiceData>* out_voices) override;
  std::string GetError() override;
  void ClearError() override;
  void SetError(const std::string& error) override;
  bool IsSpeaking() override;
  void FinalizeVoiceOrdering(std::vector<content::VoiceData>& voices) override;
  void RefreshVoices() override;

  // Unimplemented.
  void Pause() override {}
  void Resume() override {}
  void WillSpeakUtteranceWithVoice(
      content::TtsUtterance* utterance,
      const content::VoiceData& voice_data) override {}
  void Shutdown() override {}
  content::ExternalPlatformDelegate* GetExternalPlatformDelegate() override;

  // Get the single instance of this class.
  static TtsPlatformImplChromeOs* GetInstance();

 private:
  friend base::NoDestructor<TtsPlatformImplChromeOs>;
  friend class TtsChromeosTest;
  TtsPlatformImplChromeOs();
  virtual ~TtsPlatformImplChromeOs();

  void ProcessSpeech(int utterance_id,
                     const std::string& lang,
                     const content::VoiceData& voice,
                     const content::UtteranceContinuousParameters& params,
                     base::OnceCallback<void(bool)> on_speak_finished,
                     const std::string& parsed_utterance);

  std::string error_;

  std::vector<content::VoiceData> voices_;

  bool received_word_event_ = false;
};

#endif  // CHROME_BROWSER_SPEECH_TTS_CHROMEOS_H_
