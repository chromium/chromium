// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_READALOUD_AUDIO_GENERATION_SPEECH_SYNTHESIS_BROKER_H_
#define CHROME_BROWSER_READALOUD_AUDIO_GENERATION_SPEECH_SYNTHESIS_BROKER_H_

#include <string>
#include <string_view>

#include "base/i18n/language_tag.h"
#include "base/sequence_checker.h"
#include "components/optimization_guide/proto/features/read_aloud_synthesize.pb.h"

namespace readaloud {

// Manages voice model and language configuration for Read Aloud speech
// synthesis requests, constructing ReadAloudSynthesizeRequest protobufs.
//
// Lifetime & Ownership:
// Owned 1:1 by ReadAloudService on the UI thread. Created during
// ReadAloudService initialization and destroyed with the service.
class SpeechSynthesisBroker {
 public:
  static constexpr char kDefaultVoiceId[] = "en-US-Wavenet-A";
  static inline const base::i18n::LanguageTag kDefaultLanguageTag =
      base::i18n::GetKnownLanguageTag("en");

  SpeechSynthesisBroker();
  SpeechSynthesisBroker(const SpeechSynthesisBroker&) = delete;
  SpeechSynthesisBroker& operator=(const SpeechSynthesisBroker&) = delete;
  ~SpeechSynthesisBroker();

  // Configures the active voice model ID and target language tag.
  // Passing an empty string resets the respective value to its default
  // constant.
  void SetVoice(std::string_view voice_id);
  void SetLanguageCode(std::string_view language_code);

  const std::string& voice_id() const;
  const base::i18n::LanguageTag& language_tag() const;
  std::string_view language_code() const;

  // Constructs a ReadAloudSynthesizeRequest protobuf from the given text chunk
  // and active voice/language settings.
  optimization_guide::proto::ReadAloudSynthesizeRequest BuildSynthesizeRequest(
      std::u16string_view text_chunk) const;

 private:
  std::string voice_id_{kDefaultVoiceId};
  base::i18n::LanguageTag language_tag_{kDefaultLanguageTag};

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace readaloud

#endif  // CHROME_BROWSER_READALOUD_AUDIO_GENERATION_SPEECH_SYNTHESIS_BROKER_H_
