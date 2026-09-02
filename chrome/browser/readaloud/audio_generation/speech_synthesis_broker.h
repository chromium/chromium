// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_READALOUD_AUDIO_GENERATION_SPEECH_SYNTHESIS_BROKER_H_
#define CHROME_BROWSER_READALOUD_AUDIO_GENERATION_SPEECH_SYNTHESIS_BROKER_H_

#include <string>
#include <string_view>

#include "base/functional/callback.h"
#include "base/i18n/language_tag.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "components/optimization_guide/core/model_execution/remote_model_executor.h"
#include "components/optimization_guide/proto/features/read_aloud_synthesize.pb.h"
#include "mojo/public/cpp/base/big_buffer.h"

class OptimizationGuideKeyedService;

namespace readaloud {

// Manages voice model and language configuration for Read Aloud speech
// synthesis requests, constructing ReadAloudSynthesizeRequest protobufs.
//
// Lifetime & Ownership:
// Owned 1:1 by ReadAloudService on the UI thread. Created during
// ReadAloudService initialization and destroyed with the service.
class SpeechSynthesisBroker {
 public:
  static constexpr char kDefaultVoiceId[] = "msf00006";
  static inline const base::i18n::LanguageTag kDefaultLanguageTag =
      base::i18n::GetKnownLanguageTag("en");

  // Callback invoked when speech synthesis completes.
  // Receives raw response payload bytes (mojo_base::BigBuffer) and success
  // status.
  using SynthesizeSpeechCallback =
      base::OnceCallback<void(mojo_base::BigBuffer response_bytes,
                              bool success)>;

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

  // Issues an asynchronous speech synthesis request via
  // OptimizationGuideKeyedService. Enforces Rule of Two security boundary by
  // returning raw payload bytes in BigBuffer without deserializing
  // ReadAloudSynthesizeResponse in the Browser process.
  void SynthesizeSpeech(OptimizationGuideKeyedService* opt_guide_service,
                        std::u16string_view text_chunk,
                        SynthesizeSpeechCallback callback);

 private:
  void OnModelExecutionResult(
      SynthesizeSpeechCallback callback,
      optimization_guide::OptimizationGuideModelExecutionResult result,
      std::unique_ptr<optimization_guide::ModelQualityLogEntry> log_entry);

  std::string voice_id_{kDefaultVoiceId};
  base::i18n::LanguageTag language_tag_{kDefaultLanguageTag};

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<SpeechSynthesisBroker> weak_factory_{this};
};

}  // namespace readaloud

#endif  // CHROME_BROWSER_READALOUD_AUDIO_GENERATION_SPEECH_SYNTHESIS_BROKER_H_
