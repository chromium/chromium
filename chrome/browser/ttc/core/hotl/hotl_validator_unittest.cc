// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ttc/core/hotl/hotl_validator.h"

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/bind.h"
#include "base/test/gtest_util.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "chrome/browser/actor/actor_test_util.h"
#include "chrome/browser/ttc/core/features.h"
#include "chrome/browser/ttc/core/hotl/hotl_speech_recognizer.h"
#include "chrome/browser/ttc/core/session_journal_impl.h"
#include "chrome/browser/ttc/core/test_utils.h"
#include "chrome/grit/generated_resources.h"
#include "components/actor/core/aggregated_journal.h"
#include "components/actor/core/task_id.h"
#include "components/actor/public/mojom/actor_types.mojom.h"
#include "media/base/audio_parameters.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"

namespace ttc {

namespace {

using ::actor::ActorJournalRecorder;
using ::actor::mojom::JournalEntryType;
using ::testing::Contains;
using ::testing::ElementsAre;
using ::testing::Field;
using ::testing::IsEmpty;
using ::testing::NiceMock;
using ::testing::Pair;
using ::testing::SizeIs;

class HotlValidatorTest : public testing::Test {
 protected:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  actor::AggregatedJournal aggregated_journal_;
  ActorJournalRecorder recorder_{aggregated_journal_};
  SessionJournalImpl journal_{aggregated_journal_, actor::TaskId(1)};
};

TEST_F(HotlValidatorTest, ApproveResolvesApprovedAndJournalsAsyncSlice) {
  base::test::TestFuture<HotlValidator::Result> future;
  {
    HotlValidator validator(journal_, future.GetCallback());
    validator.Answer(/*approved=*/true);
    task_environment_.FastForwardBy(kHotlConfirmationTimeout.Get());
    EXPECT_EQ(future.Get(), HotlValidator::Result::kApproved);
  }

  const std::vector<ActorJournalRecorder::Entry> entries =
      recorder_.GetEntries("TtcHotlConfirmation");
  ASSERT_THAT(entries, SizeIs(2));
  EXPECT_EQ(entries[0].type, JournalEntryType::kBegin);
  EXPECT_THAT(
      entries[0].details,
      Contains(Pair("timeout_ms",
                    base::NumberToString(
                        kHotlConfirmationTimeout.Get().InMilliseconds()))));
  EXPECT_EQ(entries[1].type, JournalEntryType::kEnd);
  EXPECT_THAT(entries[1].details, Contains(Pair("outcome", "approved")));
  EXPECT_THAT(entries[1].details, Contains(Pair("source", "button")));
}

TEST_F(HotlValidatorTest, DenyResolvesDeniedAndJournalsAsyncSlice) {
  base::test::TestFuture<HotlValidator::Result> future;
  HotlValidator validator(journal_, future.GetCallback());

  validator.Answer(/*approved=*/false);

  EXPECT_EQ(future.Get(), HotlValidator::Result::kDenied);
  const std::vector<ActorJournalRecorder::Entry> entries =
      recorder_.GetEntries("TtcHotlConfirmation");
  ASSERT_THAT(entries, SizeIs(2));
  EXPECT_EQ(entries[1].type, JournalEntryType::kEnd);
  EXPECT_THAT(entries[1].details, Contains(Pair("outcome", "denied")));
  EXPECT_THAT(entries[1].details, Contains(Pair("source", "button")));
}

TEST_F(HotlValidatorTest, TimeoutResolvesTimedOutAndJournalsAsyncSlice) {
  base::test::TestFuture<HotlValidator::Result> future;
  HotlValidator validator(journal_, future.GetCallback());

  task_environment_.FastForwardBy(kHotlConfirmationTimeout.Get() -
                                  base::Milliseconds(1));
  EXPECT_FALSE(future.IsReady());
  task_environment_.FastForwardBy(base::Milliseconds(1));

  EXPECT_EQ(future.Get(), HotlValidator::Result::kTimedOut);
  const std::vector<ActorJournalRecorder::Entry> entries =
      recorder_.GetEntries("TtcHotlConfirmation");
  ASSERT_THAT(entries, SizeIs(2));
  EXPECT_EQ(entries[1].type, JournalEntryType::kEnd);
  EXPECT_THAT(entries[1].details, Contains(Pair("outcome", "timeout")));
  EXPECT_THAT(entries[1].details, Contains(Pair("source", "timeout")));
}

TEST_F(HotlValidatorTest, DestroyingWhilePendingDropsCallbackAndLogsCancelled) {
  base::test::TestFuture<HotlValidator::Result> future;
  auto validator =
      std::make_unique<HotlValidator>(journal_, future.GetCallback());

  validator.reset();
  task_environment_.FastForwardBy(kHotlConfirmationTimeout.Get());

  EXPECT_FALSE(future.IsReady());
  const std::vector<ActorJournalRecorder::Entry> entries =
      recorder_.GetEntries("TtcHotlConfirmation");
  ASSERT_THAT(entries, SizeIs(2));
  EXPECT_EQ(entries[0].type, JournalEntryType::kBegin);
  EXPECT_EQ(entries[1].type, JournalEntryType::kEnd);
  EXPECT_THAT(entries[1].details, Contains(Pair("outcome", "cancelled")));
}

TEST_F(HotlValidatorTest, CallbackCanDestroyValidatorOnAnswer) {
  std::unique_ptr<HotlValidator> validator;
  std::optional<HotlValidator::Result> observed;
  validator = std::make_unique<HotlValidator>(
      journal_, base::BindLambdaForTesting([&](HotlValidator::Result result) {
        validator.reset();
        observed = result;
      }));

  validator->Answer(/*approved=*/true);

  EXPECT_EQ(observed, HotlValidator::Result::kApproved);
  EXPECT_FALSE(validator);
}

TEST_F(HotlValidatorTest, CallbackCanDestroyValidatorOnTimeout) {
  std::unique_ptr<HotlValidator> validator;
  base::test::TestFuture<HotlValidator::Result> future;
  validator = std::make_unique<HotlValidator>(
      journal_, base::BindLambdaForTesting([&](HotlValidator::Result result) {
        validator.reset();
        future.SetValue(result);
      }));

  task_environment_.FastForwardBy(kHotlConfirmationTimeout.Get());

  EXPECT_EQ(future.Get(), HotlValidator::Result::kTimedOut);
  EXPECT_FALSE(validator);
}

TEST_F(HotlValidatorTest, CallbackCanReplaceValidator) {
  std::unique_ptr<HotlValidator> validator;
  base::test::TestFuture<HotlValidator::Result> second;
  validator = std::make_unique<HotlValidator>(
      journal_, base::BindLambdaForTesting([&](HotlValidator::Result result) {
        EXPECT_EQ(result, HotlValidator::Result::kDenied);
        validator =
            std::make_unique<HotlValidator>(journal_, second.GetCallback());
      }));

  validator->Answer(/*approved=*/false);
  ASSERT_TRUE(validator);
  validator->Answer(/*approved=*/true);

  EXPECT_EQ(second.Get(), HotlValidator::Result::kApproved);
}

TEST_F(HotlValidatorTest, AnswerAfterResolveCheckFails) {
  base::test::TestFuture<HotlValidator::Result> future;
  HotlValidator validator(journal_, future.GetCallback());
  validator.Answer(/*approved=*/true);

  EXPECT_CHECK_DEATH(validator.Answer(/*approved=*/false));
}

TEST_F(HotlValidatorTest, WithoutRecognizerDoesNotLogSpeechStarted) {
  base::test::TestFuture<HotlValidator::Result> future;
  HotlValidator validator(journal_, future.GetCallback());

  validator.Answer(/*approved=*/true);

  EXPECT_EQ(future.Get(), HotlValidator::Result::kApproved);
  EXPECT_THAT(recorder_.GetEntries("TtcHotlSpeechStarted"), IsEmpty());
}

class FakeHotlSpeechRecognizer : public HotlSpeechRecognizer {
 public:
  // HotlSpeechRecognizer:
  void Start(std::string_view locale,
             TranscriptCallback on_transcript,
             ErrorCallback on_error) override {
    locale_ = std::string(locale);
    on_transcript_ = std::move(on_transcript);
    on_error_ = std::move(on_error);
  }
  void Stop() override {
    on_transcript_.Reset();
    on_error_.Reset();
  }
  void PushAudioChunk(base::span<const int16_t> pcm_data,
                      const media::AudioParameters& params) override {
    pushed_samples_ += pcm_data.size();
  }

