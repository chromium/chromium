// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TTC_CORE_HOTL_HOTL_VALIDATOR_H_
#define CHROME_BROWSER_TTC_CORE_HOTL_HOTL_VALIDATOR_H_

#include <stdint.h>

#include <memory>
#include <optional>
#include <string>
#include <string_view>

#include "base/containers/span.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/timer/timer.h"
#include "components/ttc/app/public/session_journal.h"

namespace media {
class AudioParameters;
}  // namespace media

namespace ttc {

class Conversation;
class HotlSpeechRecognizer;

// Resolves one Human-on-the-Loop (HOTL) confirmation. The confirmation starts
// on construction and ends when the user answers with a button or by voice, or
// when the timeout expires. Destroying the validator before it resolves cancels
// the timer, drops the pending callback without running it, and returns the
// microphone to the conversation.
class HotlValidator {
 public:
  enum class Result {
    kApproved,
    kDenied,
    kTimedOut,
  };
  using ResultCallback = base::OnceCallback<void(Result)>;

  // Configuration for accepting voice answers in addition to buttons and the
  // timeout. `recognizer` listens for `locale` (a non-empty BCP-47 tag matching
  // the language of IDS_TTC_CONFIRMATION_YES and IDS_TTC_CONFIRMATION_NO).
  // While voice answers are active, microphone audio from `conversation` routes
  // to `recognizer` instead of the model. `conversation` must outlive the
  // `HotlValidator`.
  struct RecognizerOptions {
    RecognizerOptions(Conversation& conversation,
                      std::unique_ptr<HotlSpeechRecognizer> recognizer,
                      std::string locale);
    RecognizerOptions(RecognizerOptions&&);
    RecognizerOptions& operator=(RecognizerOptions&&);
    ~RecognizerOptions();

    raw_ref<Conversation> conversation;
    std::unique_ptr<HotlSpeechRecognizer> recognizer;
    std::string locale;
  };

  // Starts a confirmation and runs `callback` once with its result. Accepts
  // button answers and the timeout, plus voice answers when
  // `recognizer_options` is set. `journal` must outlive `this`.
  HotlValidator(
      SessionJournal& journal,
      ResultCallback callback,
      std::optional<RecognizerOptions> recognizer_options = std::nullopt);

  HotlValidator(const HotlValidator&) = delete;
  HotlValidator& operator=(const HotlValidator&) = delete;
  ~HotlValidator();

  // Resolves the confirmation with the user's button answer. Must not be called
  // after the confirmation has already resolved.
  void Answer(bool approved);

 private:
  enum class Source {
    kButton,
    kSpeech,
    kTimeout,
  };

  static std::string_view ToString(Result result);
  static std::string_view ToString(Source source);

  void StartSpeech(std::unique_ptr<HotlSpeechRecognizer> recognizer,
                   std::string_view locale);
  // Destroys the recognizer and returns the microphone to the conversation.
  void StopSpeech();
  void OnCapturedAudio(base::span<const int16_t> pcm_data,
                       const media::AudioParameters& params);
  void OnTranscript(std::u16string_view transcript);
  void OnSpeechError();
  void OnTimeout();

  // Stops speech and the timer, ends the async journal slice, and runs
  // `callback_` with `result`. `callback_` may destroy `this`.
  void Resolve(Result result, Source source);

  const raw_ref<SessionJournal> journal_;
  std::unique_ptr<SessionJournal::PendingAsyncEvent> journal_event_;
  raw_ptr<Conversation> conversation_ = nullptr;
  ResultCallback callback_;
  base::OneShotTimer timeout_;
  // Non-null while voice answers are active, which is also while the
  // conversation's audio sink points to `this`.
  std::unique_ptr<HotlSpeechRecognizer> recognizer_;
};

}  // namespace ttc

#endif  // CHROME_BROWSER_TTC_CORE_HOTL_HOTL_VALIDATOR_H_
