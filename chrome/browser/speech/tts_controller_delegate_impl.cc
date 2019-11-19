// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/speech/tts_controller_delegate_impl.h"

#include <stddef.h>

#include <string>
#include <vector>

#include "base/containers/queue.h"
#include "base/json/json_reader.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/tts_controller.h"
#include "third_party/blink/public/mojom/speech/speech_synthesis.mojom.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

#if defined(OS_CHROMEOS)
bool VoiceIdMatches(const std::string& voice_id,
                    const content::VoiceData& voice) {
  if (voice_id.empty() || voice.name.empty() ||
      (voice.engine_id.empty() && !voice.native))
    return false;
  std::unique_ptr<base::DictionaryValue> json =
      base::DictionaryValue::From(base::JSONReader::ReadDeprecated(voice_id));
  std::string default_name;
  std::string default_extension_id;
  json->GetString("name", &default_name);
  json->GetString("extension", &default_extension_id);
  if (voice.native)
    return default_name == voice.name && default_extension_id.empty();
  return default_name == voice.name && default_extension_id == voice.engine_id;
}
#endif  // defined(OS_CHROMEOS)

}  // namespace

//
// TtsControllerDelegateImpl
//

// static
TtsControllerDelegateImpl* TtsControllerDelegateImpl::GetInstance() {
  return base::Singleton<TtsControllerDelegateImpl>::get();
}

TtsControllerDelegateImpl::TtsControllerDelegateImpl()
    : tts_engine_delegate_(nullptr) {}

TtsControllerDelegateImpl::~TtsControllerDelegateImpl() {
}

int TtsControllerDelegateImpl::GetMatchingVoice(
    content::TtsUtterance* utterance,
    std::vector<content::VoiceData>& voices) {
  // Return the index of the voice that best match the utterance parameters.
  //
  // These criteria are considered mandatory - if they're specified, any voice
  // that doesn't match is rejected.
  //
  //   Extension ID
  //   Voice name
  //
  // The other criteria are scored based on how well they match, in
  // this order of precedence:
  //
  //   Utterange language (exact region preferred, then general language code)
  //   App/system language (exact region preferred, then general language code)
  //   Required event types
  //   User-selected preference of voice given the general language code.

  // TODO(gaochun): Replace the global variable g_browser_process with
  // GetContentClient()->browser() to eliminate the dependency of browser
  // once TTS implementation was moved to content.
  std::string app_lang = g_browser_process->GetApplicationLocale();

#if defined(OS_CHROMEOS)
  const PrefService* prefs = GetPrefService(utterance);
  const base::DictionaryValue* lang_to_voice_pref;
  if (prefs) {
    lang_to_voice_pref =
        prefs->GetDictionary(prefs::kTextToSpeechLangToVoiceName);
  }
#endif  // defined(OS_CHROMEOS)

  // Start with a best score of -1, that way even if none of the criteria
  // match, something will be returned if there are any voices.
  int best_score = -1;
  int best_score_index = -1;
  for (size_t i = 0; i < voices.size(); ++i) {
    const content::VoiceData& voice = voices[i];
    int score = 0;

    // If the extension ID is specified, check for an exact match.
    if (!utterance->GetEngineId().empty() &&
        utterance->GetEngineId() != voice.engine_id)
      continue;

    // If the voice name is specified, check for an exact match.
    if (!utterance->GetVoiceName().empty() &&
        voice.name != utterance->GetVoiceName())
      continue;

    // Prefer the utterance language.
    if (!voice.lang.empty() && !utterance->GetLang().empty()) {
      // An exact language match is worth more than a partial match.
      if (voice.lang == utterance->GetLang()) {
        score += 128;
      } else if (l10n_util::GetLanguage(voice.lang) ==
                 l10n_util::GetLanguage(utterance->GetLang())) {
        score += 64;
      }
    }

    // Next, prefer required event types.
    if (!utterance->GetRequiredEventTypes().empty()) {
      bool has_all_required_event_types = true;
      for (auto iter = utterance->GetRequiredEventTypes().begin();
           iter != utterance->GetRequiredEventTypes().end(); ++iter) {
        if (voice.events.find(*iter) == voice.events.end()) {
          has_all_required_event_types = false;
          break;
        }
      }
      if (has_all_required_event_types)
        score += 32;
    }

#if defined(OS_CHROMEOS)
    // Prefer the user's preference voice for the language:
    if (lang_to_voice_pref) {
      // First prefer the user's preference voice for the utterance language,
      // if the utterance language is specified.
      std::string voice_id;
      if (!utterance->GetLang().empty()) {
        lang_to_voice_pref->GetString(
            l10n_util::GetLanguage(utterance->GetLang()), &voice_id);
        if (VoiceIdMatches(voice_id, voice))
          score += 16;
      }

      // Then prefer the user's preference voice for the system language.
      // This is a lower priority match than the utterance voice.
      voice_id.clear();
      lang_to_voice_pref->GetString(l10n_util::GetLanguage(app_lang),
                                    &voice_id);
      if (VoiceIdMatches(voice_id, voice))
        score += 8;

      // Finally, prefer the user's preference voice for any language. This will
      // pick the default voice if there is no better match for the current
      // system language and utterance language.
      voice_id.clear();
      lang_to_voice_pref->GetString("noLanguageCode", &voice_id);
      if (VoiceIdMatches(voice_id, voice))
        score += 4;
    }
#endif  // defined(OS_CHROMEOS)

    // Finally, prefer system language.
    if (!voice.lang.empty()) {
      if (voice.lang == app_lang) {
        score += 2;
      } else if (l10n_util::GetLanguage(voice.lang) ==
                 l10n_util::GetLanguage(app_lang)) {
        score += 1;
      }
    }

    if (score > best_score) {
      best_score = score;
      best_score_index = i;
    }
  }

  return best_score_index;
}

