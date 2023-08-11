// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/speech/tts_crosapi_util.h"

#include "base/feature_list.h"
#include "base/values.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "content/public/browser/tts_controller.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/constants/ash_features.h"
#include "chrome/browser/ash/crosapi/browser_util.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chromeos/startup/browser_params_proxy.h"
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

namespace tts_crosapi_util {

content::TtsEventType FromMojo(crosapi::mojom::TtsEventType mojo_event) {
  switch (mojo_event) {
    case crosapi::mojom::TtsEventType::kStart:
      return content::TtsEventType::TTS_EVENT_START;
    case crosapi::mojom::TtsEventType::kEnd:
      return content::TtsEventType::TTS_EVENT_END;
    case crosapi::mojom::TtsEventType::kWord:
      return content::TtsEventType::TTS_EVENT_WORD;
    case crosapi::mojom::TtsEventType::kSentence:
      return content::TtsEventType::TTS_EVENT_SENTENCE;
    case crosapi::mojom::TtsEventType::kMarker:
      return content::TtsEventType::TTS_EVENT_MARKER;
    case crosapi::mojom::TtsEventType::kInterrupted:
      return content::TtsEventType::TTS_EVENT_INTERRUPTED;
    case crosapi::mojom::TtsEventType::kCanceled:
      return content::TtsEventType::TTS_EVENT_CANCELLED;
    case crosapi::mojom::TtsEventType::kError:
      return content::TtsEventType::TTS_EVENT_ERROR;
    case crosapi::mojom::TtsEventType::kPause:
      return content::TtsEventType::TTS_EVENT_PAUSE;
    case crosapi::mojom::TtsEventType::kResume:
      return content::TtsEventType::TTS_EVENT_RESUME;
  }
}

crosapi::mojom::TtsEventType ToMojo(content::TtsEventType event_type) {
  switch (event_type) {
    case content::TtsEventType::TTS_EVENT_START:
      return crosapi::mojom::TtsEventType::kStart;
    case content::TtsEventType::TTS_EVENT_END:
      return crosapi::mojom::TtsEventType::kEnd;
    case content::TtsEventType::TTS_EVENT_WORD:
      return crosapi::mojom::TtsEventType::kWord;
    case content::TtsEventType::TTS_EVENT_SENTENCE:
      return crosapi::mojom::TtsEventType::kSentence;
    case content::TtsEventType::TTS_EVENT_MARKER:
      return crosapi::mojom::TtsEventType::kMarker;
    case content::TtsEventType::TTS_EVENT_INTERRUPTED:
      return crosapi::mojom::TtsEventType::kInterrupted;
    case content::TtsEventType::TTS_EVENT_CANCELLED:
      return crosapi::mojom::TtsEventType::kCanceled;
    case content::TtsEventType::TTS_EVENT_ERROR:
      return crosapi::mojom::TtsEventType::kError;
    case content::TtsEventType::TTS_EVENT_PAUSE:
      return crosapi::mojom::TtsEventType::kPause;
    case content::TtsEventType::TTS_EVENT_RESUME:
      return crosapi::mojom::TtsEventType::kResume;
  }
}

content::VoiceData FromMojo(const crosapi::mojom::TtsVoicePtr& mojo_voice) {
  content::VoiceData voice_data;
#if BUILDFLAG(IS_CHROMEOS_ASH)
  // The mojo_voice is from the remote TTS engine in Lacros.
  voice_data.from_remote_tts_engine = true;
#endif
  voice_data.name = mojo_voice->voice_name;
  voice_data.lang = mojo_voice->lang;
  voice_data.engine_id = mojo_voice->engine_id;
  voice_data.remote = mojo_voice->remote;
  voice_data.native = mojo_voice->native;
  voice_data.native_voice_identifier = mojo_voice->native_voice_identifier;

  for (const auto& mojo_event : mojo_voice->events)
    voice_data.events.insert(tts_crosapi_util::FromMojo(mojo_event));

  return voice_data;
}

crosapi::mojom::TtsVoicePtr ToMojo(const content::VoiceData& voice) {
  auto mojo_voice = crosapi::mojom::TtsVoice::New();
  mojo_voice->voice_name = voice.name;
  mojo_voice->lang = voice.lang;
  mojo_voice->remote = voice.remote;
  mojo_voice->engine_id = voice.engine_id;
  mojo_voice->native = voice.native;
  mojo_voice->native_voice_identifier = voice.native_voice_identifier;
  std::vector<crosapi::mojom::TtsEventType> mojo_events;
  for (const auto& event : voice.events) {
    mojo_events.push_back(tts_crosapi_util::ToMojo(event));
  }
  mojo_voice->events = std::move(mojo_events);

  return mojo_voice;
}

crosapi::mojom::TtsUtterancePtr ToMojo(content::TtsUtterance* utterance) {
  auto mojo_utterance = crosapi::mojom::TtsUtterance::New();
  mojo_utterance->utterance_id = utterance->GetId();
  mojo_utterance->text = utterance->GetText();
  mojo_utterance->lang = utterance->GetLang();
  mojo_utterance->voice_name = utterance->GetVoiceName();
  mojo_utterance->volume = utterance->GetContinuousParameters().volume;
  mojo_utterance->rate = utterance->GetContinuousParameters().rate;
  mojo_utterance->pitch = utterance->GetContinuousParameters().pitch;
  mojo_utterance->engine_id = utterance->GetEngineId();
  mojo_utterance->should_clear_queue = utterance->GetShouldClearQueue();
  mojo_utterance->src_id = utterance->GetSrcId();
  mojo_utterance->src_url = utterance->GetSrcUrl();

  for (const auto& event : utterance->GetDesiredEventTypes())
    mojo_utterance->desired_event_types.push_back(
        tts_crosapi_util::ToMojo(event));

  for (const auto& event : utterance->GetRequiredEventTypes())
    mojo_utterance->required_event_types.push_back(
        tts_crosapi_util::ToMojo(event));

  content::WebContents* web_contents = utterance->GetWebContents();
  mojo_utterance->was_created_with_web_contents = web_contents != nullptr;

  base::Value::Dict options = utterance->GetOptions()->Clone();
  mojo_utterance->options = std::move(options);

  return mojo_utterance;
}

std::unique_ptr<content::TtsUtterance> CreateUtteranceFromMojo(
    crosapi::mojom::TtsUtterancePtr& mojo_utterance,
    bool should_always_be_spoken) {
  // Construct TtsUtterance object.
  content::BrowserContext* browser_context =
      ProfileManager::GetPrimaryUserProfile();
  std::unique_ptr<content::TtsUtterance> utterance =
      content::TtsUtterance::Create(browser_context, should_always_be_spoken);

  utterance->SetText(mojo_utterance->text);
  utterance->SetLang(mojo_utterance->lang);
  utterance->SetVoiceName(mojo_utterance->voice_name);
  utterance->SetContinuousParameters(
      mojo_utterance->rate, mojo_utterance->pitch, mojo_utterance->volume);
  utterance->SetEngineId(mojo_utterance->engine_id);
  utterance->SetShouldClearQueue(mojo_utterance->should_clear_queue);
  utterance->SetSrcUrl(mojo_utterance->src_url);
  utterance->SetSrcId(mojo_utterance->src_id);

  std::set<content::TtsEventType> desired_events;
  for (const auto& mojo_event : mojo_utterance->desired_event_types)
    desired_events.insert(tts_crosapi_util::FromMojo(mojo_event));
  utterance->SetDesiredEventTypes(std::move(desired_events));

  std::set<content::TtsEventType> required_events;
  for (const auto& mojo_event : mojo_utterance->required_event_types)
    required_events.insert(tts_crosapi_util::FromMojo(mojo_event));
  utterance->SetRequiredEventTypes(std::move(required_events));

  base::Value::Dict options = mojo_utterance->options.Clone();
  utterance->SetOptions(std::move(options));
  return utterance;
}

bool ShouldEnableLacrosTtsSupport() {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  bool lacros_tts_support_enabled =
      crosapi::browser_util::IsLacrosEnabled() &&
      !base::FeatureList::IsEnabled(ash::features::kDisableLacrosTtsSupport);
  return lacros_tts_support_enabled;
#else  // IS_CHROMEOS_LACROS
  return chromeos::BrowserParamsProxy::Get()->EnableLacrosTtsSupport();
#endif
}

void GetAllVoicesForTesting(content::BrowserContext* browser_context,
                            const GURL& source_url,
                            std::vector<content::VoiceData>* out_voices) {
  content::TtsController::GetInstance()->GetVoices(
      ProfileManager::GetActiveUserProfile(), GURL(), out_voices);
}

void SpeakForTesting(std::unique_ptr<content::TtsUtterance> utterance) {
  content::TtsController::GetInstance()->SpeakOrEnqueue(std::move(utterance));
}

int GetTtsUtteranceQueueSizeForTesting() {
  return content::TtsController::GetInstance()->QueueSize();
}

}  // namespace tts_crosapi_util
