// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/speech/crosapi_tts_engine_delegate_ash.h"

#include "base/no_destructor.h"
#include "base/unguessable_token.h"
#include "chrome/browser/ash/crosapi/crosapi_ash.h"
#include "chrome/browser/ash/crosapi/crosapi_manager.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/speech/tts_ash.h"

// static
CrosapiTtsEngineDelegateAsh* CrosapiTtsEngineDelegateAsh::GetInstance() {
  static base::NoDestructor<CrosapiTtsEngineDelegateAsh> crosapi_tts_engine;
  return crosapi_tts_engine.get();
}

CrosapiTtsEngineDelegateAsh::CrosapiTtsEngineDelegateAsh() = default;

CrosapiTtsEngineDelegateAsh::~CrosapiTtsEngineDelegateAsh() = default;

void CrosapiTtsEngineDelegateAsh::GetVoices(
    content::BrowserContext* browser_context,
    std::vector<content::VoiceData>* out_voices) {
  bool is_primary_profile = ash::ProfileHelper::IsPrimaryProfile(
      Profile::FromBrowserContext(browser_context));
  // TODO(crbug.com/40792881): Support secondary profile when it becomes
  // available to Lacros.
  DCHECK(is_primary_profile);
  crosapi::TtsAsh* tts_ash =
      crosapi::CrosapiManager::Get()->crosapi_ash()->tts_ash();
  if (is_primary_profile && tts_ash->HasTtsClient()) {
    tts_ash->GetCrosapiVoices(tts_ash->GetPrimaryProfileBrowserContextId(),
                              out_voices);
  }
}

void CrosapiTtsEngineDelegateAsh::Speak(content::TtsUtterance* utterance,
                                        const content::VoiceData& voice) {
  DCHECK(voice.from_remote_tts_engine);
  crosapi::CrosapiManager::Get()
      ->crosapi_ash()
      ->tts_ash()
      ->SpeakWithLacrosVoice(utterance, voice);
}

void CrosapiTtsEngineDelegateAsh::Stop(content::TtsUtterance* utterance) {
  crosapi::CrosapiManager::Get()->crosapi_ash()->tts_ash()->StopRemoteEngine(
      utterance);
}

void CrosapiTtsEngineDelegateAsh::Pause(content::TtsUtterance* utterance) {
  crosapi::CrosapiManager::Get()->crosapi_ash()->tts_ash()->PauseRemoteEngine(
      utterance);
}

void CrosapiTtsEngineDelegateAsh::Resume(content::TtsUtterance* utterance) {
  crosapi::CrosapiManager::Get()->crosapi_ash()->tts_ash()->ResumeRemoteEngine(
      utterance);
}
