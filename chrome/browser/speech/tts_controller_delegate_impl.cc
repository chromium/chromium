// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/speech/tts_controller_delegate_impl.h"

#include <stddef.h>

#include <string>

#include "base/json/json_reader.h"
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

std::optional<content::TtsControllerDelegate::PreferredVoiceId>
PreferredVoiceIdFromString(const base::Value::Dict& pref,
                           const std::string& pref_key) {
  const std::string* voice_id =
      pref.FindStringByDottedPath(l10n_util::GetLanguage(pref_key));
  if (!voice_id || voice_id->empty())
    return std::nullopt;

  std::optional<base::Value> json = base::JSONReader::Read(*voice_id);
  std::string name;
  std::string id;
  if (json && json->is_dict()) {
    const base::Value::Dict& dict = json->GetDict();
    const std::string* name_str = dict.FindString("name");
    if (name_str)
      name = *name_str;
    const std::string* id_str = dict.FindString("extension");
    if (id_str)
      id = *id_str;
  }

  return std::optional<content::TtsControllerDelegate::PreferredVoiceId>(
      {name, id});
}

}  // namespace

//
// TtsControllerDelegateImpl
//

// static
TtsControllerDelegateImpl* TtsControllerDelegateImpl::GetInstance() {
  return base::Singleton<TtsControllerDelegateImpl>::get();
}

TtsControllerDelegateImpl::TtsControllerDelegateImpl() = default;

TtsControllerDelegateImpl::~TtsControllerDelegateImpl() = default;

std::unique_ptr<content::TtsControllerDelegate::PreferredVoiceIds>
TtsControllerDelegateImpl::GetPreferredVoiceIdsForUtterance(
    content::TtsUtterance* utterance) {
  const base::Value::Dict* lang_to_voice_pref = GetLangToVoicePref(utterance);
  if (!lang_to_voice_pref)
    return nullptr;

  std::unique_ptr<PreferredVoiceIds> preferred_ids =
      std::make_unique<PreferredVoiceIds>();

  if (!utterance->GetLang().empty()) {
    preferred_ids->lang_voice_id = PreferredVoiceIdFromString(
        *lang_to_voice_pref, l10n_util::GetLanguage(utterance->GetLang()));
  }

  const std::string app_lang = g_browser_process->GetApplicationLocale();
  preferred_ids->locale_voice_id = PreferredVoiceIdFromString(
      *lang_to_voice_pref, l10n_util::GetLanguage(app_lang));

  preferred_ids->any_locale_voice_id =
      PreferredVoiceIdFromString(*lang_to_voice_pref, "noLanguageCode");
  return preferred_ids;
}

void TtsControllerDelegateImpl::UpdateUtteranceDefaultsFromPrefs(
    content::TtsUtterance* utterance,
    double* rate,
    double* pitch,
    double* volume) {
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
}

const PrefService* TtsControllerDelegateImpl::GetPrefService(
    content::TtsUtterance* utterance) {
  // The utterance->GetBrowserContext() is null in tests.
  if (!utterance->GetBrowserContext())
    return nullptr;

  const Profile* profile =
      Profile::FromBrowserContext(utterance->GetBrowserContext());
  return profile ? profile->GetPrefs() : nullptr;
}

const base::Value::Dict* TtsControllerDelegateImpl::GetLangToVoicePref(
    content::TtsUtterance* utterance) {
  const PrefService* prefs = GetPrefService(utterance);
  return prefs == nullptr
             ? nullptr
             : &prefs->GetDict(prefs::kTextToSpeechLangToVoiceName);
}
