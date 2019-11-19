// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/speech/tts_chromeos.h"

#include "base/macros.h"
#include "components/arc/arc_service_manager.h"
#include "components/arc/mojom/tts.mojom.h"
#include "components/arc/session/arc_bridge_service.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/tts_platform.h"

TtsPlatformImplChromeOs::TtsPlatformImplChromeOs() {}
TtsPlatformImplChromeOs::~TtsPlatformImplChromeOs() {}

bool TtsPlatformImplChromeOs::PlatformImplAvailable() {
  return arc::ArcServiceManager::Get() && arc::ArcServiceManager::Get()
                                              ->arc_bridge_service()
                                              ->tts()
                                              ->IsConnected();
}

bool TtsPlatformImplChromeOs::LoadBuiltInTtsEngine(
    content::BrowserContext* browser_context) {
  content::TtsEngineDelegate* tts_engine_delegate =
      content::TtsController::GetInstance()->GetTtsEngineDelegate();
  if (tts_engine_delegate)
    return tts_engine_delegate->LoadBuiltInTtsEngine(browser_context);
  return false;
}

void TtsPlatformImplChromeOs::Speak(
    int utterance_id,
    const std::string& utterance,
    const std::string& lang,
    const content::VoiceData& voice,
    const content::UtteranceContinuousParameters& params,
    base::OnceCallback<void(bool)> on_speak_finished) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // Parse SSML and process speech.
  content::TtsController::GetInstance()->StripSSML(
      utterance, base::BindOnce(&TtsPlatformImplChromeOs::ProcessSpeech,
                                weak_factory_.GetWeakPtr(), utterance_id, lang,
                                voice, params, std::move(on_speak_finished)));
}

void TtsPlatformImplChromeOs::ProcessSpeech(
    int utterance_id,
    const std::string& lang,
    const content::VoiceData& voice,
    const content::UtteranceContinuousParameters& params,
    base::OnceCallback<void(bool)> on_speak_finished,
    const std::string& parsed_utterance) {
  auto* const arc_service_manager = arc::ArcServiceManager::Get();
  if (!arc_service_manager) {
    std::move(on_speak_finished).Run(false);
    return;
  }
  arc::mojom::TtsInstance* tts = ARC_GET_INSTANCE_FOR_METHOD(
      arc_service_manager->arc_bridge_service()->tts(), Speak);
  if (!tts) {
    std::move(on_speak_finished).Run(false);
    return;
  }

  arc::mojom::TtsUtterancePtr arc_utterance = arc::mojom::TtsUtterance::New();
  arc_utterance->utteranceId = utterance_id;
  arc_utterance->text = parsed_utterance;
  arc_utterance->rate = params.rate;
  arc_utterance->pitch = params.pitch;
  tts->Speak(std::move(arc_utterance));
  std::move(on_speak_finished).Run(true);
}

bool TtsPlatformImplChromeOs::StopSpeaking() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  auto* const arc_service_manager = arc::ArcServiceManager::Get();
  if (!arc_service_manager)
    return false;
  arc::mojom::TtsInstance* tts = ARC_GET_INSTANCE_FOR_METHOD(
      arc_service_manager->arc_bridge_service()->tts(), Stop);
  if (!tts)
    return false;

  tts->Stop();
  return true;
}

void TtsPlatformImplChromeOs::GetVoices(
    std::vector<content::VoiceData>* out_voices) {
  out_voices->push_back(content::VoiceData());
  content::VoiceData& voice = out_voices->back();
  voice.native = true;
  voice.name = "Android";
  voice.events.insert(content::TTS_EVENT_START);
  voice.events.insert(content::TTS_EVENT_END);
}

std::string TtsPlatformImplChromeOs::GetError() {
  return error_;
}

void TtsPlatformImplChromeOs::ClearError() {
  error_ = std::string();
}

void TtsPlatformImplChromeOs::SetError(const std::string& error) {
  error_ = error;
}

bool TtsPlatformImplChromeOs::IsSpeaking() {
  return false;
}

// static
TtsPlatformImplChromeOs*
TtsPlatformImplChromeOs::GetInstance() {
  return base::Singleton<TtsPlatformImplChromeOs>::get();
}
