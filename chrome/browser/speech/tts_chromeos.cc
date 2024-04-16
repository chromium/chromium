// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/speech/tts_chromeos.h"

#include <algorithm>
#include <utility>

#include "ash/components/arc/mojom/tts.mojom.h"
#include "ash/components/arc/session/arc_bridge_service.h"
#include "ash/components/arc/session/arc_service_manager.h"
#include "base/no_destructor.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/common/extensions/extension_constants.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/tts_platform.h"

void TtsPlatformImplChromeOs::SetVoices(
    std::vector<content::VoiceData> voices) {
  std::sort(voices.begin(), voices.end(), [](const auto& v1, const auto& v2) {
    return !v1.remote && v2.remote;
  });
  voices_ = std::move(voices);
  received_word_event_ = false;
}

void TtsPlatformImplChromeOs::ReceivedWordEvent() {
  if (received_word_event_)
    return;

  received_word_event_ = true;
  for (auto& voice : voices_)
    voice.events.insert(content::TTS_EVENT_WORD);

  content::TtsController::GetInstance()->VoicesChanged();
}

TtsPlatformImplChromeOs::TtsPlatformImplChromeOs() = default;
TtsPlatformImplChromeOs::~TtsPlatformImplChromeOs() = default;

bool TtsPlatformImplChromeOs::PlatformImplSupported() {
  // TODO(crbug.com/40151186): Chrome OS Platform should support background
  // initialisation.
  return arc::ArcServiceManager::Get() && arc::ArcServiceManager::Get()
                                              ->arc_bridge_service()
                                              ->tts()
                                              ->IsConnected();
}

bool TtsPlatformImplChromeOs::PlatformImplInitialized() {
  // On Chrome OS, the extension-based voices are really the platform level
  // voices. ARC++ takes a while to load, so do not block TtsController from
  // processing and speaking utterances here.
  return true;
}

void TtsPlatformImplChromeOs::LoadBuiltInTtsEngine(
    content::BrowserContext* browser_context) {
  content::TtsEngineDelegate* tts_engine_delegate =
      content::TtsController::GetInstance()->GetTtsEngineDelegate();
  if (tts_engine_delegate)
    tts_engine_delegate->LoadBuiltInTtsEngine(browser_context);
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
                                base::Unretained(this), utterance_id, lang,
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
  int voice_id = 0;
  if (!voice.native_voice_identifier.empty() &&
      base::StringToInt(voice.native_voice_identifier, &voice_id)) {
    arc_utterance->voice_id = voice_id;
  }

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
  for (const auto& voice : voices_)
    out_voices->push_back(voice);
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

void TtsPlatformImplChromeOs::FinalizeVoiceOrdering(
    std::vector<content::VoiceData>& voices) {
  // Move all Espeak voices to the end.
  auto partition_point = std::stable_partition(
      voices.begin(), voices.end(), [](const content::VoiceData& voice) {
        return voice.engine_id !=
               extension_misc::kEspeakSpeechSynthesisExtensionId;
      });

  // Move all native voices to the end, before Espeak voices.
  std::stable_partition(
      voices.begin(), partition_point,
      [](const content::VoiceData& voice) { return !voice.native; });
}

void TtsPlatformImplChromeOs::RefreshVoices() {
  // Android voices can be updated silently.
  // If it happens, we can't return the latest voices here, but below
  // eventually calls TtsController::VoicesChanged.
  auto* const arc_service_manager = arc::ArcServiceManager::Get();
  if (!arc_service_manager)
    return;

  arc::mojom::TtsInstance* tts = ARC_GET_INSTANCE_FOR_METHOD(
      arc_service_manager->arc_bridge_service()->tts(), RefreshVoices);
  if (!tts)
    return;

  tts->RefreshVoices();
}

content::ExternalPlatformDelegate*
TtsPlatformImplChromeOs::GetExternalPlatformDelegate() {
  return nullptr;
}

// static
TtsPlatformImplChromeOs* TtsPlatformImplChromeOs::GetInstance() {
  static base::NoDestructor<TtsPlatformImplChromeOs> tts_platform;
  return tts_platform.get();
}