  // Like SodaHotlSpeechRecognizer, copies the callback first because the
  // handler may destroy `this`.
  void SendTranscript(std::u16string_view transcript) {
    TranscriptCallback callback = on_transcript_;
    callback.Run(transcript);
  }
  void Fail() {
    ErrorCallback callback = std::move(on_error_);
    Stop();
    std::move(callback).Run();
  }

  const std::string& locale() const { return locale_; }
  size_t pushed_samples() const { return pushed_samples_; }
  base::WeakPtr<FakeHotlSpeechRecognizer> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  std::string locale_;
  TranscriptCallback on_transcript_;
  ErrorCallback on_error_;
  size_t pushed_samples_ = 0;
  base::WeakPtrFactory<FakeHotlSpeechRecognizer> weak_ptr_factory_{this};
};

class HotlValidatorSpeechTest : public HotlValidatorTest {
 protected:
  HotlValidatorSpeechTest() {
    ON_CALL(conversation_, SetHotlAudioSink)
        .WillByDefault([this](Conversation::HotlAudioSinkCallback sink) {
          audio_sink_ = std::move(sink);
        });
  }

  std::unique_ptr<HotlSpeechRecognizer> CreateRecognizer() {
    auto recognizer = std::make_unique<FakeHotlSpeechRecognizer>();
    recognizer_ = recognizer->GetWeakPtr();
    return recognizer;
  }

