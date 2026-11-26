// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ttc/core/hotl/hotl_validator.h"

#include <utility>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/time/time.h"
#include "chrome/browser/ttc/core/features.h"
#include "chrome/browser/ttc/core/hotl/hotl_speech_recognizer.h"
#include "chrome/browser/ttc/core/hotl/hotl_transcript_matcher.h"
#include "components/actor/core/journal_details_builder.h"
#include "components/ttc/app/public/conversation.h"

namespace ttc {

namespace {

std::string_view MatchResultToString(HotlMatchResult result) {
  switch (result) {
    case HotlMatchResult::kYes:
      return "yes";
    case HotlMatchResult::kNo:
      return "no";
    case HotlMatchResult::kAmbiguous:
      return "ambiguous";
  }
}

}  // namespace

// static
std::string_view HotlValidator::ToString(Result result) {
  switch (result) {
    case Result::kApproved:
      return "approved";
    case Result::kDenied:
      return "denied";
    case Result::kTimedOut:
      return "timeout";
  }
}

// static
std::string_view HotlValidator::ToString(Source source) {
  switch (source) {
    case Source::kButton:
      return "button";
    case Source::kSpeech:
      return "speech";
    case Source::kTimeout:
      return "timeout";
  }
}

HotlValidator::RecognizerOptions::RecognizerOptions(
    Conversation& conversation,
    std::unique_ptr<HotlSpeechRecognizer> recognizer,
    std::string locale)
    : conversation(conversation),
      recognizer(std::move(recognizer)),
      locale(std::move(locale)) {}

HotlValidator::RecognizerOptions::RecognizerOptions(RecognizerOptions&&) =
    default;
HotlValidator::RecognizerOptions& HotlValidator::RecognizerOptions::operator=(
    RecognizerOptions&&) = default;
HotlValidator::RecognizerOptions::~RecognizerOptions() = default;

HotlValidator::HotlValidator(
    SessionJournal& journal,
    ResultCallback callback,
    std::optional<RecognizerOptions> recognizer_options)
    : journal_(journal), callback_(std::move(callback)) {
  CHECK(callback_);
  const base::TimeDelta timeout = kHotlConfirmationTimeout.Get();
  journal_event_ = journal_->BeginAsyncEvent(
      "TtcHotlConfirmation", actor::JournalDetailsBuilder()
                                 .Add("timeout_ms", timeout.InMilliseconds())
                                 .Build());
  CHECK(journal_event_);
  timeout_.Start(
      FROM_HERE, timeout,
      base::BindOnce(&HotlValidator::OnTimeout, base::Unretained(this)));
  if (recognizer_options.has_value()) {
    CHECK(recognizer_options->recognizer);
    CHECK(!recognizer_options->locale.empty());
    conversation_ = &recognizer_options->conversation.get();
    StartSpeech(std::move(recognizer_options->recognizer),
                recognizer_options->locale);
  }
}

HotlValidator::~HotlValidator() {
  StopSpeech();
  if (journal_event_) {
    journal_event_->EndEntry(
        actor::JournalDetailsBuilder().Add("outcome", "cancelled").Build());
  }
}

void HotlValidator::Answer(bool approved) {
  CHECK(callback_);
  Resolve(approved ? Result::kApproved : Result::kDenied, Source::kButton);
}

void HotlValidator::StartSpeech(
    std::unique_ptr<HotlSpeechRecognizer> recognizer,
    std::string_view locale) {
  recognizer_ = std::move(recognizer);
  journal_->Log("TtcHotlSpeechStarted",
                actor::JournalDetailsBuilder().Add("locale", locale).Build());
  // `recognizer_` drops its callbacks when it stops or is destroyed.
  recognizer_->Start(
      locale,
      base::BindRepeating(&HotlValidator::OnTranscript, base::Unretained(this)),
      base::BindOnce(&HotlValidator::OnSpeechError, base::Unretained(this)));
  // StopSpeech() clears the sink before `this` is destroyed.
  conversation_->SetHotlAudioSink(base::BindRepeating(
      &HotlValidator::OnCapturedAudio, base::Unretained(this)));
}

void HotlValidator::StopSpeech() {
  if (!recognizer_) {
    return;
  }
  conversation_->SetHotlAudioSink(base::NullCallback());
  conversation_ = nullptr;
  recognizer_.reset();
}

void HotlValidator::OnCapturedAudio(base::span<const int16_t> pcm_data,
                                    const media::AudioParameters& params) {
  recognizer_->PushAudioChunk(pcm_data, params);
}

void HotlValidator::OnTranscript(std::u16string_view transcript) {
  const HotlMatchResult result = MatchHotlTranscript(transcript);
  // Do not journal `transcript`: it can hold unrelated private speech.
  journal_->Log("TtcHotlTranscriptMatched",
                actor::JournalDetailsBuilder()
                    .Add("result", MatchResultToString(result))
                    .Build());
  switch (result) {
    case HotlMatchResult::kYes:
      Resolve(Result::kApproved, Source::kSpeech);
      return;
    case HotlMatchResult::kNo:
      Resolve(Result::kDenied, Source::kSpeech);
      return;
    case HotlMatchResult::kAmbiguous:
      // The user may say other words before the answer. Keep listening.
      return;
  }
}

void HotlValidator::OnSpeechError() {
  journal_->Log("TtcHotlSpeechUnavailable", {});
  // Buttons and the timeout still resolve the confirmation.
  StopSpeech();
}

void HotlValidator::OnTimeout() {
  Resolve(Result::kTimedOut, Source::kTimeout);
}

void HotlValidator::Resolve(Result result, Source source) {
  CHECK(callback_);
  StopSpeech();
  timeout_.Stop();
  journal_event_->EndEntry(actor::JournalDetailsBuilder()
                               .Add("outcome", ToString(result))
                               .Add("source", ToString(source))
                               .Build());
  journal_event_.reset();
  ResultCallback callback = std::move(callback_);
  // Run the callback last because it may destroy `this`.
  std::move(callback).Run(result);
}

}  // namespace ttc
