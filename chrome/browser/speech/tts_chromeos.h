// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SPEECH_TTS_CHROMEOS_H_
#define CHROME_BROWSER_SPEECH_TTS_CHROMEOS_H_

#include "base/macros.h"
#include "content/public/browser/tts_platform.h"

// This class includes extension-based tts through LoadBuiltInTtsExtension and
// native tts through ARC.
class TtsPlatformImplChromeOs : public content::TtsPlatform {
 public:
  // TtsPlatform overrides:
  bool PlatformImplAvailable() override;
  bool LoadBuiltInTtsEngine(content::BrowserContext* browser_context) override;
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

  // Unimplemented.
  void Pause() override {}
  void Resume() override {}
  void WillSpeakUtteranceWithVoice(
      content::TtsUtterance* utterance,
      const content::VoiceData& voice_data) override {}

  // Get the single instance of this class.
  static TtsPlatformImplChromeOs* GetInstance();

 private:
  TtsPlatformImplChromeOs();
  virtual ~TtsPlatformImplChromeOs();

  void ProcessSpeech(int utterance_id,
                     const std::string& lang,
                     const content::VoiceData& voice,
                     const content::UtteranceContinuousParameters& params,
                     base::OnceCallback<void(bool)> on_speak_finished,
                     const std::string& parsed_utterance);

  friend struct base::DefaultSingletonTraits<TtsPlatformImplChromeOs>;

  std::string error_;

  base::WeakPtrFactory<TtsPlatformImplChromeOs> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(TtsPlatformImplChromeOs);
};

#endif  // CHROME_BROWSER_SPEECH_TTS_CHROMEOS_H_