  std::unique_ptr<HotlValidator> StartSpeechValidator(
      HotlValidator::ResultCallback callback) {
    return std::make_unique<HotlValidator>(
        journal_, std::move(callback),
        HotlValidator::RecognizerOptions(conversation_, CreateRecognizer(),
                                         "en-US"));
  }

  // Expects one resolution journaled with `outcome` and `source`.
  void ExpectResolved(std::string_view outcome, std::string_view source) {
    const std::vector<ActorJournalRecorder::Entry> entries =
        recorder_.GetEntries("TtcHotlConfirmation");
    ASSERT_THAT(entries, SizeIs(2));
    EXPECT_EQ(entries[0].type, JournalEntryType::kBegin);
    EXPECT_EQ(entries[1].type, JournalEntryType::kEnd);
    EXPECT_THAT(entries[1].details, Contains(Pair("outcome", outcome)));
    EXPECT_THAT(entries[1].details, Contains(Pair("source", source)));
  }

  // Expects the transcripts journaled so far to match `results`.
  void ExpectTranscriptResults(std::vector<std::string> results) {
    std::vector<std::string> actual;
    for (const ActorJournalRecorder::Entry& entry :
         recorder_.GetEntries("TtcHotlTranscriptMatched")) {
      actual.push_back(entry.details.at("result"));
    }
    EXPECT_EQ(actual, results);
  }

