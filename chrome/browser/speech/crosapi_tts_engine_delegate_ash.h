// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SPEECH_CROSAPI_TTS_ENGINE_DELEGATE_ASH_H_
#define CHROME_BROWSER_SPEECH_CROSAPI_TTS_ENGINE_DELEGATE_ASH_H_

#include "content/public/browser/tts_controller.h"

// RemoteTtsEngineDelegate implementation that handles TTS requests to
// remote TTS engines living in Lacros.
class CrosapiTtsEngineDelegateAsh : public content::RemoteTtsEngineDelegate {
 public:
  static CrosapiTtsEngineDelegateAsh* GetInstance();
  CrosapiTtsEngineDelegateAsh();
  CrosapiTtsEngineDelegateAsh(const CrosapiTtsEngineDelegateAsh&) = delete;
  CrosapiTtsEngineDelegateAsh& operator=(const CrosapiTtsEngineDelegateAsh&) =
      delete;
  ~CrosapiTtsEngineDelegateAsh() override;

  // content::RemoteTtsEngineDelegate:
  void GetVoices(content::BrowserContext* browser_context,
                 std::vector<content::VoiceData>* out_voices) override;
  void Speak(content::TtsUtterance* utterance,
             const content::VoiceData& voice) override;
  void Stop(content::TtsUtterance* utterance) override;
  void Pause(content::TtsUtterance* utterance) override;
  void Resume(content::TtsUtterance* utterance) override;
};

#endif  // CHROME_BROWSER_SPEECH_CROSAPI_TTS_ENGINE_DELEGATE_ASH_H_
