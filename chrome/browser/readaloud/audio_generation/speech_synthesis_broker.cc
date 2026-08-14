// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/readaloud/audio_generation/speech_synthesis_broker.h"

#include "base/i18n/tag_converters.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"

namespace readaloud {

SpeechSynthesisBroker::SpeechSynthesisBroker() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

SpeechSynthesisBroker::~SpeechSynthesisBroker() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void SpeechSynthesisBroker::SetVoice(std::string_view voice_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (voice_id.empty()) {
    voice_id_ = kDefaultVoiceId;
  } else {
    voice_id_ = std::string(voice_id);
  }
}

void SpeechSynthesisBroker::SetLanguageCode(std::string_view language_code) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (language_code.empty()) {
    std::string default_locale =
        g_browser_process ? g_browser_process->GetApplicationLocale() : "en";
    std::optional<base::i18n::LanguageTag> parsed =
        base::i18n::GetLanguageTagFromString(default_locale);
    language_tag_ = parsed ? *parsed : kDefaultLanguageTag;
    return;
  }
  std::optional<base::i18n::LanguageTag> parsed =
      base::i18n::GetLanguageTagFromString(language_code);
  if (parsed) {
    language_tag_ = *parsed;
  } else {
    language_tag_ = kDefaultLanguageTag;
  }
}

const std::string& SpeechSynthesisBroker::voice_id() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return voice_id_;
}

const base::i18n::LanguageTag& SpeechSynthesisBroker::language_tag() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return language_tag_;
}

std::string_view SpeechSynthesisBroker::language_code() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return language_tag_.tag_string();
}

optimization_guide::proto::ReadAloudSynthesizeRequest
SpeechSynthesisBroker::BuildSynthesizeRequest(
    std::u16string_view text_chunk) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  optimization_guide::proto::ReadAloudSynthesizeRequest request;
  request.set_text_chunk(base::UTF16ToUTF8(text_chunk));
  request.set_voice_id(voice_id_);
  request.set_language_code(std::string(language_tag_.tag_string()));
  return request;
}

}  // namespace readaloud