  NiceMock<MockConversation> conversation_;
  Conversation::HotlAudioSinkCallback audio_sink_;
  base::WeakPtr<FakeHotlSpeechRecognizer> recognizer_;
};

TEST_F(HotlValidatorSpeechTest, ConstructorListensAndRoutesAudio) {
  base::test::TestFuture<HotlValidator::Result> future;
  auto validator = StartSpeechValidator(future.GetCallback());

  ASSERT_TRUE(recognizer_);
  EXPECT_EQ(recognizer_->locale(), "en-US");
  ASSERT_TRUE(audio_sink_);
  const media::AudioParameters kParams(
      media::AudioParameters::AUDIO_PCM_LOW_LATENCY,
      media::ChannelLayoutConfig::Mono(), /*sample_rate=*/16000,
      /*frames_per_buffer=*/160);
  const int16_t samples[] = {1, 2, 3, 4};
  audio_sink_.Run(samples, kParams);
  EXPECT_EQ(recognizer_->pushed_samples(), 4u);
  EXPECT_THAT(recorder_.GetEntries("TtcHotlSpeechStarted"),
              ElementsAre(Field(&ActorJournalRecorder::Entry::details,
                                Contains(Pair("locale", "en-US")))));
}

TEST_F(HotlValidatorSpeechTest, SpokenYesApproves) {
  base::test::TestFuture<HotlValidator::Result> future;
  auto validator = StartSpeechValidator(future.GetCallback());

  recognizer_->SendTranscript(
      l10n_util::GetStringUTF16(IDS_TTC_CONFIRMATION_YES));

  EXPECT_EQ(future.Get(), HotlValidator::Result::kApproved);
  EXPECT_FALSE(recognizer_);
  EXPECT_FALSE(audio_sink_);
  ExpectTranscriptResults({"yes"});
  ExpectResolved("approved", "speech");
}

TEST_F(HotlValidatorSpeechTest, SpokenNoDenies) {
  base::test::TestFuture<HotlValidator::Result> future;
  auto validator = StartSpeechValidator(future.GetCallback());

  recognizer_->SendTranscript(
      l10n_util::GetStringUTF16(IDS_TTC_CONFIRMATION_NO));

  EXPECT_EQ(future.Get(), HotlValidator::Result::kDenied);
  EXPECT_FALSE(recognizer_);
  EXPECT_FALSE(audio_sink_);
  ExpectTranscriptResults({"no"});
  ExpectResolved("denied", "speech");
}

TEST_F(HotlValidatorSpeechTest, AmbiguousSpeechKeepsListening) {
  base::test::TestFuture<HotlValidator::Result> future;
  auto validator = StartSpeechValidator(future.GetCallback());

  recognizer_->SendTranscript(u"zebra 4921");

  EXPECT_FALSE(future.IsReady());
  ASSERT_TRUE(recognizer_);
  EXPECT_TRUE(audio_sink_);
  const std::vector<ActorJournalRecorder::Entry> matched =
      recorder_.GetEntries("TtcHotlTranscriptMatched");
  ASSERT_THAT(matched, SizeIs(1));
  EXPECT_THAT(matched[0].details, ElementsAre(Pair("result", "ambiguous")));

  recognizer_->SendTranscript(
      l10n_util::GetStringUTF16(IDS_TTC_CONFIRMATION_YES));
  EXPECT_EQ(future.Get(), HotlValidator::Result::kApproved);
  EXPECT_FALSE(audio_sink_);
  ExpectTranscriptResults({"ambiguous", "yes"});
  ExpectResolved("approved", "speech");
}

TEST_F(HotlValidatorSpeechTest, CallbackCanReplaceValidatorFromSpokenAnswer) {
  std::unique_ptr<HotlValidator> validator;
  base::test::TestFuture<HotlValidator::Result> second;
  validator = StartSpeechValidator(
      base::BindLambdaForTesting([&](HotlValidator::Result result) {
        EXPECT_EQ(result, HotlValidator::Result::kDenied);
        validator = StartSpeechValidator(second.GetCallback());
      }));
  base::WeakPtr<FakeHotlSpeechRecognizer> first = recognizer_;

  first->SendTranscript(l10n_util::GetStringUTF16(IDS_TTC_CONFIRMATION_NO));

  EXPECT_FALSE(first);
  ASSERT_TRUE(recognizer_);
  ASSERT_TRUE(audio_sink_);
  EXPECT_THAT(recorder_.GetEntries("TtcHotlSpeechStarted"), SizeIs(2));
  recognizer_->SendTranscript(
      l10n_util::GetStringUTF16(IDS_TTC_CONFIRMATION_YES));
  EXPECT_EQ(second.Get(), HotlValidator::Result::kApproved);
}

TEST_F(HotlValidatorSpeechTest, SpeechErrorKeepsConfirmationOpen) {
  base::test::TestFuture<HotlValidator::Result> future;
  auto validator = StartSpeechValidator(future.GetCallback());

  recognizer_->Fail();

  EXPECT_FALSE(future.IsReady());
  EXPECT_FALSE(recognizer_);
  EXPECT_FALSE(audio_sink_);
  EXPECT_THAT(recorder_.GetEntries("TtcHotlSpeechUnavailable"), SizeIs(1));
  task_environment_.FastForwardBy(kHotlConfirmationTimeout.Get());
  EXPECT_EQ(future.Get(), HotlValidator::Result::kTimedOut);
  ExpectResolved("timeout", "timeout");
}

TEST_F(HotlValidatorSpeechTest, TimeoutStopsSpeech) {
  base::test::TestFuture<HotlValidator::Result> future;
  auto validator = StartSpeechValidator(future.GetCallback());

  task_environment_.FastForwardBy(kHotlConfirmationTimeout.Get());

  EXPECT_EQ(future.Get(), HotlValidator::Result::kTimedOut);
  EXPECT_FALSE(recognizer_);
  EXPECT_FALSE(audio_sink_);
}

TEST_F(HotlValidatorSpeechTest, DestroyingWhilePendingClearsAudioSink) {
  base::test::TestFuture<HotlValidator::Result> future;
  auto validator = StartSpeechValidator(future.GetCallback());

  validator.reset();

  EXPECT_FALSE(recognizer_);
  EXPECT_FALSE(audio_sink_);
  EXPECT_FALSE(future.IsReady());
  const std::vector<ActorJournalRecorder::Entry> entries =
      recorder_.GetEntries("TtcHotlConfirmation");
  ASSERT_THAT(entries, SizeIs(2));
  EXPECT_EQ(entries[1].type, JournalEntryType::kEnd);
  EXPECT_THAT(entries[1].details, Contains(Pair("outcome", "cancelled")));
}

}  // namespace

}  // namespace ttc