void TtsControllerDelegateImpl::UpdateUtteranceDefaultsFromPrefs(
    content::TtsUtterance* utterance,
    double* rate,
    double* pitch,
    double* volume) {
#if defined(OS_CHROMEOS)
  // Update pitch, rate and volume from user prefs if not set explicitly
  // on this utterance.
  const PrefService* prefs = GetPrefService(utterance);
  if (*rate == blink::mojom::kSpeechSynthesisDoublePrefNotSet) {
    *rate = prefs ? prefs->GetDouble(prefs::kTextToSpeechRate)
                  : blink::mojom::kSpeechSynthesisDefaultRate;
  }
  if (*pitch == blink::mojom::kSpeechSynthesisDoublePrefNotSet) {
    *pitch = prefs ? prefs->GetDouble(prefs::kTextToSpeechPitch)
                   : blink::mojom::kSpeechSynthesisDefaultPitch;
  }
  if (*volume == blink::mojom::kSpeechSynthesisDoublePrefNotSet) {
    *volume = prefs ? prefs->GetDouble(prefs::kTextToSpeechVolume)
                    : blink::mojom::kSpeechSynthesisDefaultVolume;
  }
#endif  // defined(OS_CHROMEOS)
}

const PrefService* TtsControllerDelegateImpl::GetPrefService(
    content::TtsUtterance* utterance) {
  const PrefService* prefs = nullptr;
  // The utterance->GetBrowserContext() is null in tests.
  if (utterance->GetBrowserContext()) {
    const Profile* profile =
        Profile::FromBrowserContext(utterance->GetBrowserContext());
    if (profile)
      prefs = profile->GetPrefs();
  }
  return prefs;
}

void TtsControllerDelegateImpl::SetTtsEngineDelegate(
    content::TtsEngineDelegate* delegate) {
  tts_engine_delegate_ = delegate;
}

content::TtsEngineDelegate* TtsControllerDelegateImpl::GetTtsEngineDelegate() {
  return tts_engine_delegate_;
}
